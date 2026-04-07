#include "backend/BroadcastDiscovery.h"

#include "standalone/Log.h"
#include "standalone/wrap_endian.h"
#include "standalone/wrap_socket.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <iphlpapi.h>
#include <winsock2.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

#if !defined(_WIN32)
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace
{
    // UDP SERVERINFO 广播发现协议常量。
    // 请求: service=0x00000001, 响应: service=0x80000001。
    //
    // 协议示意（均为小端序）:
    //
    // 1) 请求报文（UDP payload）
    // +0   uint32 cookie      = 0x71146603
    // +4   uint32 invokeId    = 0
    // +8   uint32 serviceId   = 0x00000001 (SERVERINFO)
    // +12  AmsAddr requester  = {netId=0.0.0.0.0.0, port=0}
    // +20  uint32 tagCount    = 0
    //
    // 2) 响应报文（UDP payload）
    // +0   uint32 cookie      = 0x71146603
    // +4   uint32 invokeId    = 0
    // +8   uint32 serviceId   = 0x80000001 (RESPONSE|SERVERINFO)
    // +12  AmsAddr responder  = {netId, port}
    // +20  uint32 tagCount    = N
    // +24  TLV[tagCount]
    //      TLV = uint16 tagId + uint16 len + uint8 data[len]
    //
    // 常见标签:
    // - 0x0005: deviceName
    // - 0x0004: serviceText 或平台标识
    // - 0x0003: runtimeVersion
    // - 0x0012: fingerprint
    // - 0x0014: platformId（部分设备）
    static constexpr uint32_t kUdpCookie = 0x71146603;
    static constexpr uint32_t kUdpInvokeId = 0;
    static constexpr uint32_t kServerInfoRequest = 1;
    static constexpr uint32_t kServerInfoResponse = 0x80000001;

    uint16_t ReadLe16(const uint8_t *p)
    {
        uint16_t value;
        std::memcpy(&value, p, sizeof(value));
        return bhf::ads::letoh(value);
    }

    uint32_t ReadLe32(const uint8_t *p)
    {
        uint32_t value;
        std::memcpy(&value, p, sizeof(value));
        return bhf::ads::letoh(value);
    }

    void CopyTextField(char *dst, size_t dstSize, const uint8_t *src, size_t srcLen)
    {
        // 安全复制 C 字符串风格文本，自动截断并补 '\0'。
        if (!dst || dstSize == 0)
        {
            return;
        }

        size_t writeLen = 0;
        while (writeLen < srcLen && writeLen + 1 < dstSize)
        {
            if (src[writeLen] == '\0')
            {
                break;
            }
            dst[writeLen] = static_cast<char>(src[writeLen]);
            ++writeLen;
        }
        dst[writeLen] = '\0';
    }

    void CopyUtf16LeAsciiField(char *dst, size_t dstSize, const uint8_t *src, size_t srcLen)
    {
        // 目标设备某些标签会返回 UTF-16LE 文本，这里按低字节抽取为可读 ASCII。
        if (!dst || dstSize == 0)
        {
            return;
        }

        size_t writeLen = 0;
        for (size_t i = 0; i + 1 < srcLen && writeLen + 1 < dstSize; i += 2)
        {
            const uint8_t lo = src[i];
            const uint8_t hi = src[i + 1];
            if (lo == '\0' && hi == '\0')
            {
                break;
            }
            dst[writeLen++] = static_cast<char>(lo);
        }
        dst[writeLen] = '\0';
    }

    bool IsMostlyPrintableAscii(const char *text)
    {
        if (!text || text[0] == '\0')
        {
            return false;
        }

        size_t total = 0;
        size_t printable = 0;
        for (const char *p = text; *p != '\0'; ++p)
        {
            const unsigned char c = static_cast<unsigned char>(*p);
            ++total;
            if (std::isprint(c) || std::isspace(c))
            {
                ++printable;
            }
        }
        return total > 0 && printable == total;
    }

    void CopyBinaryAsHexField(char *dst, size_t dstSize, const uint8_t *src, size_t srcLen)
    {
        if (!dst || dstSize == 0)
        {
            return;
        }

        dst[0] = '\0';
        if (!src || srcLen == 0)
        {
            return;
        }

        const size_t maxBytes = (std::min)(srcLen, static_cast<size_t>(8));
        size_t write = 0;
        for (size_t i = 0; i < maxBytes && write + 4 < dstSize; ++i)
        {
            const int n = std::snprintf(dst + write, dstSize - write, (i == 0) ? "0x%02X" : " %02X", src[i]);
            if (n <= 0)
            {
                break;
            }
            write += static_cast<size_t>(n);
        }
    }

    const char *GetPlatformName(uint32_t platformId)
    {
        switch (platformId)
        {
        case 30u:
            return "Windows XP/CE";
        case 40u:
            return "Windows 7";
        case 50u:
            return "Windows 10";
        case 60u:
            return "Windows 11/Server 2022";
        case 100u:
            return "Linux";
        default:
            return nullptr;
        }
    }

    bool TryReadBuildNumber(const uint8_t *src, size_t len, uint32_t *outBuild)
    {
        if (!src || !outBuild)
        {
            return false;
        }

        // 常见场景: 4 字节小端 build（如 19045、26100）。
        if (len >= 4)
        {
            const uint32_t v = ReadLe32(src);
            if (v >= 1000u && v <= 200000u)
            {
                *outBuild = v;
                return true;
            }
        }

        // 回退: 2 字节版本号（较少见，仅在合理范围内接受）。
        if (len >= 2)
        {
            const uint32_t v = static_cast<uint32_t>(ReadLe16(src));
            if (v >= 100u && v <= 65535u)
            {
                *outBuild = v;
                return true;
            }
        }

        return false;
    }

    const char *GetWindowsNameByVersion(uint32_t major, uint32_t minor)
    {
        if (major == 10u)
        {
            return "Windows 10";
        }
        if (major == 6u && minor == 3u)
        {
            return "Windows 8.1";
        }
        if (major == 6u && minor == 2u)
        {
            return "Windows 8";
        }
        if (major == 6u && minor == 1u)
        {
            return "Windows 7";
        }
        if (major == 6u && minor == 0u)
        {
            return "Windows Vista";
        }
        if (major == 5u && minor == 1u)
        {
            return "Windows XP";
        }
        return nullptr;
    }

    bool TryParseOsVersionInfoTag(const uint8_t *src,
                                  size_t len,
                                  char *osText,
                                  size_t osTextSize,
                                  char *serviceText,
                                  size_t serviceTextSize,
                                  uint32_t *outBuild)
    {
        // 解析 Win32 OSVERSIONINFOEX 风格结构：
        // DWORD size(常见 0x114), major, minor, build, platform, WCHAR csd[128]
        if (!src || len < 20 || !osText || osTextSize == 0)
        {
            return false;
        }

        const uint32_t structSize = ReadLe32(src + 0);
        const uint32_t major = ReadLe32(src + 4);
        const uint32_t minor = ReadLe32(src + 8);
        const uint32_t build = ReadLe32(src + 12);
        const uint32_t platform = ReadLe32(src + 16);

        if (structSize < 20u || structSize > len || platform > 3u)
        {
            return false;
        }

        const char *winName = GetWindowsNameByVersion(major, minor);
        if (!winName)
        {
            return false;
        }

        if (build > 0)
        {
            std::snprintf(osText, osTextSize, "%s(%u)", winName, build);
            if (outBuild)
            {
                *outBuild = build;
            }
        }
        else
        {
            std::snprintf(osText, osTextSize, "%s", winName);
        }

        if (serviceText && serviceTextSize > 0 && len > 20)
        {
            const size_t csdLen = (std::min)(len - 20, static_cast<size_t>(256));
            CopyUtf16LeAsciiField(serviceText, serviceTextSize, src + 20, csdLen);
        }

        return true;
    }

    void ComposeOsVersionText(char *dst,
                              size_t dstSize,
                              uint32_t platformId,
                              bool hasBuild,
                              uint32_t buildNumber)
    {
        if (!dst || dstSize == 0)
        {
            return;
        }

        const char *platformName = GetPlatformName(platformId);
        if (platformName)
        {
            if (hasBuild)
            {
                std::snprintf(dst, dstSize, "%s(%u)", platformName, buildNumber);
                return;
            }
            std::snprintf(dst, dstSize, "%s", platformName);
            return;
        }

        if (hasBuild)
        {
            std::snprintf(dst, dstSize, "Build(%u)", buildNumber);
        }
    }

    bool TryReadPlatformId(const uint8_t *src, size_t len, uint32_t *outValue)
    {
        if (!src || !outValue)
        {
            return false;
        }
        if (len == 1)
        {
            *outValue = static_cast<uint32_t>(src[0]);
            return true;
        }
        if (len == 2)
        {
            *outValue = static_cast<uint32_t>(ReadLe16(src));
            return true;
        }
        if (len >= 4)
        {
            *outValue = ReadLe32(src);
            return true;
        }
        return false;
    }

    bool LooksLikeUtf16Le(const uint8_t *src, size_t srcLen)
    {
        // 经验判断：高字节大量为 0，通常意味着 UTF-16LE 英文文本。
        if (!src || srcLen < 2 || (srcLen % 2) != 0)
        {
            return false;
        }

        size_t zeroHigh = 0;
        const size_t pairs = srcLen / 2;
        for (size_t i = 1; i < srcLen; i += 2)
        {
            if (src[i] == 0)
            {
                ++zeroHigh;
            }
        }
        return zeroHigh * 2 >= pairs;
    }

    bool ParseServerInfoPayload(const uint8_t *payload,
                                size_t payloadLen,
                                const sockaddr_in &sender,
                                AdsLiteDiscoveryDeviceInfo *outInfo)
    {
        // 解析 UDP 负载格式:
        // [AmsAddr][tagCount][tagId(uint16), len(uint16), data...]...
        if (!payload || !outInfo)
        {
            return false;
        }

        std::memset(outInfo, 0, sizeof(*outInfo));

        char ipText[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &sender.sin_addr, ipText, sizeof(ipText)) != nullptr)
        {
            // IP 地址来自 UDP/IP 包头源地址，不在 ADS TLV 标签内部。
            std::memcpy(outInfo->ipAddress, ipText, (std::min)(sizeof(outInfo->ipAddress) - 1, std::strlen(ipText)));
            outInfo->ipAddress[sizeof(outInfo->ipAddress) - 1] = '\0';
        }
        outInfo->ipv4HostOrder = ntohl(sender.sin_addr.s_addr);

        const size_t fixedLen = sizeof(AmsAddr) + sizeof(uint32_t);
        if (payloadLen < fixedLen)
        {
            return false;
        }

        std::memcpy(&outInfo->netId, payload, sizeof(AmsNetId));
        outInfo->adsPort = ReadLe16(payload + sizeof(AmsNetId));

        uint32_t platformId = 0;
        bool hasPlatformId = false;
        uint32_t buildNumber = 0;
        bool hasBuildNumber = false;

        size_t offset = sizeof(AmsAddr);
        const uint32_t tagCount = ReadLe32(payload + offset);
        offset += sizeof(uint32_t);

        for (uint32_t i = 0; i < tagCount; ++i)
        {
            if (offset + sizeof(uint16_t) * 2 > payloadLen)
            {
                return false;
            }

            const uint16_t tag = ReadLe16(payload + offset);
            offset += sizeof(uint16_t);
            const uint16_t len = ReadLe16(payload + offset);
            offset += sizeof(uint16_t);

            if (offset + len > payloadLen)
            {
                return false;
            }

            if (tag < 32u)
            {
                outInfo->rawTagMask |= (1u << tag);
            }

            const uint8_t *tagData = payload + offset;
            switch (tag)
            {
            case 0x0005:
                // 设备名/主机名。
                CopyTextField(outInfo->deviceName, sizeof(outInfo->deviceName), tagData, len);
                break;
            case 0x0004:
                // 服务文本块；部分设备返回 OSVERSIONINFO 二进制结构。
                if (LooksLikeUtf16Le(tagData, len))
                {
                    CopyUtf16LeAsciiField(outInfo->serviceText, sizeof(outInfo->serviceText), tagData, len);
                }
                else
                {
                    CopyTextField(outInfo->serviceText, sizeof(outInfo->serviceText), tagData, len);
                }
                if (!IsMostlyPrintableAscii(outInfo->serviceText))
                {
                    uint32_t parsedBuild = 0;
                    if (TryParseOsVersionInfoTag(tagData,
                                                 len,
                                                 outInfo->osVersion,
                                                 sizeof(outInfo->osVersion),
                                                 outInfo->serviceText,
                                                 sizeof(outInfo->serviceText),
                                                 &parsedBuild))
                    {
                        if (parsedBuild > 0)
                        {
                            buildNumber = parsedBuild;
                            hasBuildNumber = true;
                        }
                    }
                    else
                    {
                        uint32_t tagPlatformId = 0;
                        if (TryReadPlatformId(tagData, len, &tagPlatformId))
                        {
                            platformId = tagPlatformId;
                            hasPlatformId = true;
                        }

                        // 未识别结构时，保留十六进制前缀便于诊断。
                        CopyBinaryAsHexField(outInfo->serviceText, sizeof(outInfo->serviceText), tagData, len);
                    }
                }
                break;
            case 0x0003:
                // 版本号: [major][minor][build-le16]
                if (len >= 4)
                {
                    outInfo->runtimeVersion.version = tagData[0];
                    outInfo->runtimeVersion.revision = tagData[1];
                    outInfo->runtimeVersion.build = ReadLe16(tagData + 2);
                }
                break;
            case 0x0012:
                // 设备指纹字符串。
                CopyTextField(outInfo->fingerprint, sizeof(outInfo->fingerprint), tagData, len);
                break;
            case 0x0014:
            {
                // 部分设备会额外携带 OS 相关标签：可能是平台 ID，也可能是 build 号。
                uint32_t tagPlatformId = 0;
                if (TryReadPlatformId(tagData, len, &tagPlatformId))
                {
                    platformId = tagPlatformId;
                    hasPlatformId = true;
                }

                uint32_t tagBuildNumber = 0;
                if (TryReadBuildNumber(tagData, len, &tagBuildNumber))
                {
                    buildNumber = tagBuildNumber;
                    hasBuildNumber = true;
                }
                break;
            }
            case 0x000F:
                // 某些固件可能直接返回 OS 文本。
                CopyTextField(outInfo->osVersion, sizeof(outInfo->osVersion), tagData, len);
                break;
            default:
                break;
            }

            offset += len;
        }

        // 仅当 osVersion 仍为空时，按标签组合出标准文本（如 Windows 10(26100)）。
        if (outInfo->osVersion[0] == '\0')
        {
            ComposeOsVersionText(outInfo->osVersion,
                                 sizeof(outInfo->osVersion),
                                 hasPlatformId ? platformId : 0u,
                                 hasBuildNumber,
                                 buildNumber);
        }

        return true;
    }

    bool IsDuplicateDevice(const AdsLiteDiscoveryDeviceInfo *items,
                           uint32_t count,
                           const AdsLiteDiscoveryDeviceInfo &candidate)
    {
        // 去重策略：优先用 NetId，比对不到时回退到 IPv4。
        for (uint32_t i = 0; i < count; ++i)
        {
            if (std::memcmp(items[i].netId.b, candidate.netId.b, sizeof(candidate.netId.b)) == 0)
            {
                return true;
            }
            if (items[i].ipv4HostOrder != 0 && items[i].ipv4HostOrder == candidate.ipv4HostOrder)
            {
                return true;
            }
        }
        return false;
    }

    bool IsGlobalBroadcast(const char *address)
    {
        return address && std::strcmp(address, "255.255.255.255") == 0;
    }

    bool IsSockAddrDuplicate(const std::vector<sockaddr_in> &items, const sockaddr_in &candidate)
    {
        for (const auto &item : items)
        {
            if (item.sin_addr.s_addr == candidate.sin_addr.s_addr && item.sin_port == candidate.sin_port)
            {
                return true;
            }
        }
        return false;
    }

    std::vector<sockaddr_in> BuildDirectedBroadcastTargets(uint16_t portNetworkOrder)
    {
        std::vector<sockaddr_in> targets;

#ifdef _WIN32
        ULONG outBufLen = 0;
        if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &outBufLen) != ERROR_BUFFER_OVERFLOW)
        {
            return targets;
        }

        std::vector<uint8_t> buffer(outBufLen, 0);
        PIP_ADAPTER_ADDRESSES adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &outBufLen) != NO_ERROR)
        {
            return targets;
        }

        for (PIP_ADAPTER_ADDRESSES adapter = adapters; adapter != nullptr; adapter = adapter->Next)
        {
            if (adapter->OperStatus != IfOperStatusUp)
            {
                continue;
            }

            for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress;
                 addr != nullptr;
                 addr = addr->Next)
            {
                if (!addr->Address.lpSockaddr || addr->Address.lpSockaddr->sa_family != AF_INET)
                {
                    continue;
                }

                sockaddr_in *ipv4 = reinterpret_cast<sockaddr_in *>(addr->Address.lpSockaddr);
                uint32_t ipHost = ntohl(ipv4->sin_addr.s_addr);
                ULONG prefix = addr->OnLinkPrefixLength;
                if (prefix > 32)
                {
                    prefix = 32;
                }

                uint32_t maskHost = 0;
                if (prefix > 0)
                {
                    maskHost = (prefix == 32) ? 0xffffffffu : (0xffffffffu << (32 - prefix));
                }
                const uint32_t broadcastHost = ipHost | (~maskHost);

                sockaddr_in target;
                std::memset(&target, 0, sizeof(target));
                target.sin_family = AF_INET;
                target.sin_port = portNetworkOrder;
                target.sin_addr.s_addr = htonl(broadcastHost);

                if (!IsSockAddrDuplicate(targets, target))
                {
                    targets.push_back(target);
                }
            }
        }
