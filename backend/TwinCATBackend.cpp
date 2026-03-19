#include "backend/TwinCATBackend.h"

#include <cstdint>

#include <string>

#include "standalone/Log.h"
#include "backend/NetIdResolver.h"

#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
#define NOMINMAX
#include <windows.h>
typedef void(__stdcall *PAmsRouterNotificationFuncEx)(long nEvent);
#define dllexport dllimport
#include "TcAdsAPI.h"
#undef dllexport
#endif

namespace
{
    inline long ClampTimeout(uint32_t timeout)
    {
        const uint32_t maxLong = 0x7fffffffU;
        return static_cast<long>(timeout > maxLong ? maxLong : timeout);
    }

}

TwinCATBackend::TwinCATBackend()
    : available(false),
      reason("not initialized")
{
#if !defined(_WIN32)
    reason = "TwinCAT backend is only available on Windows";
    return;
#elif !defined(ADSLITE_TWINCAT_ENABLED) || !(ADSLITE_TWINCAT_ENABLED)
    reason = "TwinCAT backend is disabled by build configuration";
    return;
#else
    available = true;
    reason = "TcAdsDll import library is linked";
#endif
}

bool TwinCATBackend::IsAvailable() const
{
    return available;
}

const char *TwinCATBackend::AvailabilityReason() const
{
    return reason.c_str();
}

int64_t TwinCATBackend::InitRouting(const char *addr, AmsNetId *ams)
{
    if (!addr || !ams)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    const long status = AdsLiteStandaloneGetRemoteAddress(addr, *ams);
    if (status == 0)
    {
        LOG_INFO("TwinCATBackend::InitRouting resolved device netid");
    }
    else
    {
        LOG_WARN("TwinCATBackend::InitRouting failed to resolve device netid");
    }
    return status;
}

void TwinCATBackend::ShutdownRouting(AmsNetId *ams)
{
    (void)ams;
    LOG_INFO("TwinCATBackend::ShutdownRouting no-op");
}

void TwinCATBackend::SetLocalAddress(const char *addr)
{
    (void)addr;
    LOG_INFO("TwinCATBackend::SetLocalAddress no-op");
}

int64_t TwinCATBackend::GetLocalAddress(uint16_t port, AmsAddr *pAddr)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
    if (!pAddr)
    {
        return ADSERR_CLIENT_NOAMSADDR;
    }
#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsGetLocalAddressEx(port, pAddr);
#else
    (void)port;
    (void)pAddr;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

uint16_t TwinCATBackend::PortOpen()
{
    if (!available)
    {
        return 0;
    }
#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    const long port = AdsPortOpenEx();
    if (port <= 0 || port > 0xffff)
    {
        return 0;
    }
    return static_cast<uint16_t>(port);
#else
    return 0;
#endif
}

int64_t TwinCATBackend::PortClose(uint16_t port)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsPortCloseEx(port);
#else
    (void)port;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

int64_t TwinCATBackend::SyncSetTimeout(uint16_t port, uint32_t timeout)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsSyncSetTimeoutEx(port, ClampTimeout(timeout));
#else
    (void)port;
    (void)timeout;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

int64_t TwinCATBackend::SyncGetTimeout(uint16_t port, uint32_t *pTimeout)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
    if (!pTimeout)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    long timeout = 0;
    const long status = AdsSyncGetTimeoutEx(port, &timeout);
    if (!status && pTimeout)
    {
        *pTimeout = static_cast<uint32_t>(timeout < 0 ? 0 : timeout);
    }
    return status;
#else
    (void)port;
    (void)pTimeout;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

int64_t TwinCATBackend::SyncReadReq(uint16_t port,
                                    const AmsAddr *pAddr,
                                    uint32_t indexGroup,
                                    uint32_t indexOffset,
                                    uint32_t length,
                                    void *pData,
                                    uint32_t *pBytesRead)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
    if (!pAddr)
    {
        return ADSERR_CLIENT_NOAMSADDR;
    }
    if (length && !pData)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsSyncReadReqEx2(port,
                             const_cast<AmsAddr *>(pAddr),
                             indexGroup,
                             indexOffset,
                             length,
                             pData,
                             reinterpret_cast<unsigned long *>(pBytesRead));
