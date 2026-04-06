/**
 * @file AdsLiteAPI.cpp
 * @brief AdsLite 高级 ADS API 实现
 *
 * 本文件提供了 AdsLiteAPI.h 中声明的所有 API 的实现。
 * 实现基于 standalone 代码库，保持了原有的功能和行为。
 */

#include "AdsLiteAPI.h"
#include "backend/AdsFileService.h"
#include "backend/BackendServices.h"
#include "backend/BackendSelector.h"
#include "backend/IAdsBackend.h"
#include "backend/NetIdResolver.h"
#include "backend/TargetSystemInfo.h"

#include <cstdio>
#include <cstring>

namespace
{
    adslite::backend::BackendServices &Services()
    {
        return adslite::backend::GetBackendServices();
    }

    IAdsBackend &Backend()
    {
        return Services().Backend();
    }

    adslite::targetinfo::TargetSystemInfo &TargetInfo()
    {
        return Services().TargetInfo();
    }

    adslite::file::FileServiceClient &FileService()
    {
        return Services().FileService();
    }
}

/**
 * @brief 获取目标设备的 AMS NetId
 * @see AdsLiteAPI.h::AdsLiteGetDeviceNetId
 */
int64_t AdsLiteGetDeviceNetId(const char *addr, AmsNetId *ams)
{
    return Backend().GetDeviceNetId(addr, ams);
}

int64_t AdsLiteDiscoverDevices(const char *broadcastOrSubnet,
                               const AdsLiteDiscoveryOptions *pOptions,
                               AdsLiteDiscoveryDeviceInfo *pDevices,
                               uint32_t deviceCapacity,
                               uint32_t *pDeviceCount,
                               uint32_t *pBytesRequired)
{
    return Backend().DiscoverDevices(broadcastOrSubnet,
                                     pOptions,
                                     pDevices,
                                     deviceCapacity,
                                     pDeviceCount,
                                     pBytesRequired);
}

/**
 * @brief 获取目标设备的 platformId
 * @see AdsLiteAPI.h::AdsLiteGetTargetPlatformId
 */
int64_t AdsLiteGetTargetPlatformId(uint16_t port,
                                   const AmsAddr *pAddr,
                                   ADSPLATFORMID *pPlatformId)
{
    return TargetInfo().ReadPlatformId(port, pAddr, pPlatformId);
}

/**
 * @brief 获取目标设备的 systemId
 * @see AdsLiteAPI.h::AdsLiteGetTargetSystemId
 */
int64_t AdsLiteGetTargetSystemId(uint16_t port,
                                 const AmsAddr *pAddr,
                                 char *pSystemId,
                                 uint32_t systemIdBufferLength)
{
    return TargetInfo().ReadSystemId(port, pAddr, pSystemId, systemIdBufferLength);
}

/**
 * @brief 初始化 ADS 路由
 * @see AdsLiteAPI.h::AdsLiteInitRouting
 */
int64_t AdsLiteInitRouting(const char *addr, AmsNetId *ams)
{
    return Backend().InitRouting(addr, ams);
}

/**
 * @brief 关闭 ADS 路由
 * @see AdsLiteAPI.h::AdsLiteShutdownRouting
 */
void AdsLiteShutdownRouting(AmsNetId *ams)
{
    TargetInfo().Clear();
    Backend().ShutdownRouting(ams);
}

/**
 * @brief 设置本地 AMS 地址
 * @see AdsLiteAPI.h::AdsLiteSetLocalAddress
 */
void AdsLiteSetLocalAddress(const char *addr)
{
    Backend().SetLocalAddress(addr);
}

/**
 * @brief 获取本地 AMS 地址
 * @see AdsLiteAPI.h::AdsLiteGetLocalAddress
 */
int64_t AdsLiteGetLocalAddress(uint16_t port, AmsAddr *pAddr)
{
    return Backend().GetLocalAddress(port, pAddr);
}

