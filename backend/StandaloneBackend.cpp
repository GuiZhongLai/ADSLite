#include "backend/StandaloneBackend.h"

#include "standalone/AdsLiteLib.h"
#include "standalone/AmsNetId.h"
#include "standalone/Log.h"
#include "backend/NetworkUtils.h"

#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#include "StandaloneBackend.h"
#endif

namespace
{
    std::string GetLocalComputerName()
    {
#ifdef _WIN32
        char buffer[MAX_COMPUTERNAME_LENGTH + 1] = {0};
        DWORD size = MAX_COMPUTERNAME_LENGTH + 1;
        if (GetComputerNameA(buffer, &size) != 0 && size > 0)
        {
            return std::string(buffer, size);
        }
#else
        char buffer[256] = {0};
        if (gethostname(buffer, sizeof(buffer) - 1) == 0 && buffer[0] != '\0')
        {
            return std::string(buffer);
        }
#endif
        return std::string();
    }
}

int64_t StandaloneBackend::GetDeviceNetId(const char *addr, AmsNetId *ams)
{
    if (!addr || !ams)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }
    return GetRemoteAddress(addr, *ams);
}

int64_t StandaloneBackend::InitRouting(const char *addr, AmsNetId *ams)
{
    if (!addr || !ams)
        return ADSERR_CLIENT_INVALIDPARM;

    try
    {
        int64_t ret = GetRemoteAddress(addr, *ams);
        if (ret != 0)
            return ret;

        const std::string localIp = getLocalIpForTarget(addr);
        if (localIp.empty())
        {
            LOG_WARN("StandaloneBackend::InitRouting failed to resolve local ip for target " << addr);
            return ADSERR_CLIENT_ERROR;
        }

        const std::string netIdText = localIp + ".1.1";
        const AmsNetId localNetId = AmsNetIdHelper::create(netIdText);
        if (AmsNetIdHelper::isEmpty(localNetId))
        {
            LOG_ERROR("StandaloneBackend::InitRouting produced empty local netid from localIp=" << localIp);
            return ADSERR_CLIENT_NOAMSADDR;
        }

        const std::string localComputerName = GetLocalComputerName();
        const std::string routeName = localComputerName.empty() ? localIp : localComputerName;

        LOG_INFO("StandaloneBackend::InitRouting localIp=" << localIp
                                                           << ", localNetId=" << AmsNetIdHelper::toString(localNetId)
                                                           << ", routeName=" << routeName);

        ret = AddRemoteRoute(addr, localNetId, routeName, routeName);
        if (ret != 0)
        {
            LOG_WARN("StandaloneBackend::InitRouting AddRemoteRoute failed ret=0x" << std::hex << ret << std::dec);
            return ret;
        }

        return AddLocalRoute(*ams, addr);
    }
    catch (const std::system_error &)
    {
        return ADSERR_CLIENT_W32ERROR;
    }
    catch (const std::runtime_error &)
    {
        return ADSERR_CLIENT_ERROR;
    }
    catch (const std::exception &)
    {
        return ADSERR_DEVICE_TIMEOUT;
    }
    catch (...)
    {
        return ADSERR_DEVICE_EXCEPTION;
    }
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
    const AmsAddr stateAddr = BuildStateAddr(pAddr);
    return AdsSyncReadStateReqEx(port, &stateAddr, pAdsState, pDeviceState);
}

int64_t StandaloneBackend::SyncWriteControlReq(uint16_t port,
                                               const AmsAddr *pAddr,
                                               uint16_t adsState,
                                               uint16_t deviceState,
                                               uint32_t length,
                                               const void *pData)
{
    const AmsAddr stateAddr = BuildStateAddr(pAddr);
    return AdsSyncWriteControlReqEx(port, &stateAddr, adsState, deviceState, length, pData);
}

AmsAddr StandaloneBackend::BuildStateAddr(const AmsAddr *pAddr) const
{
    AmsAddr stateAddr = *pAddr;
    // 操作状态固定走 10000 端口，避免调用方传入 851 导致服务不支持。
    stateAddr.port = kStatePort;
    return stateAddr;
}