#else
    (void)port;
    (void)pAddr;
    (void)indexGroup;
    (void)indexOffset;
    (void)length;
    (void)pData;
    (void)pBytesRead;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

int64_t TwinCATBackend::SyncWriteReq(uint16_t port,
                                     const AmsAddr *pAddr,
                                     uint32_t indexGroup,
                                     uint32_t indexOffset,
                                     uint32_t length,
                                     const void *pBuffer)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
    if (!pAddr)
    {
        return ADSERR_CLIENT_NOAMSADDR;
    }
    if (length && !pBuffer)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsSyncWriteReqEx(port,
                             const_cast<AmsAddr *>(pAddr),
                             indexGroup,
                             indexOffset,
                             length,
                             const_cast<void *>(pBuffer));
#else
    (void)port;
    (void)pAddr;
    (void)indexGroup;
    (void)indexOffset;
    (void)length;
    (void)pBuffer;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

int64_t TwinCATBackend::SyncReadWriteReq(uint16_t port,
                                         const AmsAddr *pAddr,
                                         uint32_t indexGroup,
                                         uint32_t indexOffset,
                                         uint32_t readLength,
                                         void *pReadData,
                                         uint32_t writeLength,
                                         const void *pWriteData,
                                         uint32_t *pBytesRead)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
    if (!pAddr)
    {
        return ADSERR_CLIENT_NOAMSADDR;
    }
    if ((readLength && !pReadData) || (writeLength && !pWriteData))
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsSyncReadWriteReqEx2(port,
                                  const_cast<AmsAddr *>(pAddr),
                                  indexGroup,
                                  indexOffset,
                                  readLength,
                                  pReadData,
                                  writeLength,
                                  const_cast<void *>(pWriteData),
                                  reinterpret_cast<unsigned long *>(pBytesRead));
#else
    (void)port;
    (void)pAddr;
    (void)indexGroup;
    (void)indexOffset;
    (void)readLength;
    (void)pReadData;
    (void)writeLength;
    (void)pWriteData;
    (void)pBytesRead;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

int64_t TwinCATBackend::SyncReadStateReq(uint16_t port,
                                         const AmsAddr *pAddr,
                                         uint16_t *pAdsState,
                                         uint16_t *pDeviceState)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
    if (!pAddr)
    {
        return ADSERR_CLIENT_NOAMSADDR;
    }
    if (!pAdsState || !pDeviceState)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsSyncReadStateReqEx(port,
                                 const_cast<AmsAddr *>(pAddr),
                                 pAdsState,
                                 pDeviceState);
#else
    (void)port;
    (void)pAddr;
    (void)pAdsState;
    (void)pDeviceState;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}

int64_t TwinCATBackend::SyncWriteControlReq(uint16_t port,
                                            const AmsAddr *pAddr,
                                            uint16_t adsState,
                                            uint16_t deviceState,
                                            uint32_t length,
                                            const void *pData)
{
    if (!available)
    {
        return ADSERR_CLIENT_PORTNOTOPEN;
    }
    if (!pAddr)
    {
        return ADSERR_CLIENT_NOAMSADDR;
    }
    if (length && !pData)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

#if defined(_WIN32) && defined(ADSLITE_TWINCAT_ENABLED) && ADSLITE_TWINCAT_ENABLED
    return AdsSyncWriteControlReqEx(port,
                                    const_cast<AmsAddr *>(pAddr),
                                    adsState,
                                    deviceState,
                                    length,
                                    const_cast<void *>(pData));
#else
    (void)port;
    (void)pAddr;
    (void)adsState;
    (void)deviceState;
    (void)length;
    (void)pData;
    return ADSERR_DEVICE_SRVNOTSUPP;
#endif
}