/**
 * @brief 打开 ADS 端口
 * @see AdsLiteAPI.h::AdsLitePortOpen
 */
uint16_t AdsLitePortOpen()
{
    return Backend().PortOpen();
}

/**
 * @brief 关闭 ADS 端口
 * @see AdsLiteAPI.h::AdsLitePortClose
 */
int64_t AdsLitePortClose(uint16_t port)
{
    return Backend().PortClose(port);
}

/**
 * @brief 设置同步操作超时时间
 * @see AdsLiteAPI.h::AdsLiteSyncSetTimeout
 */
int64_t AdsLiteSyncSetTimeout(uint16_t port, uint32_t timeout)
{
    return Backend().SyncSetTimeout(port, timeout);
}

/**
 * @brief 获取同步操作超时时间
 * @see AdsLiteAPI.h::AdsLiteSyncGetTimeout
 */
int64_t AdsLiteSyncGetTimeout(uint16_t port, uint32_t *pTimeout)
{
    return Backend().SyncGetTimeout(port, pTimeout);
}

/**
 * @brief 读取数据
 * @see AdsLiteAPI.h::AdsLiteSyncReadReq
 */
int64_t AdsLiteSyncReadReq(uint16_t port,
                           const AmsAddr *pAddr,
                           uint32_t indexGroup,
                           uint32_t indexOffset,
                           uint32_t length,
                           void *pData,
                           uint32_t *pBytesRead)
{
    return Backend().SyncReadReq(port, pAddr, indexGroup, indexOffset, length, pData, pBytesRead);
}

/**
 * @brief 写入数据
 * @see AdsLiteAPI.h::AdsLiteSyncWriteReq
 */
int64_t AdsLiteSyncWriteReq(uint16_t port,
                            const AmsAddr *pAddr,
                            uint32_t indexGroup,
                            uint32_t indexOffset,
                            uint32_t length,
                            const void *pBuffer)
{
    return Backend().SyncWriteReq(port, pAddr, indexGroup, indexOffset, length, pBuffer);
}

/**
 * @brief 读写数据
 * @see AdsLiteAPI.h::AdsLiteSyncReadWriteReq
 */
int64_t AdsLiteSyncReadWriteReq(uint16_t port,
                                const AmsAddr *pAddr,
                                uint32_t indexGroup,
                                uint32_t indexOffset,
                                uint32_t readLength,
                                void *pReadData,
                                uint32_t writeLength,
                                const void *pWriteData,
                                uint32_t *pBytesRead)
{
    return Backend().SyncReadWriteReq(port,
                                      pAddr,
                                      indexGroup,
                                      indexOffset,
                                      readLength,
                                      pReadData,
                                      writeLength,
                                      pWriteData,
                                      pBytesRead);
}

/**
 * @brief 读取设备状态
 * @see AdsLiteAPI.h::AdsLiteSyncReadStateReq
 */
int64_t AdsLiteSyncReadStateReq(uint16_t port,
                                const AmsAddr *pAddr,
                                uint16_t *pAdsState,
                                uint16_t *pDeviceState)
{
    return Backend().SyncReadStateReq(port, pAddr, pAdsState, pDeviceState);
}

/**
 * @brief 写入控制命令
 * @see AdsLiteAPI.h::AdsLiteSyncWriteControlReq
 */
int64_t AdsLiteSyncWriteControlReq(uint16_t port,
                                   const AmsAddr *pAddr,
                                   uint16_t adsState,
                                   uint16_t deviceState,
                                   uint32_t length,
                                   const void *pData)
{
    return Backend().SyncWriteControlReq(port, pAddr, adsState, deviceState, length, pData);
}

// =========================================================================
// 文件服务与程序更新辅助 API
// =========================================================================

int64_t AdsLiteFileOpen(uint16_t port,
                        const AmsAddr *pAddr,
                        const char *remotePath,
                        uint32_t openFlags,
                        uint32_t *pFileHandle)
{
    return FileService().FileOpen(port, pAddr, remotePath, openFlags, pFileHandle);
}

