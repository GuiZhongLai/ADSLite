#pragma once

#include "AdsLiteDef.h"

#include <cstdint>

struct IAdsBackend
{
    virtual ~IAdsBackend() {}

    virtual int64_t GetDeviceNetId(const char *addr, AmsNetId *ams) = 0;
    virtual int64_t DiscoverDevices(const char *broadcastOrSubnet,
                                    const AdsLiteDiscoveryOptions *pOptions,
                                    AdsLiteDiscoveryDeviceInfo *pDevices,
                                    uint32_t deviceCapacity,
                                    uint32_t *pDeviceCount,
                                    uint32_t *pBytesRequired) = 0;
    virtual int64_t InitRouting(const char *addr, AmsNetId *ams) = 0;
    virtual void ShutdownRouting(AmsNetId *ams) = 0;

    virtual void SetLocalAddress(const char *addr) = 0;
    virtual int64_t GetLocalAddress(uint16_t port, AmsAddr *pAddr) = 0;

    virtual uint16_t PortOpen() = 0;
    virtual int64_t PortClose(uint16_t port) = 0;

    virtual int64_t SyncSetTimeout(uint16_t port, uint32_t timeout) = 0;
    virtual int64_t SyncGetTimeout(uint16_t port, uint32_t *pTimeout) = 0;

    virtual int64_t SyncReadReq(uint16_t port,
                                const AmsAddr *pAddr,
                                uint32_t indexGroup,
                                uint32_t indexOffset,
                                uint32_t length,
                                void *pData,
                                uint32_t *pBytesRead) = 0;

    virtual int64_t SyncWriteReq(uint16_t port,
                                 const AmsAddr *pAddr,
                                 uint32_t indexGroup,
                                 uint32_t indexOffset,
                                 uint32_t length,
                                 const void *pBuffer) = 0;

    virtual int64_t SyncReadWriteReq(uint16_t port,
                                     const AmsAddr *pAddr,
                                     uint32_t indexGroup,
                                     uint32_t indexOffset,
                                     uint32_t readLength,
                                     void *pReadData,
                                     uint32_t writeLength,
                                     const void *pWriteData,
                                     uint32_t *pBytesRead) = 0;

    virtual int64_t SyncReadStateReq(uint16_t port,
                                     const AmsAddr *pAddr,
                                     uint16_t *pAdsState,
                                     uint16_t *pDeviceState) = 0;

    virtual int64_t SyncWriteControlReq(uint16_t port,
                                        const AmsAddr *pAddr,
                                        uint16_t adsState,
                                        uint16_t deviceState,
                                        uint32_t length,
                                        const void *pData) = 0;
};
