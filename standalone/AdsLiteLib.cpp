#define NOMINMAX
#include "AdsLiteLib.h"
#include "AmsRouter.h"
#include "Log.h"
#include "wrap_endian.h"

#include <limits>
#include <cstring>

static AmsRouter &GetRouter()
{
    static AmsRouter router;
    return router;
}

static bool PrependUdpLenTagId(Frame &frame, const uint16_t length, const uint16_t tagId)
{
    frame.prepend(bhf::ads::htole(length));
    frame.prepend(bhf::ads::htole(tagId));
    return true;
}

static bool PrependUdpTag(Frame &frame, const std::string &value, const uint16_t tagId)
{
    if (value.length() + 1 > std::numeric_limits<uint16_t>::max())
    {
        LOG_WARN(__FUNCTION__ << "(): value is too long, skipping tagId (" << std::dec << tagId << ")\n");
        return false;
    }
    const auto length = static_cast<uint16_t>(value.length() + 1);
    frame.prepend(value.data(), length);
    return PrependUdpLenTagId(frame, length, tagId);
}

static bool PrependUdpTag(Frame &frame, const AmsNetId &value, const uint16_t tagId)
{
    const uint16_t length = sizeof(value);
    frame.prepend(&value, length);
    return PrependUdpLenTagId(frame, length, tagId);
}

enum UdpTag : uint16_t
{
    PASSWORD = 2,
    COMPUTERNAME = 5,
    NETID = 7,
    ROUTENAME = 12,
    USERNAME = 13,
};

enum UdpServiceId : uint32_t
{
    SERVERINFO = 1,
    ADDROUTE = 6,
    RESPONSE = 0x80000000,
};

static long SendRecv(const std::string &remote, Frame &f, const uint32_t serviceId)
{
    f.prepend(bhf::ads::htole(serviceId));

    static const uint32_t invokeId = 0;
    f.prepend(bhf::ads::htole(invokeId));

    static const uint32_t UDP_COOKIE = 0x71146603;
    f.prepend(bhf::ads::htole(UDP_COOKIE));

    const auto addresses = bhf::ads::GetListOfAddresses(remote, "48899");
    UdpSocket s{addresses.get()};
    s.write(f);
    f.reset();

    static constexpr auto headerLength = sizeof(serviceId) + sizeof(invokeId) + sizeof(UDP_COOKIE);
    timeval timeout{5, 0};
    s.read(f, &timeout);
    if (headerLength > f.capacity())
    {
        LOG_ERROR(__FUNCTION__ << "(): frame too short to be AMS response '0x" << std::hex << f.capacity() << "'\n");
        return ADSERR_DEVICE_INVALIDSIZE;
    }

    const auto cookie = f.pop_letoh<uint32_t>();
    if (UDP_COOKIE != cookie)
    {
        LOG_ERROR(__FUNCTION__ << "(): response contains invalid cookie '" << cookie << "'\n");
        return ADSERR_DEVICE_INVALIDDATA;
    }
    const auto invoke = f.pop_letoh<uint32_t>();
    if (invokeId != invoke)
    {
        LOG_ERROR(__FUNCTION__ << "(): response contains invalid invokeId '" << invoke << "'\n");
        return ADSERR_DEVICE_INVALIDDATA;
    }
    const auto service = f.pop_letoh<uint32_t>();
    if ((UdpServiceId::RESPONSE | serviceId) != service)
    {
        LOG_ERROR(__FUNCTION__ << "(): response contains invalid serviceId '" << std::hex << service << "'\n");
        return ADSERR_DEVICE_INVALIDDATA;
    }
    return 0;
}

#define ASSERT_PORT(port)                       \
    do                                          \
    {                                           \
        if ((port) <= 0 || (port) > UINT16_MAX) \
        {                                       \
            return ADSERR_CLIENT_PORTNOTOPEN;   \
        }                                       \
    } while (false)

#define ASSERT_PORT_AND_AMSADDR(port, pAddr) \
    do                                       \
    {                                        \
        ASSERT_PORT(port);                   \
        if (!(pAddr))                        \
        {                                    \
            return ADSERR_CLIENT_NOAMSADDR;  \
        }                                    \
    } while (false)

long AdsPortCloseEx(uint16_t port)
{
    ASSERT_PORT(port);
    return GetRouter().ClosePort(port);
}

uint16_t AdsPortOpenEx()
{
    return GetRouter().OpenPort();
}

