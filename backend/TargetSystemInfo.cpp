#include "backend/TargetSystemInfo.h"

#include "../standalone/wrap_endian.h"

#include <cstdio>
#include <cstring>

namespace adslite
{
    namespace targetinfo
    {
        namespace
        {
            constexpr uint32_t DEVICE_INFO_INDEX_GROUP = 0x01010004u;
            constexpr uint32_t DEVICE_INFO_OFFSET_SYSTEM_ID = 0x1u;
            constexpr uint32_t DEVICE_INFO_OFFSET_PLATFORM_ID = 0x2u;
            constexpr uint32_t SYSTEM_ID_RAW_SIZE = 16u;
            constexpr uint16_t DEVICE_INFO_AMS_PORT = 30u;
            constexpr uint32_t TARGET_SYSTEM_ID_LENGTH = ADSLITE_SYSTEM_ID_BUFFER_LENGTH;
        }

        TargetSystemInfo::TargetSystemInfo(IAdsBackend &backend)
            : backend_(backend)
        {
        }

        AmsAddr TargetSystemInfo::BuildDeviceInfoAddr(const AmsAddr *pAddr) const
        {
            AmsAddr deviceInfoAddr = *pAddr;
            // 平台/系统信息属于设备信息服务，目标 AMS 端口必须固定为 30。
            deviceInfoAddr.port = DEVICE_INFO_AMS_PORT;
            return deviceInfoAddr;
        }

        int64_t TargetSystemInfo::ReadPlatformId(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 ADSPLATFORMID *pPlatformId)
        {
            if (!pAddr || !pPlatformId)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            if (targetPlatformId != ADSLITE_PLATFORM_ID_UNKNOWN)
            {
                *pPlatformId = targetPlatformId;
                return ADSERR_NOERR;
            }

            const AmsAddr deviceInfoAddr = BuildDeviceInfoAddr(pAddr);
            uint32_t rawPlatformId = 0;
            uint32_t bytesRead = 0;
            const int64_t status = backend_.SyncReadReq(port,
                                                        &deviceInfoAddr,
                                                        DEVICE_INFO_INDEX_GROUP,
                                                        DEVICE_INFO_OFFSET_PLATFORM_ID,
                                                        static_cast<uint32_t>(sizeof(rawPlatformId)),
                                                        &rawPlatformId,
                                                        &bytesRead);
            if (status != ADSERR_NOERR)
            {
                return status;
            }

            const uint32_t platformId = bhf::ads::letoh(rawPlatformId);
            targetPlatformId = static_cast<ADSPLATFORMID>(platformId);
            *pPlatformId = targetPlatformId;
            return ADSERR_NOERR;
        }

        int64_t TargetSystemInfo::ReadSystemId(uint16_t port,
                                               const AmsAddr *pAddr,
                                               char *pSystemId,
                                               uint32_t systemIdBufferLength)
        {
            if (!pAddr || !pSystemId)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }
            if (systemIdBufferLength < TARGET_SYSTEM_ID_LENGTH)
            {
                return ADSERR_DEVICE_INVALIDSIZE;
            }

            if (targetSystemId[0] != '\0')
            {
                std::memcpy(pSystemId, targetSystemId, TARGET_SYSTEM_ID_LENGTH);
                return ADSERR_NOERR;
            }

            const AmsAddr deviceInfoAddr = BuildDeviceInfoAddr(pAddr);
            uint8_t readBuffer[SYSTEM_ID_RAW_SIZE] = {0};
            uint32_t bytesRead = 0;
            const int64_t status = backend_.SyncReadReq(port,
                                                        &deviceInfoAddr,
                                                        DEVICE_INFO_INDEX_GROUP,
                                                        DEVICE_INFO_OFFSET_SYSTEM_ID,
                                                        static_cast<uint32_t>(sizeof(readBuffer)),
                                                        readBuffer,
                                                        &bytesRead);
            if (status != ADSERR_NOERR)
            {
                return status;
            }

            char systemId[ADSLITE_SYSTEM_ID_BUFFER_LENGTH] = {0};
            const int written = std::snprintf(systemId,
                                              static_cast<uint32_t>(sizeof(systemId)),
                                              "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                                              readBuffer[3], readBuffer[2], readBuffer[1], readBuffer[0],
                                              readBuffer[5], readBuffer[4], readBuffer[7], readBuffer[6],
                                              readBuffer[8], readBuffer[9],
                                              readBuffer[10], readBuffer[11], readBuffer[12], readBuffer[13], readBuffer[14], readBuffer[15]);
            if (written <= 0 || static_cast<uint32_t>(written) >= sizeof(systemId))
            {
                return ADSERR_DEVICE_INVALIDSIZE;
            }

            std::memcpy(targetSystemId, systemId, TARGET_SYSTEM_ID_LENGTH);
            std::memcpy(pSystemId, systemId, TARGET_SYSTEM_ID_LENGTH);
            return ADSERR_NOERR;
        }

        void TargetSystemInfo::Clear()
        {
            targetPlatformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            std::memset(targetSystemId, 0, sizeof(targetSystemId));
        }
    }
}
