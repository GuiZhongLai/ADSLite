#pragma once

#include "../AdsLiteDef.h"
#include "IAdsBackend.h"

#include <cstdint>

namespace adslite
{
    namespace ecat
    {
        class ECatClient
        {
        public:
            explicit ECatClient(IAdsBackend &backend);

            // 枚举目标上的 EtherCAT Master（走 IO 口 AMSPORT_R0_IO）。
            // 协议对齐 Beckhoff AdsLib ECatAccess::ListECatMasters / AdsTool ecat list-masters。
            // 缓冲语义对齐 AdsLiteDiscoverDevices：capacity 不够时只写入前 N 项，
            // pCount / pBytesRequired 仍反映全部数量。
            int64_t ListECatMasters(uint16_t port,
                                    const AmsAddr *pAddr,
                                    AdsLiteECatMasterInfo *pMasters,
                                    uint32_t capacity,
                                    uint32_t *pCount,
                                    uint32_t *pBytesRequired);

        private:
            static constexpr uint32_t kIoDeviceStateBase = 0x5000u;
            static constexpr uint32_t kReadDevIds = 0x1u;
            static constexpr uint32_t kReadDevCount = 0x2u;
            static constexpr uint32_t kReadDevNetId = 0x5u;
            static constexpr uint32_t kReadDevType = 0x7u;
            // IOADS_IOF_READDEVNAME 与 READDEVIDS 同为 0x1，靠 indexGroup=base+deviceId 区分。
            static constexpr uint32_t kReadDevName = 0x1u;

            static AmsAddr BuildIoAddr(const AmsAddr *pAddr);

            IAdsBackend &backend_;
        };
    }
}
