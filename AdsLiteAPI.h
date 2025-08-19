#ifndef ADS_LITE_API_H
#define ADS_LITE_API_H

#include "AdsLiteDef.h"

#define ADSAPIERR_NOERROR 0x0000

#if defined(_WIN32) || defined(_WIN64)
#ifdef ADS_LITE_BUILD_DLL
#define ADS_LITE_API __declspec(dllexport)
#else
#define ADS_LITE_API __declspec(dllimport)
#endif
#else
#if __GNUC__ >= 4
#define ADS_LITE_API __attribute__((visibility("default")))
#else
#define ADS_LITE_API
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    ADS_LITE_API int64_t AdsLiteGetRemoteAddress(const char *addr, AmsNetId *ams);

    ADS_LITE_API int64_t AdsLiteAddRemoteRoute(const char *addr, const char *destAddr, const char *routeName);

    ADS_LITE_API int64_t AdsLiteAddLocalRoute(const char *addr, AmsNetId *ams);

    ADS_LITE_API void AdsLiteDeleteLocalRoute(AmsNetId *ams);

    ADS_LITE_API void AdsLiteSetLocalAddress(const char *addr);

    ADS_LITE_API int64_t AdsLiteGetLocalAddress(uint16_t port, AmsAddr *pAddr);

    ADS_LITE_API uint16_t AdsLitePortOpen();

    ADS_LITE_API int64_t AdsLitePortClose(uint16_t port);

    ADS_LITE_API int64_t AdsLiteSyncSetTimeout(uint16_t port, uint32_t timeout);

    ADS_LITE_API int64_t AdsLiteSyncGetTimeout(uint16_t port, uint32_t *pTimeout);

    ADS_LITE_API int64_t AdsLiteSyncReadReq(uint16_t port,
                                            const AmsAddr *pAddr,
                                            uint32_t indexGroup,
                                            uint32_t indexOffset,
                                            uint32_t length,
                                            void *pData,
                                            uint32_t *pBytesRead);

    ADS_LITE_API int64_t AdsLiteSyncWriteReq(uint16_t port,
                                             const AmsAddr *pAddr,
                                             uint32_t indexGroup,
                                             uint32_t indexOffset,
                                             uint32_t length,
                                             const void *pBuffer);

    ADS_LITE_API int64_t AdsLiteSyncReadWriteReq(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 uint32_t indexGroup,
                                                 uint32_t indexOffset,
                                                 uint32_t readLength,
                                                 void *pReadData,
                                                 uint32_t writeLength,
                                                 const void *pWriteData,
                                                 uint32_t *pBytesRead);

    ADS_LITE_API int64_t AdsLiteSyncReadStateReq(uint16_t port, const AmsAddr *pAddr, uint16_t *pAdsState, uint16_t *pDeviceState);

    ADS_LITE_API int64_t AdsLiteSyncWriteControlReq(uint16_t port,
                                                    const AmsAddr *pAddr,
                                                    uint16_t adsState,
                                                    uint16_t deviceState,
                                                    uint32_t length,
                                                    const void *pData);

#ifdef __cplusplus
}
#endif

#endif // ADS_LITE_API_H