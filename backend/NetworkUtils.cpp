#include "backend/NetworkUtils.h"

#include <string>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h> // InetNtopA
#include <iphlpapi.h>
#include <windows.h>
#else
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif

// 辅助函数：将网络字节序的 IPv4 地址（uint32_t）转为点分十进制字符串
static std::string ipToString(unsigned long ip_n)
{
    char buffer[INET_ADDRSTRLEN];
#ifdef _WIN32
    if (InetNtopA(AF_INET, &ip_n, buffer, sizeof(buffer)) == nullptr)
    {
        return "";
    }
#else
    if (inet_ntop(AF_INET, &ip_n, buffer, sizeof(buffer)) == nullptr)
    {
        return "";
    }
#endif
    return std::string(buffer);
}

// 辅助函数：根据前缀长度（0~32）生成网络字节序的子网掩码
static unsigned long prefixLengthToNetmask(unsigned long prefixLen)
{
    if (prefixLen == 0)
    {
        return 0x00000000;
    }
    if (prefixLen >= 32)
    {
        return 0xFFFFFFFF;
    }
    // 主机字节序下左移，再转为网络字节序
    unsigned long mask = (0xFFFFFFFFU << (32 - prefixLen)) & 0xFFFFFFFFU;
    return htonl(mask); // 返回网络字节序，与 inet_addr / sin_addr.s_addr 一致
}

std::string getLocalIpForTarget(const std::string &target_ip)
{
    // 将目标 IP 转为网络字节序的 32 位整数
    unsigned long target_ip_n = inet_addr(target_ip.c_str());
    if (target_ip_n == INADDR_NONE)
    {
        return ""; // 无效 IP
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return "";
    }

    ULONG outBufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(outBufLen));
    if (!pAddresses)
    {
        WSACleanup();
        return "";
    }

    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen);

    if (dwRetVal == ERROR_BUFFER_OVERFLOW)
    {
        free(pAddresses);
        pAddresses = static_cast<PIP_ADAPTER_ADDRESSES>(malloc(outBufLen));
        if (!pAddresses)
        {
            WSACleanup();
            return "";
        }
        dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &outBufLen);
    }

    if (dwRetVal != NO_ERROR)
    {
        free(pAddresses);
        WSACleanup();
        return "";
    }

    std::string result;

    for (PIP_ADAPTER_ADDRESSES adapter = pAddresses; adapter != nullptr; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp)
        {
            continue;
        }

        for (PIP_ADAPTER_UNICAST_ADDRESS addr = adapter->FirstUnicastAddress;
             addr != nullptr;
             addr = addr->Next)
        {

            if (addr->Address.lpSockaddr->sa_family != AF_INET)
            {
                continue;
            }

            sockaddr_in *ipv4 = reinterpret_cast<sockaddr_in *>(addr->Address.lpSockaddr);
            unsigned long local_ip_n = ipv4->sin_addr.s_addr;

            // 获取前缀长度（IPv4 通常为 0~32）
            ULONG prefixLen = addr->OnLinkPrefixLength;
            if (prefixLen > 32)
                prefixLen = 32;

            unsigned long netmask_n = prefixLengthToNetmask(prefixLen);

            unsigned long local_net = local_ip_n & netmask_n;
            unsigned long target_net = target_ip_n & netmask_n;

            if (local_net == target_net)
            {
                result = ipToString(local_ip_n);
                break;
            }
        }
        if (!result.empty())
        {
            break;
        }
    }

    free(pAddresses);
    WSACleanup();
    return result;

#else // Linux / POSIX
    struct ifaddrs *ifaddrs_ptr = nullptr;
    if (getifaddrs(&ifaddrs_ptr) == -1)
    {
        return "";
    }

    std::string result;

    for (struct ifaddrs *ifa = ifaddrs_ptr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
        {
            continue;
        }
        if (!(ifa->ifa_flags & IFF_UP))
        {
            continue;
        }
        if (!ifa->ifa_netmask)
        {
            continue;
        }

        struct sockaddr_in *addr = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
        struct sockaddr_in *netmask = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_netmask);

        unsigned long local_ip_n = addr->sin_addr.s_addr;
        unsigned long netmask_n = netmask->sin_addr.s_addr;

        unsigned long local_net = local_ip_n & netmask_n;
        unsigned long target_net = target_ip_n & netmask_n;

        if (local_net == target_net)
        {
            result = ipToString(local_ip_n);
            break;
        }
    }

    freeifaddrs(ifaddrs_ptr);
    return result;
#endif
}