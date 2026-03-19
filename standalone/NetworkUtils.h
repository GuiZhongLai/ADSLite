#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif

std::string getSubnetPrefix(const std::string &ip)
{
    auto last_dot = ip.find_last_of('.');
    if (last_dot != std::string::npos)
    {
        return ip.substr(0, last_dot);
    }
    return ip;
}

std::string getLocalIpForTarget(const std::string &target_ip)
{
    std::string target_prefix = getSubnetPrefix(target_ip);

#ifdef _WIN32
    // Windows 实现
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        return "";
    }

    IP_ADAPTER_ADDRESSES *adapter_addresses = nullptr;
    ULONG out_buffer_length = 0;

    // 第一次调用获取所需缓冲区大小
    GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                         adapter_addresses, &out_buffer_length);

    adapter_addresses = static_cast<IP_ADAPTER_ADDRESSES *>(malloc(out_buffer_length));
    if (!adapter_addresses)
    {
        WSACleanup();
        return "";
    }

    if (GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr,
                             adapter_addresses, &out_buffer_length) != NO_ERROR)
    {
        free(adapter_addresses);
        WSACleanup();
        return "";
    }

    for (auto adapter = adapter_addresses; adapter != nullptr; adapter = adapter->Next)
    {
        if (adapter->OperStatus != IfOperStatusUp)
            continue; // 只处理已激活的接口

        for (auto addr = adapter->FirstUnicastAddress; addr != nullptr; addr = addr->Next)
        {
            if (addr->Address.lpSockaddr->sa_family == AF_INET)
            {
                auto ipv4 = reinterpret_cast<sockaddr_in *>(addr->Address.lpSockaddr);
                std::string local_ip = inet_ntoa(ipv4->sin_addr);
                if (getSubnetPrefix(local_ip) == target_prefix)
                {
                    free(adapter_addresses);
                    WSACleanup();
                    return local_ip;
                }
            }
        }
    }

    free(adapter_addresses);
    WSACleanup();
    return "";

#else
    // Linux/Unix 实现
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1)
    {
        return "";
    }

    std::string result;

    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
            continue;
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;
        if (!(ifa->ifa_flags & IFF_UP))
            continue; // 只处理已激活的接口

        struct sockaddr_in *sin = reinterpret_cast<struct sockaddr_in *>(ifa->ifa_addr);
        std::string local_ip = inet_ntoa(sin->sin_addr);

        if (getSubnetPrefix(local_ip) == target_prefix)
        {
            result = local_ip;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return result;
#endif
}