long AdsGetLocalAddressEx(uint16_t port, AmsAddr *pAddr)
{
    ASSERT_PORT_AND_AMSADDR(port, pAddr);
    return GetRouter().GetLocalAddress(port, pAddr);
}

long AdsSyncSetTimeoutEx(uint16_t port, uint32_t timeout)
{
    ASSERT_PORT(port);
    return GetRouter().SetTimeout(port, timeout);
}

long AdsSyncGetTimeoutEx(uint16_t port, uint32_t *pTimeout)
{
    ASSERT_PORT(port);
    if (!pTimeout)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }
    return GetRouter().GetTimeout(port, *pTimeout);
}

long AdsSyncReadReqEx2(uint16_t port, const AmsAddr *pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t length, void *pData, uint32_t *pBytesRead)
{
    ASSERT_PORT_AND_AMSADDR(port, pAddr);
    if (!pData)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    try
    {
        AmsRequest request{
            *pAddr,
            port,
            AoEHeader::READ,
            length,
            pData,
            pBytesRead,
            sizeof(AoERequestHeader)};
        request.frame.prepend(AoERequestHeader{
            indexGroup,
            indexOffset,
            length});
        return GetRouter().AdsRequest(request);
    }
    catch (const std::bad_alloc &)
    {
        return GLOBALERR_NO_MEMORY;
    }
}

long AdsSyncWriteReqEx2(uint16_t port, const AmsAddr *pAddr, uint32_t indexGroup, uint32_t indexOffset, uint32_t length, const void *pBuffer)
{
    ASSERT_PORT_AND_AMSADDR(port, pAddr);
    if (!pBuffer)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    try
    {
        AmsRequest request{
            *pAddr,
            port,
            AoEHeader::WRITE,
            0,
            nullptr,
            nullptr,
            sizeof(AoERequestHeader) + length,
        };
        request.frame.prepend(pBuffer, length);
        request.frame.prepend<AoERequestHeader>({indexGroup,
                                                 indexOffset,
                                                 length});
        return GetRouter().AdsRequest(request);
    }
    catch (const std::bad_alloc &)
    {
        return GLOBALERR_NO_MEMORY;
    }
}

long AdsSyncReadStateReqEx(uint16_t port, const AmsAddr *pAddr, uint16_t *pAdsState, uint16_t *pDeviceState)
{
    ASSERT_PORT_AND_AMSADDR(port, pAddr);
    if (!pAdsState || !pDeviceState)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    try
    {
        uint16_t buffer[2];
        AmsRequest request{
            *pAddr,
            port,
            AoEHeader::READ_STATE,
            sizeof(buffer),
            buffer};
        const auto status = GetRouter().AdsRequest(request);
        if (!status)
        {
            *pAdsState = bhf::ads::letoh(buffer[0]);
            *pDeviceState = bhf::ads::letoh(buffer[1]);
        }
        return status;
    }
    catch (const std::bad_alloc &)
    {
        return GLOBALERR_NO_MEMORY;
    }
}

long AdsSyncWriteControlReqEx(uint16_t port, const AmsAddr *pAddr, uint16_t adsState, uint16_t deviceState, uint32_t length, const void *pData)
{
    ASSERT_PORT_AND_AMSADDR(port, pAddr);
    try
    {
        AmsRequest request{
            *pAddr,
            port,
            AoEHeader::WRITE_CONTROL,
            0, nullptr, nullptr,
            sizeof(AdsWriteCtrlRequest) + length};
        request.frame.prepend(pData, length);
        request.frame.prepend<AdsWriteCtrlRequest>({adsState,
                                                    deviceState,
                                                    length});
        return GetRouter().AdsRequest(request);
    }
    catch (const std::bad_alloc &)
    {
        return GLOBALERR_NO_MEMORY;
    }
}

long AdsSyncReadWriteReqEx2(uint16_t port,
                            const AmsAddr *pAddr,
                            uint32_t indexGroup,
                            uint32_t indexOffset,
                            uint32_t readLength,
                            void *pReadData,
                            uint32_t writeLength,
                            const void *pWriteData,
                            uint32_t *pBytesRead)
{
    ASSERT_PORT_AND_AMSADDR(port, pAddr);
    if ((readLength && !pReadData) || (writeLength && !pWriteData))
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    try
    {
        AmsRequest request{
            *pAddr,
            port,
            AoEHeader::READ_WRITE,
            readLength,
            pReadData,
            pBytesRead,
            sizeof(AoEReadWriteReqHeader) + writeLength};
        request.frame.prepend(pWriteData, writeLength);
        request.frame.prepend(AoEReadWriteReqHeader{
            indexGroup,
            indexOffset,
            readLength,
            writeLength});
        return GetRouter().AdsRequest(request);
    }
    catch (const std::bad_alloc &)
    {
        return GLOBALERR_NO_MEMORY;
    }
}