int64_t AdsLiteFileClose(uint16_t port,
                         const AmsAddr *pAddr,
                         uint32_t fileHandle)
{
    return FileService().FileClose(port, pAddr, fileHandle);
}

int64_t AdsLiteFileRead(uint16_t port,
                        const AmsAddr *pAddr,
                        uint32_t fileHandle,
                        uint32_t length,
                        void *pData,
                        uint32_t *pBytesRead)
{
    return FileService().FileRead(port, pAddr, fileHandle, length, pData, pBytesRead);
}

int64_t AdsLiteFileWrite(uint16_t port,
                         const AmsAddr *pAddr,
                         uint32_t fileHandle,
                         const void *pData,
                         uint32_t length)
{
    return FileService().FileWrite(port, pAddr, fileHandle, pData, length);
}

int64_t AdsLiteFileDelete(uint16_t port,
                          const AmsAddr *pAddr,
                          const char *remotePath)
{
    return FileService().FileDelete(port, pAddr, remotePath, ADSLITE_FOPEN_READ);
}

int64_t AdsLiteDirCreate(uint16_t port,
                         const AmsAddr *pAddr,
                         const char *remoteDirPath)
{
    return FileService().DirCreate(port, pAddr, remoteDirPath);
}

int64_t AdsLiteDirDelete(uint16_t port,
                         const AmsAddr *pAddr,
                         const char *remoteDirPath,
                         bool deleteDirSelf)
{
    return FileService().DirDelete(port, pAddr, remoteDirPath, deleteDirSelf);
}

int64_t AdsLiteFileRename(uint16_t port,
                          const AmsAddr *pAddr,
                          const char *sourcePath,
                          const char *targetPath)
{
    return FileService().FileRename(port, pAddr, sourcePath, targetPath);
}

int64_t AdsLiteFileList(uint16_t port,
                        const AmsAddr *pAddr,
                        const char *pathPattern,
                        char *pNameBuffer,
                        uint32_t nameBufferLength,
                        uint32_t *pBytesRequired,
                        uint32_t *pItemCount)
{
    return FileService().FileList(port,
                                  pAddr,
                                  pathPattern,
                                  ADSLITE_FOPEN_READ,
                                  pNameBuffer,
                                  nameBufferLength,
                                  pBytesRequired,
                                  pItemCount);
}

// =========================================================================
// 异步操作 API
// =========================================================================

/**
 * @brief 异步读取数据
 * @see AdsLiteAPI.h::AdsLiteAsyncReadReq
 */
int64_t AdsLiteAsyncReadReq(uint16_t port,
                            const AmsAddr *pAddr,
                            uint32_t indexGroup,
                            uint32_t indexOffset,
                            uint32_t length,
                            void *pData,
                            uint32_t *pBytesRead,
                            uint32_t *pInvokeId)
{
    // 异步读取使用同步 API + invokeId 机制
    // 注意：由于底层 AdsLiteLib 主要是同步设计，
    // 这里提供一个简化的异步接口，实际实现需要更复杂的 AmsConnection 支持
    // 当前实现使用同步调用，返回同步结果

    // TODO: 实现真正的异步支持需要修改 AmsConnection 以支持非阻塞调用
    // 临时方案：使用同步调用
    if (pInvokeId)
    {
        *pInvokeId = 0; // 同步调用，无实际 invokeId
    }
    return Backend().SyncReadReq(port, pAddr, indexGroup, indexOffset, length, pData, pBytesRead);
}

/**
 * @brief 异步写入数据
 * @see AdsLiteAPI.h::AdsLiteAsyncWriteReq
 */
int64_t AdsLiteAsyncWriteReq(uint16_t port,
                             const AmsAddr *pAddr,
                             uint32_t indexGroup,
                             uint32_t indexOffset,
                             uint32_t length,
                             const void *pBuffer,
                             uint32_t *pInvokeId)
{
    if (pInvokeId)
    {
        *pInvokeId = 0;
    }
    return Backend().SyncWriteReq(port, pAddr, indexGroup, indexOffset, length, pBuffer);
}

