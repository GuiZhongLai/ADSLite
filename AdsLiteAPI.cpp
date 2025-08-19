#include "AdsLiteAPI.h"
#include "AdsLiteLib.h"
#include "AmsNetId.h"
#include "Log.h"

int64_t AdsLiteGetRemoteAddress(const char *addr, AmsNetId *ams)
{
    return GetRemoteAddress(addr, *ams);
}

int64_t AdsLiteAddRemoteRoute(const char *addr, const char *destAddr, const char *routeName)
{
    std::string result = std::string(destAddr) + ".1.1";
    AmsNetId destNetId;
    destNetId = AmsNetIdHelper::create(result);
    return AddRemoteRoute(addr, destNetId, destAddr, routeName);
}

int64_t AdsLiteAddLocalRoute(const char *addr, AmsNetId *ams)
{
    return AddLocalRoute(*ams, addr);
}

void AdsLiteDeleteLocalRoute(AmsNetId *ams)
{
    DeleteLocalRoute(*ams);
}

void AdsLiteSetLocalAddress(const char *addr)
{
    AmsNetId netId;
    netId = AmsNetIdHelper::create(addr);
    SetLocalAddress(netId);
}

int64_t AdsLiteGetLocalAddress(uint16_t port, AmsAddr *pAddr)
{
    return AdsGetLocalAddressEx(port, pAddr);
}

uint16_t AdsLitePortOpen()
{
    return AdsPortOpenEx();
}

int64_t AdsLitePortClose(uint16_t port)
{
    return AdsPortCloseEx(port);
}

int64_t AdsLiteSyncSetTimeout(uint16_t port, uint32_t timeout)
{
    return AdsSyncSetTimeoutEx(port, timeout);
}

int64_t AdsLiteSyncGetTimeout(uint16_t port, uint32_t *pTimeout)
{
    return AdsSyncGetTimeoutEx(port, pTimeout);
}

int64_t AdsLiteSyncReadReq(uint16_t port, const AmsAddr *pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t length, void *pData, uint32_t *pBytesRead)
{
    return AdsSyncReadReqEx2(port, pAddr, indexGroup, indexOffset, length, pData, pBytesRead);
}

int64_t AdsLiteSyncWriteReq(uint16_t port, const AmsAddr *pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t length, const void *pBuffer)
{
    return AdsSyncWriteReqEx2(port, pAddr, indexGroup, indexOffset, length, pBuffer);
}

int64_t AdsLiteSyncReadWriteReq(uint16_t port, const AmsAddr *pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t readLength, void *pReadData, uint32_t writeLength, const void *pWriteData, uint32_t *pBytesRead)
{
    return AdsSyncReadWriteReqEx2(port, pAddr, indexGroup, indexOffset, readLength, pReadData, writeLength, pWriteData, pBytesRead);
}

int64_t AdsLiteSyncReadStateReq(uint16_t port, const AmsAddr *pAddr, uint16_t *pAdsState, uint16_t *pDeviceState)
{
    return AdsSyncReadStateReqEx(port, pAddr, pAdsState, pDeviceState);
}

int64_t AdsLiteSyncWriteControlReq(uint16_t port, const AmsAddr *pAddr, uint16_t adsState, uint16_t deviceState, uint32_t length, const void *pData)
{
    return AdsSyncWriteControlReqEx(port, pAddr, adsState, deviceState, length, pData);
}
