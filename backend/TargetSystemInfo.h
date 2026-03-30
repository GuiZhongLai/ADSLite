#pragma once

#include "IAdsBackend.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>

namespace adslite
{
    namespace targetinfo
    {
        class TargetSystemInfo
        {
        public:
            explicit TargetSystemInfo(IAdsBackend &backend);

            int64_t ReadPlatformId(uint16_t port,
                                   const AmsAddr *pAddr,
                                   ADSPLATFORMID *pPlatformId);

            int64_t ReadSystemId(uint16_t port,
                                 const AmsAddr *pAddr,
                                 char *pSystemId,
                                 uint32_t systemIdBufferLength);

            void Clear();

        private:
            AmsAddr BuildDeviceInfoAddr(const AmsAddr *pAddr) const;

            IAdsBackend &backend_;
            ADSPLATFORMID targetPlatformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            char targetSystemId[ADSLITE_SYSTEM_ID_BUFFER_LENGTH] = {0}; // 36 chars + null terminator
        };
    }
}
