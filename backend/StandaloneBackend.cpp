#include "backend/StandaloneBackend.h"

#include "standalone/AdsLiteLib.h"
#include "standalone/AmsNetId.h"
#include "standalone/NetworkUtils.h"

#include <string>

int64_t StandaloneBackend::InitRouting(const char *addr, AmsNetId *ams)
{
    int64_t ret = GetRemoteAddress(addr, *ams);
    if (ret != 0)
    {
        return ret;
    }

    const std::string localIp = getLocalIpForTarget(addr);
    if (localIp.empty())
    {
        return ADSERR_CLIENT_ERROR;
    }

    const std::string netIdText = localIp + ".1.1";
    const AmsNetId localNetId = AmsNetIdHelper::create(netIdText);
    ret = AddRemoteRoute(addr, localNetId, localIp, "AdsLiteRoute");
    if (ret != 0)
    {
        return ret;
    }

    return AddLocalRoute(*ams, addr);
}

void StandaloneBackend::ShutdownRouting(AmsNetId *ams)
{
    DeleteLocalRoute(*ams);
}

void StandaloneBackend::SetLocalAddress(const char *addr)
{
    const AmsNetId netId = AmsNetIdHelper::create(addr);
    ::SetLocalAddress(netId);
}

int64_t StandaloneBackend::GetLocalAddress(uint16_t port, AmsAddr *pAddr)
{
    return AdsGetLocalAddressEx(port, pAddr);
}

uint16_t StandaloneBackend::PortOpen()
{
    return AdsPortOpenEx();
}

int64_t StandaloneBackend::PortClose(uint16_t port)
{
    return AdsPortCloseEx(port);
}

int64_t StandaloneBackend::SyncSetTimeout(uint16_t port, uint32_t timeout)
{
    return AdsSyncSetTimeoutEx(port, timeout);
}

int64_t StandaloneBackend::SyncGetTimeout(uint16_t port, uint32_t *pTimeout)
{
    return AdsSyncGetTimeoutEx(port, pTimeout);
}

int64_t StandaloneBackend::SyncReadReq(uint16_t port,
                                       const AmsAddr *pAddr,
                                       uint32_t indexGroup,
                                       uint32_t indexOffset,
                                       uint32_t length,
                                       void *pData,
                                       uint32_t *pBytesRead)
{
    return AdsSyncReadReqEx2(port, pAddr, indexGroup, indexOffset, length, pData, pBytesRead);
}

int64_t StandaloneBackend::SyncWriteReq(uint16_t port,
                                        const AmsAddr *pAddr,
                                        uint32_t indexGroup,
                                        uint32_t indexOffset,
                                        uint32_t length,
                                        const void *pBuffer)
{
    return AdsSyncWriteReqEx2(port, pAddr, indexGroup, indexOffset, length, pBuffer);
}

int64_t StandaloneBackend::SyncReadWriteReq(uint16_t port,
                                            const AmsAddr *pAddr,
                                            uint32_t indexGroup,
                                            uint32_t indexOffset,
                                            uint32_t readLength,
                                            void *pReadData,
                                            uint32_t writeLength,
                                            const void *pWriteData,
                                            uint32_t *pBytesRead)
{
    return AdsSyncReadWriteReqEx2(port,
                                  pAddr,
                                  indexGroup,
                                  indexOffset,
                                  readLength,
                                  pReadData,
                                  writeLength,
                                  pWriteData,
                                  pBytesRead);
}

int64_t StandaloneBackend::SyncReadStateReq(uint16_t port,
                                            const AmsAddr *pAddr,
                                            uint16_t *pAdsState,
                                            uint16_t *pDeviceState)
{
    return AdsSyncReadStateReqEx(port, pAddr, pAdsState, pDeviceState);
}

int64_t StandaloneBackend::SyncWriteControlReq(uint16_t port,
                                               const AmsAddr *pAddr,
                                               uint16_t adsState,
                                               uint16_t deviceState,
                                               uint32_t length,
                                               const void *pData)
{
    return AdsSyncWriteControlReqEx(port, pAddr, adsState, deviceState, length, pData);
}
