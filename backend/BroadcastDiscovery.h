#pragma once

#include "AdsLiteDef.h"

#include <cstdint>

namespace adslite
{
    namespace backend
    {
        class BroadcastDiscovery
        {
        public:
            static int64_t Discover(const char *broadcastOrSubnet,
                                    uint32_t timeoutMs,
                                    AdsLiteDiscoveryDeviceInfo *pDevices,
                                    uint32_t deviceCapacity,
                                    uint32_t *pDeviceCount);
        };
    }
}
