#pragma once

#include "AdsLiteDef.h"

#include <cstring>

inline bool operator==(const AmsAddr &lhs, const AmsAddr &rhs)
{
    return std::memcmp(&lhs, &rhs, sizeof(lhs)) == 0;
}

inline bool operator<(const AmsAddr &lhs, const AmsAddr &rhs)
{
    int netIdCmp = std::memcmp(&lhs.netId, &rhs.netId, sizeof(AmsNetId));
    if (netIdCmp != 0)
        return netIdCmp < 0;
    return lhs.port < rhs.port;
}