long AddLocalRoute(AmsNetId ams, const char *ip)
{
    try
    {
        return GetRouter().AddRoute(ams, ip);
    }
    catch (const std::bad_alloc &)
    {
        return GLOBALERR_NO_MEMORY;
    }
    catch (const std::runtime_error &)
    {
        return GLOBALERR_TARGET_PORT;
    }
}

void DeleteLocalRoute(AmsNetId ams)
{
    GetRouter().DelRoute(ams);
}

void SetLocalAddress(AmsNetId ams)
{
    GetRouter().SetLocalAddress(ams);
}

long AddRemoteRoute(const std::string &remote, AmsNetId destNetId, const std::string &destAddr, const std::string &routeName)
{
    const std::string remoteUsername = "Administrator";
    const std::string remotePassword = "1";
    Frame f{256};
    uint32_t tagCount = 0;
    tagCount += PrependUdpTag(f, destAddr, UdpTag::COMPUTERNAME);
    tagCount += PrependUdpTag(f, remotePassword, UdpTag::PASSWORD);
    tagCount += PrependUdpTag(f, remoteUsername, UdpTag::USERNAME);
    tagCount += PrependUdpTag(f, destNetId, UdpTag::NETID);
    tagCount += PrependUdpTag(f, routeName.empty() ? destAddr : routeName, UdpTag::ROUTENAME);
    f.prepend(bhf::ads::htole(tagCount));

    const auto myAddr = AmsAddr{destNetId, 0};
    f.prepend(&myAddr, sizeof(myAddr));

    const auto status = SendRecv(remote, f, UdpServiceId::ADDROUTE);
    if (status)
    {
        return status;
    }

    // 至少有AmsAddr和count字段
    if (sizeof(AmsAddr) + sizeof(uint32_t) > f.capacity())
    {
        LOG_ERROR(__FUNCTION__ << "(): frame too short to be AMS response '0x" << std::hex << f.capacity() << "'\n");
        return ADSERR_DEVICE_INVALIDSIZE;
    }

    // 忽略AmsAddr作为响应
    f.remove(sizeof(AmsAddr));

    // 处理UDP发现标签
    auto count = f.pop_letoh<uint32_t>();
    while (count--)
    {
        uint16_t tag;
        uint16_t len;
        if (sizeof(tag) + sizeof(len) > f.capacity())
        {
            LOG_ERROR(__FUNCTION__ << "(): frame too short to be AMS response '0x" << std::hex << f.capacity() << "'\n");
            return ADSERR_DEVICE_INVALIDSIZE;
        }

        tag = f.pop_letoh<uint16_t>();
        len = f.pop_letoh<uint16_t>();
        if (1 != tag)
        {
            LOG_WARN(__FUNCTION__ << "(): response contains tagId '0x" << std::hex << tag << "' -> ignoring\n");
            f.remove(len);
            continue;
        }
        if (sizeof(uint32_t) != len)
        {
            LOG_ERROR(__FUNCTION__ << "(): response contains invalid tag length '" << std::hex << len << "'\n");
            return ADSERR_DEVICE_INVALIDSIZE;
        }
        return f.pop_letoh<uint32_t>();
    }
    return ADSERR_DEVICE_INVALIDDATA;
}

long GetRemoteAddress(const std::string &remote, AmsNetId &netId)
{
    Frame f{128};

    const uint32_t tagCount = 0;
    f.prepend(bhf::ads::htole(tagCount));

    const auto myAddr = AmsAddr{{}, 0};
    f.prepend(&myAddr, sizeof(myAddr));

    const auto status = SendRecv(remote, f, UdpServiceId::SERVERINFO);
    if (status)
    {
        return status;
    }

    // 至少有AmsAddr
    if (sizeof(netId) > f.capacity())
    {
        LOG_ERROR(__FUNCTION__ << "(): frame too short to be AMS response '0x" << std::hex << f.capacity() << "'\n");
        return ADSERR_DEVICE_INVALIDSIZE;
    }
    memcpy(&netId, f.data(), sizeof(netId));
    return 0;
}
