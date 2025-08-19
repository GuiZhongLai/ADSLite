#pragma once

#include "AdsLiteDef.h"
#include <cstdint>
#include <string>

long AdsPortCloseEx(uint16_t port);

uint16_t AdsPortOpenEx();

long AdsGetLocalAddressEx(uint16_t port, AmsAddr *pAddr);

long AdsSyncSetTimeoutEx(uint16_t port, uint32_t timeout);

long AdsSyncGetTimeoutEx(uint16_t port, uint32_t *pTimeout);

long AdsSyncReadReqEx2(uint16_t port, const AmsAddr *pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t length, void *pData, uint32_t *pBytesRead);

long AdsSyncWriteReqEx2(uint16_t port, const AmsAddr *pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t length, const void *pBuffer);

long AdsSyncReadStateReqEx(uint16_t port, const AmsAddr *pAddr, uint16_t *pAdsState, uint16_t *pDeviceState);

long AdsSyncWriteControlReqEx(uint16_t port, const AmsAddr *pAddr, uint16_t adsState, uint16_t deviceState, uint32_t length, const void *pData);

long AdsSyncReadWriteReqEx2(uint16_t port, const AmsAddr *pAddr,
                            uint32_t indexGroup,
                            uint32_t indexOffset,
                            uint32_t readLength,
                            void *pReadData,
                            uint32_t writeLength,
                            const void *pWriteData,
                            uint32_t *pBytesRead);

long AddLocalRoute(AmsNetId ams, const char *ip);

void DeleteLocalRoute(AmsNetId ams);

void SetLocalAddress(AmsNetId ams);

long AddRemoteRoute(const std::string &remote,
                    AmsNetId destNetId,
                    const std::string &destAddr,
                    const std::string &routeName);

long GetRemoteAddress(const std::string &remote, AmsNetId &netId);