#else
        struct ifaddrs *ifaddr = nullptr;
        if (getifaddrs(&ifaddr) != 0 || !ifaddr)
        {
            return targets;
        }

        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
            {
                continue;
            }
            if ((ifa->ifa_flags & IFF_UP) == 0)
            {
                continue;
            }
            if (!ifa->ifa_netmask)
            {
                continue;
            }

            const auto *ip = reinterpret_cast<const sockaddr_in *>(ifa->ifa_addr);
            const auto *mask = reinterpret_cast<const sockaddr_in *>(ifa->ifa_netmask);
            const uint32_t ipHost = ntohl(ip->sin_addr.s_addr);
            const uint32_t maskHost = ntohl(mask->sin_addr.s_addr);
            const uint32_t broadcastHost = ipHost | (~maskHost);

            sockaddr_in target;
            std::memset(&target, 0, sizeof(target));
            target.sin_family = AF_INET;
            target.sin_port = portNetworkOrder;
            target.sin_addr.s_addr = htonl(broadcastHost);

            if (!IsSockAddrDuplicate(targets, target))
            {
                targets.push_back(target);
            }
        }

        freeifaddrs(ifaddr);
#endif

        return targets;
    }

    std::string FormatIpV4(const in_addr &addr)
    {
        char ipText[INET_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET, &addr, ipText, sizeof(ipText)) == nullptr)
        {
            return std::string("<invalid-ip>");
        }
        return std::string(ipText);
    }
}

