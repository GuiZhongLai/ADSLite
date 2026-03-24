/**
 * @file AdsLiteAPI.h
 * @brief AdsLite 高级 ADS API 接口
 *
 * 本文件提供了一套简化的 C 风格 API，用于与 TwinCAT ADS 设备进行通信。
 * API 设计遵循以下原则：
 * - 简单的初始化流程
 * - 统一错误码返回
 * - 清晰的资源管理
 *
 * @note 使用前请先调用 AdsLiteInitRouting() 初始化路由
 * @note 不再需要通信时调用 AdsLiteShutdownRouting() 清理资源
 */

#ifndef ADS_LITE_API_H
#define ADS_LITE_API_H

#include "AdsLiteDef.h"

#include <stddef.h>

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

    /**
     * @brief 获取目标设备的 AMS NetId
     *
     * 通过 UDP 广播查询获取目标设备的 AMS NetId。
     * 这是初始化路由前的第一步操作。
     *
     * @param[in] addr 目标设备的 IP 地址或主机名
     * @param[out] ams 指向 AmsNetId 结构体的指针，成功时将填充目标设备的 NetId
     * @return 返回错误码，0 表示成功
     *         - ADSERR_NOERR (0x00): 成功
     *         - 其他值: 参见 ADS Return Codes
     */
    ADS_LITE_API int64_t AdsLiteGetDeviceNetId(const char *addr, AmsNetId *ams);

    /**
     * @brief 获取目标设备 SystemId（GUID 字符串）
     *
     * 通过固定索引组读取 16 字节 SystemId，并格式化为
     * "XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX"。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[out] pSystemId 输出字符串缓冲区
     * @param[in] systemIdBufferLength 缓冲区长度，至少 37 字节
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteGetSystemId(uint16_t port,
                                            const AmsAddr *pAddr,
                                            char *pSystemId,
                                            uint32_t systemIdBufferLength);

    /**
     * @brief 初始化 ADS 路由
     *
     * 完整的路由初始化流程：
     * 1. 获取目标设备的 AMS NetId
     * 2. 确定本地 IP 地址
     * 3. 配置本地路由表
     * 4. 配置远程路由表
     *
     * @param[in] addr 目标设备的 IP 地址或主机名
     * @param[out] ams 指向 AmsNetId 结构体的指针
     *                    输入时：指定目标设备的 AMS NetId（可由 AdsLiteGetDeviceNetId 获取）
     *                    输出时：成功初始化后保持不变
     * @return 返回错误码，0 表示成功
     *         - ADSERR_NOERR (0x00): 成功
     *         - ADSERR_CLIENT_ERROR (0x740): 本地 IP 不匹配
     *         - 其他值: 参见 ADS Return Codes
     *
     * @note 调用此函数前，可以先调用 AdsLiteGetDeviceNetId 获取设备的 NetId
     * @note 不再需要通信时，必须调用 AdsLiteShutdownRouting() 清理资源
     *
     * @code
     * // 使用示例
     * AmsNetId targetAms;
     * int64_t result = AdsLiteInitRouting("192.168.1.1", &targetAms);
     * if (result == 0) {
     *     // 初始化成功，可以进行读写操作
     * } else {
     *     // 处理错误
     * }
     * @endcode
     */
    ADS_LITE_API int64_t AdsLiteInitRouting(const char *addr, AmsNetId *ams);

    /**
     * @brief 关闭 ADS 路由
     *
     * 清理路由表资源，包括：
     * - 删除本地路由表项
     * - 通知远程设备删除路由
     *
     * @param[in] ams 目标设备的 AMS NetId（由 AdsLiteInitRouting 使用）
     * @return 无返回值
     *
     * @warning 在程序退出或不再需要与目标设备通信时，必须调用此函数
     *
     * @code
     * // 使用示例
     * AdsLiteShutdownRouting(&targetAms);
     * @endcode
     */
    ADS_LITE_API void AdsLiteShutdownRouting(AmsNetId *ams);

    /**
     * @brief 设置本地 AMS 地址
     *
     * 手动设置本地 AMS NetId。通常在 AdsLiteInitRouting 中自动设置，
     * 但某些场景可能需要手动控制。
     *
     * @param[in] addr 本地 IP 地址或 NetId 字符串（如 "192.168.1.100.1.1"）
     */
    ADS_LITE_API void AdsLiteSetLocalAddress(const char *addr);

    /**
     * @brief 获取本地 AMS 地址
     *
     * 查询指定端口的本地 AMS 地址。
     *
     * @param[in] port 本地端口号（由 AdsLitePortOpen 返回）
     * @param[out] pAddr 指向 AmsAddr 结构体的指针，将填充本地地址信息
     * @return 返回错误码，0 表示成功
     *         - ADSERR_NOERR (0x00): 成功
     *         - ADSERR_CLIENT_PORTNOTOPEN (0x748): 端口未打开
     */
    ADS_LITE_API int64_t AdsLiteGetLocalAddress(uint16_t port, AmsAddr *pAddr);

    /**
     * @brief 打开 ADS 端口
     *
     * 打开一个本地端口用于 ADS 通信。端口号用于标识本地通信端点。
     *
     * @return 成功时返回打开的端口号（范围 30000-32767）
     *         失败时返回 0
     *
     * @note 每个线程应使用独立的端口
     * @note 使用完毕后必须调用 AdsLitePortClose() 关闭端口
     *
     * @code
     * // 使用示例
     * uint16_t port = AdsLitePortOpen();
     * if (port != 0) {
     *     // 端口打开成功
     * }
     * @endcode
     */
    ADS_LITE_API uint16_t AdsLitePortOpen();

    /**
     * @brief 关闭 ADS 端口
     *
     * 关闭之前打开的端口，释放相关资源。
     *
     * @param[in] port 要关闭的端口号
     * @return 返回错误码，0 表示成功
     *         - ADSERR_NOERR (0x00): 成功
     *         - ADSERR_CLIENT_PORTNOTOPEN (0x748): 端口未打开
     */
    ADS_LITE_API int64_t AdsLitePortClose(uint16_t port);

    /**
     * @brief 设置同步操作超时时间
     *
     * 设置指定端口上同步 ADS 操作的默认超时时间。
     *
     * @param[in] port 本地端口号
     * @param[in] timeout 超时时间（毫秒），建议范围 100-5000
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteSyncSetTimeout(uint16_t port, uint32_t timeout);

    /**
     * @brief 获取同步操作超时时间
     *
     * 查询指定端口上同步 ADS 操作的默认超时时间。
     *
     * @param[in] port 本地端口号
     * @param[out] pTimeout 指向超时时间变量的指针（毫秒）
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteSyncGetTimeout(uint16_t port, uint32_t *pTimeout);

    /**
     * @brief 读取数据
     *
     * 从 ADS 设备的指定索引组/偏移位置同步读取数据。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] indexGroup 索引组（Index Group）
     * @param[in] indexOffset 索引偏移（Index Offset）
     * @param[in] length 要读取的字节数
     * @param[out] pData 读取数据的缓冲区
     * @param[out] pBytesRead 实际读取的字节数（可为 nullptr）
     * @return 返回错误码，0 表示成功
     *         - ADSERR_NOERR (0x00): 成功
     *         - ADSERR_DEVICE_INVALIDSIZE (0x705): 长度参数无效
     *         - 其他值: 参见 ADS Return Codes
     */
    ADS_LITE_API int64_t AdsLiteSyncReadReq(uint16_t port,
                                            const AmsAddr *pAddr,
                                            uint32_t indexGroup,
                                            uint32_t indexOffset,
                                            uint32_t length,
                                            void *pData,
                                            uint32_t *pBytesRead);

    /**
     * @brief 写入数据
     *
     * 向 ADS 设备的指定索引组/偏移位置同步写入数据。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] indexGroup 索引组（Index Group）
     * @param[in] indexOffset 索引偏移（Index Offset）
     * @param[in] length 要写入的字节数
     * @param[in] pBuffer 要写入的数据缓冲区
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteSyncWriteReq(uint16_t port,
                                             const AmsAddr *pAddr,
                                             uint32_t indexGroup,
                                             uint32_t indexOffset,
                                             uint32_t length,
                                             const void *pBuffer);

    /**
     * @brief 读写数据
     *
     * 向 ADS 设备写入数据并同时读取响应数据。
     * 适用于需要读取写入结果的场景。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] indexGroup 索引组（Index Group）
     * @param[in] indexOffset 索引偏移（Index Offset）
     * @param[in] readLength 要读取的字节数
     * @param[out] pReadData 读取数据的缓冲区
     * @param[in] writeLength 要写入的字节数
     * @param[in] pWriteData 要写入的数据缓冲区
     * @param[out] pBytesRead 实际读取的字节数（可为 nullptr）
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteSyncReadWriteReq(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 uint32_t indexGroup,
                                                 uint32_t indexOffset,
                                                 uint32_t readLength,
                                                 void *pReadData,
                                                 uint32_t writeLength,
                                                 const void *pWriteData,
                                                 uint32_t *pBytesRead);

    /**
     * @brief 读取设备状态
     *
     * 查询 ADS 设备的当前状态。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[out] pAdsState 指向 ADS 状态的指针（ADSSTATE 枚举）
     * @param[out] pDeviceState 指向设备状态的指针
     * @return 返回错误码，0 表示成功
     *
     * @see ADSSTATE 枚举定义
     */
    ADS_LITE_API int64_t AdsLiteSyncReadStateReq(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 uint16_t *pAdsState,
                                                 uint16_t *pDeviceState);

    /**
     * @brief 写入控制命令
     *
     * 向 ADS 设备写入控制命令和可选数据。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] adsState 要设置的 ADS 状态
     * @param[in] deviceState 要设置的设备状态
     * @param[in] length 数据长度（字节）
     * @param[in] pData 控制数据缓冲区（可为 nullptr）
     * @return 返回错误码，0 表示成功
     *
     * @see ADSSTATE 枚举定义
     */
    ADS_LITE_API int64_t AdsLiteSyncWriteControlReq(uint16_t port,
                                                    const AmsAddr *pAddr,
                                                    uint16_t adsState,
                                                    uint16_t deviceState,
                                                    uint32_t length,
                                                    const void *pData);

    // =========================================================================
    // 文件服务与程序更新辅助 API
    // =========================================================================

    /**
     * @brief 打开目标设备上的文件
     *
     * 通过 ADS 系统服务打开远端文件并返回文件句柄。
        * 打开标志位定义见 AdsLiteDef.h::AdsLiteFileOpenFlags。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] remotePath 远端文件路径
     * @param[in] openFlags 打开标志，建议使用 AdsLiteFileOpenFlags 组合
     *                    例如覆盖写入使用：
     *                    ADSLITE_FOPEN_WRITE | ADSLITE_FOPEN_BINARY |
     *                    ADSLITE_FOPEN_PLUS | ADSLITE_FOPEN_ENSURE_DIR
     * @param[out] pFileHandle 文件句柄
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteFileOpen(uint16_t port,
                                         const AmsAddr *pAddr,
                                         const char *remotePath,
                                         uint32_t openFlags,
                                         uint32_t *pFileHandle);

    /**
     * @brief 关闭目标设备上的文件句柄
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] fileHandle 文件句柄
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteFileClose(uint16_t port,
                                          const AmsAddr *pAddr,
                                          uint32_t fileHandle);

    /**
     * @brief 读取目标设备文件内容
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] fileHandle 文件句柄
     * @param[in] length 读取字节数
     * @param[out] pData 读取缓冲区
     * @param[out] pBytesRead 实际读取字节数（可为 nullptr）
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteFileRead(uint16_t port,
                                         const AmsAddr *pAddr,
                                         uint32_t fileHandle,
                                         uint32_t length,
                                         void *pData,
                                         uint32_t *pBytesRead);

    /**
     * @brief 向目标设备文件写入内容
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] fileHandle 文件句柄
     * @param[in] pData 待写入数据
     * @param[in] length 写入字节数
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteFileWrite(uint16_t port,
                                          const AmsAddr *pAddr,
                                          uint32_t fileHandle,
                                          const void *pData,
                                          uint32_t length);

    /**
     * @brief 删除目标设备上的文件
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] remotePath 远端文件路径
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteFileDelete(uint16_t port,
                                           const AmsAddr *pAddr,
                                           const char *remotePath);

    /**
     * @brief 创建目标设备上的目录
     *
     * 该接口通过文件服务保证目录存在，适用于部署前预创建目录。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] remoteDirPath 远端目录路径
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteDirCreate(uint16_t port,
                                          const AmsAddr *pAddr,
                                          const char *remoteDirPath);

    /**
     * @brief 删除目标设备上的目录内容，并可选删除目录本身
     *
     * 内部先递归删除目录下所有文件与子目录，再根据 deleteDirSelf
     * 决定是否删除 remoteDirPath 本身。目录不存在返回成功。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] remoteDirPath 远端目录路径
     * @param[in] deleteDirSelf true=删除目录本身，false=仅清空目录内容
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteDirDelete(uint16_t port,
                                          const AmsAddr *pAddr,
                                          const char *remoteDirPath,
                                          bool deleteDirSelf);

    /**
     * @brief 重命名目标设备上的文件
     *
     * 当前实现采用“复制到新路径后删除旧文件”的语义。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] sourcePath 原始文件路径
     * @param[in] targetPath 目标文件路径
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteFileRename(uint16_t port,
                                           const AmsAddr *pAddr,
                                           const char *sourcePath,
                                           const char *targetPath);

    /**
     * @brief 获取目录下文件名列表
     *
     * 内部会使用文件服务遍历接口聚合结果。
     * 结果以 '\n' 分隔写入缓冲区。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备 AMS 地址
     * @param[in] pathPattern 路径或通配路径（例如 "C:/TwinCAT/3.1/Boot/*"）
     * @param[out] pNameBuffer 输出缓冲区（可为 nullptr，用于仅查询所需长度）
     * @param[in] nameBufferLength 输出缓冲区长度
     * @param[out] pBytesRequired 需要的总字节数（含末尾 '\0'）
     * @param[out] pItemCount 文件项数量
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteFileList(uint16_t port,
                                         const AmsAddr *pAddr,
                                         const char *pathPattern,
                                         char *pNameBuffer,
                                         uint32_t nameBufferLength,
                                         uint32_t *pBytesRequired,
                                         uint32_t *pItemCount);

    // =========================================================================
    // 异步操作 API
    // =========================================================================

    /**
     * @brief 异步读取数据
     *
     * 从 ADS 设备异步读取数据。函数立即返回，通过 invokeId 标识请求。
     * 使用 AdsLiteAsyncWait 等待操作完成。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] indexGroup 索引组
     * @param[in] indexOffset 索引偏移
     * @param[in] length 要读取的字节数
     * @param[out] pData 读取数据的缓冲区
     * @param[out] pBytesRead 实际读取的字节数
     * @param[out] pInvokeId 异步调用 ID，用于后续等待结果
     * @return 返回错误码，0 表示请求已提交
     */
    ADS_LITE_API int64_t AdsLiteAsyncReadReq(uint16_t port,
                                             const AmsAddr *pAddr,
                                             uint32_t indexGroup,
                                             uint32_t indexOffset,
                                             uint32_t length,
                                             void *pData,
                                             uint32_t *pBytesRead,
                                             uint32_t *pInvokeId);

    /**
     * @brief 异步写入数据
     *
     * 向 ADS 设备异步写入数据。函数立即返回。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] indexGroup 索引组
     * @param[in] indexOffset 索引偏移
     * @param[in] length 要写入的字节数
     * @param[in] pBuffer 要写入的数据缓冲区
     * @param[out] pInvokeId 异步调用 ID
     * @return 返回错误码，0 表示请求已提交
     */
    ADS_LITE_API int64_t AdsLiteAsyncWriteReq(uint16_t port,
                                              const AmsAddr *pAddr,
                                              uint32_t indexGroup,
                                              uint32_t indexOffset,
                                              uint32_t length,
                                              const void *pBuffer,
                                              uint32_t *pInvokeId);

    /**
     * @brief 异步读写数据
     *
     * 向 ADS 设备异步写入数据并读取响应。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] indexGroup 索引组
     * @param[in] indexOffset 索引偏移
     * @param[in] readLength 要读取的字节数
     * @param[out] pReadData 读取数据的缓冲区
     * @param[in] writeLength 要写入的字节数
     * @param[in] pWriteData 要写入的数据缓冲区
     * @param[out] pBytesRead 实际读取的字节数
     * @param[out] pInvokeId 异步调用 ID
     * @return 返回错误码，0 表示请求已提交
     */
    ADS_LITE_API int64_t AdsLiteAsyncReadWriteReq(uint16_t port,
                                                  const AmsAddr *pAddr,
                                                  uint32_t indexGroup,
                                                  uint32_t indexOffset,
                                                  uint32_t readLength,
                                                  void *pReadData,
                                                  uint32_t writeLength,
                                                  const void *pWriteData,
                                                  uint32_t *pBytesRead,
                                                  uint32_t *pInvokeId);

    /**
     * @brief 等待异步操作完成
     *
     * 等待之前提交的异步操作完成。
     *
     * @param[in] port 本地端口号
     * @param[in] invokeId 异步调用 ID（由异步函数返回）
     * @param[in] timeoutMs 超时时间（毫秒）
     * @return 返回错误码，0 表示操作成功完成
     *         - ADSERR_CLIENT_SYNCTIMEOUT (0x745): 超时
     */
    ADS_LITE_API int64_t AdsLiteAsyncWait(uint16_t port, uint32_t invokeId, uint32_t timeoutMs);

    // =========================================================================
    // 设备通知 API
    // =========================================================================

    /**
     * @brief 添加设备通知
     *
     * 注册一个设备通知，当指定索引组/偏移的数据发生变化时，
     * 回调函数会被调用。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] indexGroup 索引组
     * @param[in] indexOffset 索引偏移
     * @param[in] length 数据长度
     * @param[in] attr 通知属性（传输模式、最大延迟、循环时间）
     * @param[in] callback 回调函数
     * @param[in] hUser 用户句柄，将传递给回调函数
     * @param[out] pNotificationHandle 通知句柄，用于删除通知
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteAddNotification(uint16_t port,
                                                const AmsAddr *pAddr,
                                                uint32_t indexGroup,
                                                uint32_t indexOffset,
                                                uint32_t length,
                                                const AdsNotificationAttrib *attr,
                                                PAdsNotificationFuncEx callback,
                                                uint32_t hUser,
                                                uint32_t *pNotificationHandle);

    /**
     * @brief 删除设备通知
     *
     * 注销之前注册的通知。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] notificationHandle 通知句柄
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteDelNotification(uint16_t port,
                                                const AmsAddr *pAddr,
                                                uint32_t notificationHandle);

    // =========================================================================
    // 符号名/句柄读写 API
    // =========================================================================

    /**
     * @brief 通过变量名获取符号句柄
     *
     * 根据 PLC 变量名获取句柄，后续操作可使用此句柄。
     * 使用完毕后需要调用 AdsLiteReleaseSymbolHandle 释放句柄。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] symbolName 变量名（如 "Main.plcTask.Instance"、"GVL.bVariable"）
     * @param[out] pHandle 符号句柄
     * @return 返回错误码，0 表示成功
     *
     * @note 使用 AdsLiteReadByName 和 AdsLiteWriteByName 可以自动管理句柄
     */
    ADS_LITE_API int64_t AdsLiteGetSymbolHandle(uint16_t port,
                                                const AmsAddr *pAddr,
                                                const char *symbolName,
                                                uint32_t *pHandle);

    /**
     * @brief 释放符号句柄
     *
     * 释放之前获取的符号句柄。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] handle 符号句柄
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteReleaseSymbolHandle(uint16_t port,
                                                    const AmsAddr *pAddr,
                                                    uint32_t handle);

    /**
     * @brief 通过变量名读取数据（自动句柄管理）
     *
     * 根据变量名自动获取句柄并读取数据，函数内部自动管理句柄生命周期。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] symbolName 变量名
     * @param[out] pData 读取数据的缓冲区
     * @param[in] length 缓冲区长度（应不小于变量实际大小）
     * @param[out] pBytesRead 实际读取的字节数
     * @return 返回错误码，0 表示成功
     *
     * @code
     * // 读取一个 BOOL 变量
     * bool value;
     * uint32_t bytesRead;
     * AdsLiteReadByName(port, &addr, "GVL.bVariable", &value, sizeof(value), &bytesRead);
     *
     * // 读取一个 INT 变量
     * int16_t value;
     * uint32_t bytesRead;
     * AdsLiteReadByName(port, &addr, "GVL.nCounter", &value, sizeof(value), &bytesRead);
     * @endcode
     */
    ADS_LITE_API int64_t AdsLiteReadByName(uint16_t port,
                                           const AmsAddr *pAddr,
                                           const char *symbolName,
                                           void *pData,
                                           uint32_t length,
                                           uint32_t *pBytesRead);

    /**
     * @brief 通过变量名写入数据
     *
     * 根据变量名写入数据，自动管理句柄。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] symbolName 变量名
     * @param[in] pData 要写入的数据
     * @param[in] length 数据长度
     * @return 返回错误码，0 表示成功
     *
     * @code
     * // 写入一个 BOOL 变量
     * bool value = true;
     * AdsLiteWriteByName(port, &addr, "GVL.bVariable", &value, sizeof(value));
     * @endcode
     */
    ADS_LITE_API int64_t AdsLiteWriteByName(uint16_t port,
                                            const AmsAddr *pAddr,
                                            const char *symbolName,
                                            const void *pData,
                                            uint32_t length);

    /**
     * @brief 通过句柄读取数据
     *
     * 使用已获取的句柄读取数据。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] handle 符号句柄
     * @param[out] pData 读取数据的缓冲区
     * @param[in] length 缓冲区长度
     * @param[out] pBytesRead 实际读取的字节数
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteReadByHandle(uint16_t port,
                                             const AmsAddr *pAddr,
                                             uint32_t handle,
                                             void *pData,
                                             uint32_t length,
                                             uint32_t *pBytesRead);

    /**
     * @brief 通过句柄写入数据
     *
     * 使用已获取的句柄写入数据。
     *
     * @param[in] port 本地端口号
     * @param[in] pAddr 目标设备的 AMS 地址
     * @param[in] handle 符号句柄
     * @param[in] pData 要写入的数据
     * @param[in] length 数据长度
     * @return 返回错误码，0 表示成功
     */
    ADS_LITE_API int64_t AdsLiteWriteByHandle(uint16_t port,
                                              const AmsAddr *pAddr,
                                              uint32_t handle,
                                              const void *pData,
                                              uint32_t length);

#ifdef __cplusplus
}
#endif

#endif // ADS_LITE_API_H
