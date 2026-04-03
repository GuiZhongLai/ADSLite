#pragma once

#include "backend/IAdsBackend.h"

#include <string>

class TwinCATBackend : public IAdsBackend
{
public:
    TwinCATBackend();

    bool IsAvailable() const;
    const char *AvailabilityReason() const;

    int64_t GetDeviceNetId(const char *addr, AmsNetId *ams) override;
    int64_t InitRouting(const char *addr, AmsNetId *ams) override;
    void ShutdownRouting(AmsNetId *ams) override;

    void SetLocalAddress(const char *addr) override;
    int64_t GetLocalAddress(uint16_t port, AmsAddr *pAddr) override;

    uint16_t PortOpen() override;
    int64_t PortClose(uint16_t port) override;

    int64_t SyncSetTimeout(uint16_t port, uint32_t timeout) override;
    int64_t SyncGetTimeout(uint16_t port, uint32_t *pTimeout) override;

    int64_t SyncReadReq(uint16_t port,
                        const AmsAddr *pAddr,
                        uint32_t indexGroup,
                        uint32_t indexOffset,
                        uint32_t length,
                        void *pData,
                        uint32_t *pBytesRead) override;

    int64_t SyncWriteReq(uint16_t port,
                         const AmsAddr *pAddr,
                         uint32_t indexGroup,
                         uint32_t indexOffset,
                         uint32_t length,
                         const void *pBuffer) override;

    int64_t SyncReadWriteReq(uint16_t port,
                             const AmsAddr *pAddr,
                             uint32_t indexGroup,
                             uint32_t indexOffset,
                             uint32_t readLength,
                             void *pReadData,
                             uint32_t writeLength,
                             const void *pWriteData,
                             uint32_t *pBytesRead) override;

    int64_t SyncReadStateReq(uint16_t port,
                             const AmsAddr *pAddr,
                             uint16_t *pAdsState,
                             uint16_t *pDeviceState) override;

    int64_t SyncWriteControlReq(uint16_t port,
                                const AmsAddr *pAddr,
                                uint16_t adsState,
                                uint16_t deviceState,
                                uint32_t length,
                                const void *pData) override;

private:
    AmsAddr BuildStateAddr(const AmsAddr *pAddr) const;

    bool available;
    std::string reason;
    static constexpr uint16_t kStatePort = 10000;
};
