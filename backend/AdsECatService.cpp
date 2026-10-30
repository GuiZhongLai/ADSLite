#include "backend/AdsECatService.h"

#include "../standalone/wrap_endian.h"
#include "standalone/Log.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <vector>

namespace adslite
{
    namespace ecat
    {
        ECatClient::ECatClient(IAdsBackend &backend)
            : backend_(backend)
        {
        }

        AmsAddr ECatClient::BuildIoAddr(const AmsAddr *pAddr)
        {
            AmsAddr ioAddr = *pAddr;
            // 与 Beckhoff ECatAccess 一致：ECAT Master 枚举固定走 IO 口 300。
            ioAddr.port = AMSPORT_R0_IO;
            return ioAddr;
        }

        int64_t ECatClient::ListECatMasters(uint16_t port,
                                           const AmsAddr *pAddr,
                                           AdsLiteECatMasterInfo *pMasters,
                                           uint32_t capacity,
                                           uint32_t *pCount,
                                           uint32_t *pBytesRequired)
        {
            if (!pAddr || !pCount)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }
            if (pMasters == nullptr && capacity > 0)
            {
                // 允许 pMasters==nullptr 仅查询数量；此时 capacity 应视为 0。
                capacity = 0;
            }

            *pCount = 0;
            if (pBytesRequired)
            {
                *pBytesRequired = 0;
            }

            const AmsAddr ioAddr = BuildIoAddr(pAddr);

            uint32_t numberOfDevices = 0;
            uint32_t bytesRead = 0;
            int64_t status = backend_.SyncReadReq(port,
                                                  &ioAddr,
                                                  kIoDeviceStateBase,
                                                  kReadDevCount,
                                                  static_cast<uint32_t>(sizeof(numberOfDevices)),
                                                  &numberOfDevices,
                                                  &bytesRead);
            if (status != ADSERR_NOERR)
            {
                LOG_WARN("ListECatMasters: READDEVCOUNT failed ret=0x" << std::hex << status);
                return status;
            }
            if (bytesRead != sizeof(numberOfDevices))
            {
                LOG_WARN("ListECatMasters: corrupt device count length=" << bytesRead);
                return ADSERR_DEVICE_INVALIDDATA;
            }

            numberOfDevices = bhf::ads::letoh(numberOfDevices);
            if (numberOfDevices == 0)
            {
                return ADSERR_NOERR;
            }
            // device id 为 uint16；过大则后续向量 "+1" 无意义且易溢出。
            if (numberOfDevices > static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()))
            {
                LOG_WARN("ListECatMasters: device count out of range=" << numberOfDevices);
                return ADSERR_DEVICE_INVALIDDATA;
            }

            // 官方布局：首元素为占位（与 count 相关），真正的 deviceId 从 index 1 开始。
            std::vector<uint16_t> deviceIds(static_cast<size_t>(numberOfDevices) + 1u);
            const uint32_t idsBytes =
                static_cast<uint32_t>(deviceIds.size() * sizeof(uint16_t));
            bytesRead = 0;
            status = backend_.SyncReadReq(port,
                                          &ioAddr,
                                          kIoDeviceStateBase,
                                          kReadDevIds,
                                          idsBytes,
                                          deviceIds.data(),
                                          &bytesRead);
            if (status != ADSERR_NOERR)
            {
                LOG_WARN("ListECatMasters: READDEVIDS failed ret=0x" << std::hex << status);
                return status;
            }
            if (bytesRead < idsBytes)
            {
                LOG_WARN("ListECatMasters: short READDEVIDS length=" << bytesRead
                                                                    << " expected=" << idsBytes);
                return ADSERR_DEVICE_INVALIDDATA;
            }

            std::vector<AdsLiteECatMasterInfo> masters;
            masters.reserve(numberOfDevices);

            for (uint32_t i = 1; i <= numberOfDevices; ++i)
            {
                const uint16_t deviceId = bhf::ads::letoh(deviceIds[i]);
                const uint32_t deviceGroup = kIoDeviceStateBase + static_cast<uint32_t>(deviceId);

                uint16_t deviceType = 0;
                bytesRead = 0;
                status = backend_.SyncReadReq(port,
                                              &ioAddr,
                                              deviceGroup,
                                              kReadDevType,
                                              static_cast<uint32_t>(sizeof(deviceType)),
                                              &deviceType,
                                              &bytesRead);
                if (status != ADSERR_NOERR)
                {
                    LOG_WARN("ListECatMasters: READDEVTYPE id=" << deviceId
                                                               << " failed ret=0x" << std::hex
                                                               << status);
                    return status;
                }
                if (bytesRead != sizeof(deviceType))
                {
                    LOG_WARN("ListECatMasters: corrupt READDEVTYPE length for id=" << deviceId
                                                                                  << " len=" << bytesRead);
                    return ADSERR_DEVICE_INVALIDDATA;
                }
                deviceType = bhf::ads::letoh(deviceType);

                AdsLiteECatMasterInfo info{};
                info.deviceId = deviceId;
                info.deviceType = deviceType;
                std::memset(info.name, 0, sizeof(info.name));

                bytesRead = 0;
                status = backend_.SyncReadReq(port,
                                              &ioAddr,
                                              deviceGroup,
                                              kReadDevName,
                                              static_cast<uint32_t>(sizeof(info.name) - 1u),
                                              info.name,
                                              &bytesRead);
                if (status != ADSERR_NOERR)
                {
                    LOG_WARN("ListECatMasters: READDEVNAME id=" << deviceId
                                                               << " failed ret=0x" << std::hex
                                                               << status);
                    return status;
                }
                info.name[sizeof(info.name) - 1u] = '\0';

                AmsNetId netId{};
                bytesRead = 0;
                status = backend_.SyncReadReq(port,
                                              &ioAddr,
                                              deviceGroup,
                                              kReadDevNetId,
                                              static_cast<uint32_t>(sizeof(netId)),
                                              &netId,
                                              &bytesRead);
                if (status != ADSERR_NOERR)
                {
                    LOG_WARN("ListECatMasters: READDEVNETID id=" << deviceId
                                                                << " failed ret=0x" << std::hex
                                                                << status);
                    return status;
                }
                if (bytesRead != sizeof(netId))
                {
                    LOG_WARN("ListECatMasters: corrupt netId length for id=" << deviceId
                                                                            << " len=" << bytesRead);
                    return ADSERR_DEVICE_INVALIDDATA;
                }
                info.netId = netId;
                masters.push_back(info);
            }

            *pCount = numberOfDevices;
            if (pBytesRequired)
            {
                *pBytesRequired =
                    numberOfDevices * static_cast<uint32_t>(sizeof(AdsLiteECatMasterInfo));
            }

            if (pMasters && capacity > 0 && !masters.empty())
            {
                const uint32_t toCopy = std::min(capacity, numberOfDevices);
                std::memcpy(pMasters, masters.data(),
                            static_cast<size_t>(toCopy) * sizeof(AdsLiteECatMasterInfo));
            }

            return ADSERR_NOERR;
        }
    }
}