/**
 * @brief 异步读写数据
 * @see AdsLiteAPI.h::AdsLiteAsyncReadWriteReq
 */
int64_t AdsLiteAsyncReadWriteReq(uint16_t port,
                                 const AmsAddr *pAddr,
                                 uint32_t indexGroup,
                                 uint32_t indexOffset,
                                 uint32_t readLength,
                                 void *pReadData,
                                 uint32_t writeLength,
                                 const void *pWriteData,
                                 uint32_t *pBytesRead,
                                 uint32_t *pInvokeId)
{
    if (pInvokeId)
    {
        *pInvokeId = 0;
    }
    return Backend().SyncReadWriteReq(port,
                                      pAddr,
                                      indexGroup,
                                      indexOffset,
                                      readLength,
                                      pReadData,
                                      writeLength,
                                      pWriteData,
                                      pBytesRead);
}

/**
 * @brief 等待异步操作完成
 * @see AdsLiteAPI.h::AdsLiteAsyncWait
 */
int64_t AdsLiteAsyncWait(uint16_t port, uint32_t invokeId, uint32_t timeoutMs)
{
    // 同步调用无需等待
    if (invokeId == 0)
    {
        return 0;
    }
    // TODO: 实现真正的异步等待机制
    return 0;
}

// =========================================================================
// 设备通知 API
// =========================================================================

/**
 * @brief 添加设备通知
 * @see AdsLiteAPI.h::AdsLiteAddNotification
 *
 * @note 当前 AdsLiteLib 未完全暴露通知功能，此为占位实现
 */
int64_t AdsLiteAddNotification(uint16_t port,
                               const AmsAddr *pAddr,
                               uint32_t indexGroup,
                               uint32_t indexOffset,
                               uint32_t length,
                               const AdsNotificationAttrib *attr,
                               PAdsNotificationFuncEx callback,
                               uint32_t hUser,
                               uint32_t *pNotificationHandle)
{
    // TODO: 通知功能需要更深入的 AmsConnection/AmsPort 集成
    // 临时返回错误
    return ADSERR_DEVICE_SRVNOTSUPP;
}

/**
 * @brief 删除设备通知
 * @see AdsLiteAPI.h::AdsLiteDelNotification
 */
int64_t AdsLiteDelNotification(uint16_t port,
                               const AmsAddr *pAddr,
                               uint32_t notificationHandle)
{
    return ADSERR_DEVICE_SRVNOTSUPP;
}

// =========================================================================
// 符号名/句柄读写 API
// =========================================================================

/**
 * @brief 通过变量名获取符号句柄
 * @see AdsLiteAPI.h::AdsLiteGetSymbolHandle
 *
 * 使用 ADSIGRP_SYM_HNDBYNAME (0xF003) 获取句柄
 */
int64_t AdsLiteGetSymbolHandle(uint16_t port,
                               const AmsAddr *pAddr,
                               const char *symbolName,
                               uint32_t *pHandle)
{
    if (!symbolName || !pHandle)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    // 构建请求：符号名 + null 终止符
    size_t nameLen = strlen(symbolName);
    if (nameLen >= ADS_FIXEDNAMESIZE)
    {
        return ADSERR_DEVICE_INVALIDDATA;
    }

    // 发送读取句柄请求
    return Backend().SyncReadWriteReq(
        port,
        pAddr,
        ADSIGRP_SYM_HNDBYNAME, // Index Group: Get Handle by Name
        0,                     // Index Offset
        sizeof(uint32_t),      // Read Length: handle (4 bytes)
        pHandle,               // Read Buffer
        nameLen + 1,           // Write Length: symbol name + null
        symbolName,            // Write Buffer: symbol name
        nullptr                // Bytes Read (optional)
    );
}

