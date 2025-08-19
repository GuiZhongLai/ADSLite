#pragma once

#include "AdsLiteDef.h"

#include <cstdint>
#include <string>
#include <cstring>
#include <sstream>

inline bool operator==(const AmsNetId &lhs, const AmsNetId &rhs)
{
    return std::memcmp(lhs.b, rhs.b, 6) == 0;
}
inline bool operator!=(const AmsNetId &lhs, const AmsNetId &rhs)
{
    return !operator==(lhs, rhs);
}
inline bool operator<(const AmsNetId &lhs, const AmsNetId &rhs)
{
    return std::memcmp(lhs.b, rhs.b, 6) < 0;
}
inline bool operator>(const AmsNetId &lhs, const AmsNetId &rhs)
{
    return rhs < lhs;
}
inline bool operator<=(const AmsNetId &lhs, const AmsNetId &rhs)
{
    return !(rhs < lhs);
}
inline bool operator>=(const AmsNetId &lhs, const AmsNetId &rhs)
{
    return !(lhs < rhs);
}

namespace AmsNetIdHelper
{
    inline bool isEmpty(const AmsNetId &id)
    {
        static const unsigned char zero[6] = {0};
        return std::memcmp(id.b, zero, 6) == 0;
    }

    inline AmsNetId create(uint32_t ipv4Addr)
    {
        AmsNetId id;
        id.b[5] = 1;
        id.b[4] = 1;
        id.b[3] = (uint8_t)(ipv4Addr & 0x000000ff);
        id.b[2] = (uint8_t)((ipv4Addr & 0x0000ff00) >> 8);
        id.b[1] = (uint8_t)((ipv4Addr & 0x00ff0000) >> 16);
        id.b[0] = (uint8_t)((ipv4Addr & 0xff000000) >> 24);
        return id;
    }

    inline AmsNetId create(const std::string &addr)
    {
        AmsNetId id;
        std::istringstream iss(addr);
        std::string s;
        size_t i = 0;

        // 读取最多 6 个字节
        while ((i < sizeof(id.b)) && std::getline(iss, s, '.'))
        {
            // 使用 atoi 并 % 256 防止越界
            id.b[i] = static_cast<unsigned char>(std::atoi(s.c_str()) % 256);
            ++i;
        }

        // 检查：是否正好 6 段，且没有多余内容
        if ((i != sizeof(id.b)) || std::getline(iss, s, '.'))
        {
            // 如果段数不足 6 或还有剩余内容（超过 6 段），则设为全零
            static const ::AmsNetId empty = {{0}};
            std::memcpy(id.b, empty.b, sizeof(id.b));
        }
        return id;
    }

    inline AmsNetId create(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
    {
        return AmsNetId{{b0, b1, b2, b3, b4, b5}};
    }

    inline std::string toString(const AmsNetId &id)
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d.%d.%d.%d.%d.%d",
                      id.b[0], id.b[1], id.b[2], id.b[3], id.b[4], id.b[5]);
        return std::string(buf);
    }
}