namespace adslite
{
    namespace backend
    {
        // 向 48899 广播 SERVERINFO 请求，并在 timeoutMs 窗口内收集所有响应设备。
        int64_t BroadcastDiscovery::Discover(const char *broadcastOrSubnet,
                                             uint32_t timeoutMs,
                                             AdsLiteDiscoveryDeviceInfo *pDevices,
                                             uint32_t deviceCapacity,
                                             uint32_t *pDeviceCount)
        {
            if (!broadcastOrSubnet || !pDeviceCount)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            if (!pDevices && deviceCapacity != 0)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            if (timeoutMs == 0)
            {
                timeoutMs = 2000;
            }

            *pDeviceCount = 0;

            const int wsaState = InitSocketLibrary();
            if (wsaState != 0)
            {
                LOG_ERROR("BroadcastDiscovery::Discover WSA init failed, code=" << wsaState);
                return ADSERR_CLIENT_ERROR;
            }

            LOG_INFO("BroadcastDiscovery::Discover start target=" << broadcastOrSubnet
                                                                  << " timeoutMs=" << timeoutMs
                                                                  << " capacity=" << deviceCapacity);

            struct addrinfo hints;
            std::memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;

            struct addrinfo *results = nullptr;
            if (getaddrinfo(broadcastOrSubnet, "48899", &hints, &results) != 0 || !results)
            {
                LOG_WARN("BroadcastDiscovery::Discover getaddrinfo failed for target=" << broadcastOrSubnet);
                WSACleanup();
                return ADSERR_CLIENT_INVALIDPARM;
            }

            SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (sock == INVALID_SOCKET)
            {
                freeaddrinfo(results);
                WSACleanup();
                return ADSERR_CLIENT_ERROR;
            }

            const int enableBroadcast = 1;
            setsockopt(sock,
                       SOL_SOCKET,
                       SO_BROADCAST,
                       reinterpret_cast<const char *>(&enableBroadcast),
                       static_cast<socklen_t>(sizeof(enableBroadcast)));

            // SERVERINFO 请求负载:
            // [cookie][invokeId][serviceId][AmsAddr(置零)][tagCount=0]
            uint8_t request[sizeof(uint32_t) * 4 + sizeof(AmsAddr)] = {0};
            const uint32_t cookieLe = bhf::ads::htole(kUdpCookie);
            const uint32_t invokeIdLe = bhf::ads::htole(kUdpInvokeId);
            const uint32_t serviceInfoLe = bhf::ads::htole(kServerInfoRequest);
            const uint32_t tagCountLe = 0;
            std::memcpy(request + 0, &cookieLe, sizeof(uint32_t));
            std::memcpy(request + 4, &invokeIdLe, sizeof(uint32_t));
            std::memcpy(request + 8, &serviceInfoLe, sizeof(uint32_t));
            std::memcpy(request + 12 + sizeof(AmsAddr), &tagCountLe, sizeof(uint32_t));

            bool sent = false;
            uint32_t sendOkCount = 0;
            uint32_t sendFailCount = 0;
            for (struct addrinfo *rp = results; rp != nullptr; rp = rp->ai_next)
            {
                if (rp->ai_family != AF_INET)
                {
                    continue;
                }

                const auto *targetAddr = reinterpret_cast<const sockaddr_in *>(rp->ai_addr);
                LOG_INFO("BroadcastDiscovery::Discover send target=" << FormatIpV4(targetAddr->sin_addr));

                if (sendto(sock,
                           reinterpret_cast<const char *>(request),
                           static_cast<int>(sizeof(request)),
                           0,
                           rp->ai_addr,
                           static_cast<socklen_t>(rp->ai_addrlen)) == static_cast<int>(sizeof(request)))
                {
                    sent = true;
                    ++sendOkCount;
                }
                else
                {
                    ++sendFailCount;
                    LOG_WARN("BroadcastDiscovery::Discover send failed wsa=" << WSAGetLastError());
                }
            }

            if (IsGlobalBroadcast(broadcastOrSubnet))
            {
                const auto directedTargets = BuildDirectedBroadcastTargets(htons(48899));
                for (const auto &directed : directedTargets)
                {
                    LOG_INFO("BroadcastDiscovery::Discover extra send directed-broadcast=" << FormatIpV4(directed.sin_addr));
                    if (sendto(sock,
                               reinterpret_cast<const char *>(request),
                               static_cast<int>(sizeof(request)),
                               0,
                               reinterpret_cast<const sockaddr *>(&directed),
                               static_cast<socklen_t>(sizeof(directed))) == static_cast<int>(sizeof(request)))
                    {
                        sent = true;
                        ++sendOkCount;
                    }
                    else
                    {
                        ++sendFailCount;
                        LOG_WARN("BroadcastDiscovery::Discover directed send failed wsa=" << WSAGetLastError());
                    }
                }
            }

            LOG_INFO("BroadcastDiscovery::Discover send summary ok=" << sendOkCount << " fail=" << sendFailCount);

            if (!sent)
            {
                freeaddrinfo(results);
                closesocket(sock);
                WSACleanup();
                return ADSERR_CLIENT_ERROR;
            }

            std::vector<AdsLiteDiscoveryDeviceInfo> allFound;
            allFound.reserve(deviceCapacity > 0 ? deviceCapacity : 8);

            uint32_t recvPackets = 0;
            uint32_t recvMatchedHeader = 0;
            uint32_t recvParseFailed = 0;
            uint32_t recvDuplicated = 0;
            uint32_t recvAccepted = 0;

            // 在超时窗口内持续收包，直到时间耗尽。
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
            uint8_t buffer[2048] = {0};

            while (std::chrono::steady_clock::now() < deadline)
            {
                const auto now = std::chrono::steady_clock::now();
                const auto remainMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
                if (remainMs <= 0)
                {
                    break;
                }

                timeval tv;
                tv.tv_sec = static_cast<long>(remainMs / 1000);
                tv.tv_usec = static_cast<long>((remainMs % 1000) * 1000);

                fd_set readSet;
                FD_ZERO(&readSet);
                FD_SET(sock, &readSet);

                const int selectState = NATIVE_SELECT(sock + 1, &readSet, nullptr, nullptr, &tv);
                if (selectState <= 0)
                {
                    break;
                }

                sockaddr_in sender;
                std::memset(&sender, 0, sizeof(sender));
                socklen_t senderLen = static_cast<socklen_t>(sizeof(sender));
                const int bytes = recvfrom(sock,
                                           reinterpret_cast<char *>(buffer),
                                           static_cast<int>(sizeof(buffer)),
                                           0,
                                           reinterpret_cast<sockaddr *>(&sender),
                                           &senderLen);
                if (bytes <= 0)
                {
                    continue;
                }
                ++recvPackets;

                if (bytes < 12)
                {
                    ++recvParseFailed;
                    LOG_WARN("BroadcastDiscovery::Discover drop short packet bytes=" << bytes);
                    continue;
                }

                const uint32_t cookie = ReadLe32(buffer + 0);
                const uint32_t invokeId = ReadLe32(buffer + 4);
                const uint32_t service = ReadLe32(buffer + 8);
                // 仅接受匹配本次请求的 SERVERINFO 响应包。
                if (cookie != kUdpCookie || invokeId != kUdpInvokeId || service != kServerInfoResponse)
                {
                    LOG_INFO("BroadcastDiscovery::Discover ignore packet from=" << FormatIpV4(sender.sin_addr)
                                                                                << " cookie=0x" << std::hex << cookie
                                                                                << " invoke=" << std::dec << invokeId
                                                                                << " service=0x" << std::hex << service << std::dec);
                    continue;
                }
                ++recvMatchedHeader;

                AdsLiteDiscoveryDeviceInfo item;
                if (!ParseServerInfoPayload(buffer + 12,
                                            static_cast<size_t>(bytes - 12),
                                            sender,
                                            &item))
                {
                    ++recvParseFailed;
                    LOG_WARN("BroadcastDiscovery::Discover parse failed from=" << FormatIpV4(sender.sin_addr)
                                                                               << " payloadBytes=" << (bytes - 12));
                    continue;
                }

                if (IsDuplicateDevice(allFound.data(), static_cast<uint32_t>(allFound.size()), item))
                {
                    ++recvDuplicated;
                    continue;
                }

                allFound.push_back(item);
                ++recvAccepted;
                LOG_INFO("BroadcastDiscovery::Discover accepted ip=" << item.ipAddress
                                                                     << " netid="
                                                                     << static_cast<int>(item.netId.b[0]) << "."
                                                                     << static_cast<int>(item.netId.b[1]) << "."
                                                                     << static_cast<int>(item.netId.b[2]) << "."
                                                                     << static_cast<int>(item.netId.b[3]) << "."
                                                                     << static_cast<int>(item.netId.b[4]) << "."
                                                                     << static_cast<int>(item.netId.b[5]));
            }

            freeaddrinfo(results);
            closesocket(sock);
            WSACleanup();

            *pDeviceCount = static_cast<uint32_t>(allFound.size());
            const uint32_t copyCount = (std::min)(deviceCapacity, *pDeviceCount);
            for (uint32_t i = 0; i < copyCount; ++i)
            {
                pDevices[i] = allFound[i];
            }

            LOG_INFO("BroadcastDiscovery::Discover recv summary packets=" << recvPackets
                                                                          << " matchedHeader=" << recvMatchedHeader
                                                                          << " parseFailed=" << recvParseFailed
                                                                          << " duplicated=" << recvDuplicated
                                                                          << " accepted=" << recvAccepted);

            // 语义约定：未发现任何设备返回超时，发现到至少一台返回成功。
            if (*pDeviceCount == 0)
            {
                if (IsGlobalBroadcast(broadcastOrSubnet))
                {
                    LOG_WARN("BroadcastDiscovery::Discover no device via global broadcast; try directed broadcast like 192.168.x.255");
                }
                return ADSERR_CLIENT_SYNCTIMEOUT;
            }
            return ADSERR_NOERR;
        }
    }
}