/**
 * @brief 释放符号句柄
 * @see AdsLiteAPI.h::AdsLiteReleaseSymbolHandle
 *
 * 使用 ADSIGRP_SYM_RELEASEHND (0xF006) 释放句柄
 */
int64_t AdsLiteReleaseSymbolHandle(uint16_t port,
                                   const AmsAddr *pAddr,
                                   uint32_t handle)
{
    return Backend().SyncWriteReq(
        port, pAddr,
        ADSIGRP_SYM_RELEASEHND, // Index Group: Release Handle
        0,                      // Index Offset
        sizeof(handle),         // Length
        &handle                 // Buffer
    );
}

/**
 * @brief 通过变量名读取数据（自动句柄管理）
 * @see AdsLiteAPI.h::AdsLiteReadByName
 */
int64_t AdsLiteReadByName(uint16_t port,
                          const AmsAddr *pAddr,
                          const char *symbolName,
                          void *pData,
                          uint32_t length,
                          uint32_t *pBytesRead)
{
    if (!symbolName || !pData)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    uint32_t handle = 0;
    int64_t result;

    // 第一步：获取句柄
    result = AdsLiteGetSymbolHandle(port, pAddr, symbolName, &handle);
    if (result != 0)
    {
        return result;
    }

    // 第二步：使用句柄读取数据
    result = AdsLiteReadByHandle(port, pAddr, handle, pData, length, pBytesRead);

    // 第三步：释放句柄
    AdsLiteReleaseSymbolHandle(port, pAddr, handle);

    return result;
}

/**
 * @brief 通过变量名写入数据
 * @see AdsLiteAPI.h::AdsLiteWriteByName
 */
int64_t AdsLiteWriteByName(uint16_t port,
                           const AmsAddr *pAddr,
                           const char *symbolName,
                           const void *pData,
                           uint32_t length)
{
    if (!symbolName || !pData)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    uint32_t handle = 0;
    int64_t result;

    // 第一步：获取句柄
    result = AdsLiteGetSymbolHandle(port, pAddr, symbolName, &handle);
    if (result != 0)
    {
        return result;
    }

    // 第二步：使用句柄写入数据
    result = AdsLiteWriteByHandle(port, pAddr, handle, pData, length);

    // 第三步：释放句柄
    AdsLiteReleaseSymbolHandle(port, pAddr, handle);

    return result;
}

/**
 * @brief 通过句柄读取数据
 * @see AdsLiteAPI.h::AdsLiteReadByHandle
 *
 * 使用 ADSIGRP_SYM_VALBYHND (0xF005) 读取数据
 */
int64_t AdsLiteReadByHandle(uint16_t port,
                            const AmsAddr *pAddr,
                            uint32_t handle,
                            void *pData,
                            uint32_t length,
                            uint32_t *pBytesRead)
{
    if (!pData)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    return Backend().SyncReadReq(
        port, pAddr,
        ADSIGRP_SYM_VALBYHND, // Index Group: Read by Handle
        handle,               // Index Offset: 使用 handle 作为偏移
        length,               // Length
        pData,                // Buffer
        pBytesRead            // Bytes Read
    );
}

/**
 * @brief 通过句柄写入数据
 * @see AdsLiteAPI.h::AdsLiteWriteByHandle
 *
 * 通过写入句柄值来写入数据
 */
int64_t AdsLiteWriteByHandle(uint16_t port,
                             const AmsAddr *pAddr,
                             uint32_t handle,
                             const void *pData,
                             uint32_t length)
{
    if (!pData)
    {
        return ADSERR_CLIENT_INVALIDPARM;
    }

    // 构建写入数据：handle + 数据
    // 写入到 ADSIGRP_SYM_VALBYHND (0xF005)
    return Backend().SyncWriteReq(
        port, pAddr,
        ADSIGRP_SYM_VALBYHND, // Index Group
        handle,               // Index Offset: handle
        length,               // Length
        pData                 // Buffer
    );
}
