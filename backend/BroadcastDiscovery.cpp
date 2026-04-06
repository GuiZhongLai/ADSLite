#include "backend/BroadcastDiscovery.h"

#include "standalone/wrap_endian.h"
#include "standalone/wrap_socket.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

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
    // - 0x0004: serviceText (常见 UTF-16LE)
    // - 0x0003: runtimeVersion
    // - 0x0012: systemId
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
                // 服务文本块，常见为 UTF-16LE。
                if (LooksLikeUtf16Le(tagData, len))
                {
                    CopyUtf16LeAsciiField(outInfo->serviceText, sizeof(outInfo->serviceText), tagData, len);
                }
                else
                {
                    CopyTextField(outInfo->serviceText, sizeof(outInfo->serviceText), tagData, len);
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
                // 系统标识字符串。
                CopyTextField(outInfo->systemId, sizeof(outInfo->systemId), tagData, len);
                break;
            default:
                break;
            }

            offset += len;
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

            InitSocketLibrary();

            struct addrinfo hints;
            std::memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            hints.ai_protocol = IPPROTO_UDP;

            struct addrinfo *results = nullptr;
            if (getaddrinfo(broadcastOrSubnet, "48899", &hints, &results) != 0 || !results)
            {
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
            for (struct addrinfo *rp = results; rp != nullptr; rp = rp->ai_next)
            {
                if (rp->ai_family != AF_INET)
                {
                    continue;
                }
                if (sendto(sock,
                           reinterpret_cast<const char *>(request),
                           static_cast<int>(sizeof(request)),
                           0,
                           rp->ai_addr,
                           static_cast<socklen_t>(rp->ai_addrlen)) == static_cast<int>(sizeof(request)))
                {
                    sent = true;
                }
            }

            if (!sent)
            {
                freeaddrinfo(results);
                closesocket(sock);
                WSACleanup();
                return ADSERR_CLIENT_ERROR;
            }

            std::vector<AdsLiteDiscoveryDeviceInfo> allFound;
            allFound.reserve(deviceCapacity > 0 ? deviceCapacity : 8);

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

                if (bytes < 12)
                {
                    continue;
                }

                const uint32_t cookie = ReadLe32(buffer + 0);
                const uint32_t invokeId = ReadLe32(buffer + 4);
                const uint32_t service = ReadLe32(buffer + 8);
                // 仅接受匹配本次请求的 SERVERINFO 响应包。
                if (cookie != kUdpCookie || invokeId != kUdpInvokeId || service != kServerInfoResponse)
                {
                    continue;
                }

                AdsLiteDiscoveryDeviceInfo item;
                if (!ParseServerInfoPayload(buffer + 12,
                                            static_cast<size_t>(bytes - 12),
                                            sender,
                                            &item))
                {
                    continue;
                }

                if (IsDuplicateDevice(allFound.data(), static_cast<uint32_t>(allFound.size()), item))
                {
                    continue;
                }

                allFound.push_back(item);
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

            // 语义约定：未发现任何设备返回超时，发现到至少一台返回成功。
            if (*pDeviceCount == 0)
            {
                return ADSERR_CLIENT_SYNCTIMEOUT;
            }
            return ADSERR_NOERR;
        }
    }
}
