/**
 * @file AdsLiteDef.h
 * @brief AdsLite 核心类型定义
 *
 * 本文件包含 ADS 协议的核心定义，包括：
 * - 常量定义（端口号、命令 ID、索引组、返回码）
 * - 数据结构定义（地址、版本、通知）
 * - 回调函数类型定义
 *
 * @note 所有使用 AdsLite 库的文件都应包含此头文件
 */

#ifndef ADS_LITE_DEF_H
#define ADS_LITE_DEF_H

#include <cstdint>

#ifndef ANYSIZE_ARRAY
#define ANYSIZE_ARRAY 1
#endif

#define ADS_FIXEDNAMESIZE 256

#ifdef __cplusplus
extern "C"
{
#endif

    // =========================================================================
    // AMS 端口常量
    // =========================================================================

#define AMSPORT_LOGGER 100
#define AMSPORT_R0_RTIME 200
#define AMSPORT_R0_TRACE (AMSPORT_R0_RTIME + 90)
#define AMSPORT_R0_IO 300
#define AMSPORT_R0_SPS 400
#define AMSPORT_R0_NC 500
#define AMSPORT_R0_ISG 550
#define AMSPORT_R0_PCS 600
#define AMSPORT_R0_PLC 801
#define AMSPORT_R0_PLC_RTS1 801
#define AMSPORT_R0_PLC_RTS2 811
#define AMSPORT_R0_PLC_RTS3 821
#define AMSPORT_R0_PLC_RTS4 831
#define AMSPORT_R0_PLC_TC3 851

    // =========================================================================
    // ADS 命令 ID
    // =========================================================================

#define ADSSRVID_INVALID 0x00
#define ADSSRVID_READDEVICEINFO 0x01
#define ADSSRVID_READ 0x02
#define ADSSRVID_WRITE 0x03
#define ADSSRVID_READSTATE 0x04
#define ADSSRVID_WRITECTRL 0x05
#define ADSSRVID_ADDDEVICENOTE 0x06
#define ADSSRVID_DELDEVICENOTE 0x07
#define ADSSRVID_DEVICENOTE 0x08
#define ADSSRVID_READWRITE 0x09

    // =========================================================================
    // ADS 保留索引组
    // =========================================================================

#define ADSIGRP_SYMTAB 0xF000
#define ADSIGRP_SYMNAME 0xF001
#define ADSIGRP_SYMVAL 0xF002

#define ADSIGRP_SYM_HNDBYNAME 0xF003
#define ADSIGRP_SYM_VALBYNAME 0xF004
#define ADSIGRP_SYM_VALBYHND 0xF005
#define ADSIGRP_SYM_RELEASEHND 0xF006
#define ADSIGRP_SYM_INFOBYNAME 0xF007
#define ADSIGRP_SYM_VERSION 0xF008
#define ADSIGRP_SYM_INFOBYNAMEEX 0xF009

#define ADSIGRP_SYM_DOWNLOAD 0xF00A
#define ADSIGRP_SYM_UPLOAD 0xF00B
#define ADSIGRP_SYM_UPLOADINFO 0xF00C
#define ADSIGRP_SYM_DOWNLOAD2 0xF00D
#define ADSIGRP_SYM_DT_UPLOAD 0xF00E
#define ADSIGRP_SYM_UPLOADINFO2 0xF00F

#define ADSIGRP_SYMNOTE 0xF010

#define ADSIGRP_IOIMAGE_RWIB 0xF020
#define ADSIGRP_IOIMAGE_RWIX 0xF021
#define ADSIGRP_IOIMAGE_RISIZE 0xF025
#define ADSIGRP_IOIMAGE_RWOB 0xF030
#define ADSIGRP_IOIMAGE_RWOX 0xF031
#define ADSIGRP_IOIMAGE_ROSIZE 0xF035
#define ADSIGRP_IOIMAGE_CLEARI 0xF040
#define ADSIGRP_IOIMAGE_CLEARO 0xF050
#define ADSIGRP_IOIMAGE_RWIOB 0xF060

#define ADSIGRP_DEVICE_DATA 0xF100
#define ADSIOFFS_DEVDATA_ADSSTATE 0x0000
#define ADSIOFFS_DEVDATA_DEVSTATE 0x0002

    // =========================================================================
    // 全局返回码
    // =========================================================================

#define ERR_GLOBAL 0x0000

#define GLOBALERR_TARGET_PORT (0x06 + ERR_GLOBAL)   // 目标端口不可用
#define GLOBALERR_MISSING_ROUTE (0x07 + ERR_GLOBAL) // 缺少路由
#define GLOBALERR_NO_MEMORY (0x19 + ERR_GLOBAL)     // 内存不足
#define GLOBALERR_TCP_SEND (0x1A + ERR_GLOBAL)      // TCP 发送失败

    // =========================================================================
    // 路由返回码
    // =========================================================================

#define ERR_ROUTER 0x0500

#define ROUTERERR_PORTALREADYINUSE (0x06 + ERR_ROUTER) // 端口已被占用
#define ROUTERERR_NOTREGISTERED (0x07 + ERR_ROUTER)    // 端口或路由未注册
#define ROUTERERR_NOMOREQUEUES (0x08 + ERR_ROUTER)     // 没有更多消息队列可用

    // =========================================================================
    // ADS 返回码
    // =========================================================================

#define ADSERR_NOERR 0x00 // 无错误
#define ERR_ADSERRS 0x0700

#define ADSERR_DEVICE_ERROR (0x00 + ERR_ADSERRS)                // 设备错误类
#define ADSERR_DEVICE_SRVNOTSUPP (0x01 + ERR_ADSERRS)           // 服务不被服务器支持
#define ADSERR_DEVICE_INVALIDGRP (0x02 + ERR_ADSERRS)           // indexGroup 无效
#define ADSERR_DEVICE_INVALIDOFFSET (0x03 + ERR_ADSERRS)        // indexOffset 无效
#define ADSERR_DEVICE_INVALIDACCESS (0x04 + ERR_ADSERRS)        // 不允许读写访问
#define ADSERR_DEVICE_INVALIDSIZE (0x05 + ERR_ADSERRS)          // 参数大小不正确
#define ADSERR_DEVICE_INVALIDDATA (0x06 + ERR_ADSERRS)          // 参数值无效
#define ADSERR_DEVICE_NOTREADY (0x07 + ERR_ADSERRS)             // 设备未处于就绪状态
#define ADSERR_DEVICE_BUSY (0x08 + ERR_ADSERRS)                 // 设备忙
#define ADSERR_DEVICE_INVALIDCONTEXT (0x09 + ERR_ADSERRS)       // 上下文无效（必须在 Windows 上下文中）
#define ADSERR_DEVICE_NOMEMORY (0x0A + ERR_ADSERRS)             // 内存不足
#define ADSERR_DEVICE_INVALIDPARM (0x0B + ERR_ADSERRS)          // 参数值无效
#define ADSERR_DEVICE_NOTFOUND (0x0C + ERR_ADSERRS)             // 未找到对象（文件等）
#define ADSERR_DEVICE_SYNTAX (0x0D + ERR_ADSERRS)               // 命令或文件语法错误
#define ADSERR_DEVICE_INCOMPATIBLE (0x0E + ERR_ADSERRS)         // 对象不匹配
#define ADSERR_DEVICE_EXISTS (0x0F + ERR_ADSERRS)               // 对象已存在
#define ADSERR_DEVICE_SYMBOLNOTFOUND (0x10 + ERR_ADSERRS)       // 符号未找到
#define ADSERR_DEVICE_SYMBOLVERSIONINVALID (0x11 + ERR_ADSERRS) // 符号版本无效
#define ADSERR_DEVICE_INVALIDSTATE (0x12 + ERR_ADSERRS)         // 服务器状态无效
#define ADSERR_DEVICE_TRANSMODENOTSUPP (0x13 + ERR_ADSERRS)     // 不支持该 AdsTransMode
#define ADSERR_DEVICE_NOTIFYHNDINVALID (0x14 + ERR_ADSERRS)     // 通知句柄无效
#define ADSERR_DEVICE_CLIENTUNKNOWN (0x15 + ERR_ADSERRS)        // 通知客户端未注册
#define ADSERR_DEVICE_NOMOREHDLS (0x16 + ERR_ADSERRS)           // 没有更多通知句柄可用
#define ADSERR_DEVICE_INVALIDWATCHSIZE (0x17 + ERR_ADSERRS)     // 监视区尺寸过大
#define ADSERR_DEVICE_NOTINIT (0x18 + ERR_ADSERRS)              // 设备未初始化
#define ADSERR_DEVICE_TIMEOUT (0x19 + ERR_ADSERRS)              // 设备超时
#define ADSERR_DEVICE_NOINTERFACE (0x1A + ERR_ADSERRS)          // 查询接口失败
#define ADSERR_DEVICE_INVALIDINTERFACE (0x1B + ERR_ADSERRS)     // 请求的接口错误
#define ADSERR_DEVICE_INVALIDCLSID (0x1C + ERR_ADSERRS)         // 类 ID 无效
#define ADSERR_DEVICE_INVALIDOBJID (0x1D + ERR_ADSERRS)         // 对象 ID 无效
#define ADSERR_DEVICE_PENDING (0x1E + ERR_ADSERRS)              // 请求挂起中
#define ADSERR_DEVICE_ABORTED (0x1F + ERR_ADSERRS)              // 请求已中止
#define ADSERR_DEVICE_WARNING (0x20 + ERR_ADSERRS)              // 警告信号
#define ADSERR_DEVICE_INVALIDARRAYIDX (0x21 + ERR_ADSERRS)      // 数组索引无效
#define ADSERR_DEVICE_SYMBOLNOTACTIVE (0x22 + ERR_ADSERRS)      // 符号未激活，应释放句柄后重试
#define ADSERR_DEVICE_ACCESSDENIED (0x23 + ERR_ADSERRS)         // 访问被拒绝
#define ADSERR_DEVICE_LICENSENOTFOUND (0x24 + ERR_ADSERRS)      // 未找到许可证
#define ADSERR_DEVICE_LICENSEEXPIRED (0x25 + ERR_ADSERRS)       // 许可证已过期
#define ADSERR_DEVICE_LICENSEEXCEEDED (0x26 + ERR_ADSERRS)      // 许可证额度已超出
#define ADSERR_DEVICE_LICENSEINVALID (0x27 + ERR_ADSERRS)       // 许可证无效
#define ADSERR_DEVICE_LICENSESYSTEMID (0x28 + ERR_ADSERRS)      // 许可证系统 ID 无效
#define ADSERR_DEVICE_LICENSENOTIMELIMIT (0x29 + ERR_ADSERRS)   // 许可证不是时限许可
#define ADSERR_DEVICE_LICENSEFUTUREISSUE (0x2A + ERR_ADSERRS)   // 许可证签发时间在未来
#define ADSERR_DEVICE_LICENSETIMETOLONG (0x2B + ERR_ADSERRS)    // 许可证有效期过长
#define ADSERR_DEVICE_EXCEPTION (0x2C + ERR_ADSERRS)            // 设备特定代码抛出异常
#define ADSERR_DEVICE_LICENSEDUPLICATED (0x2D + ERR_ADSERRS)    // 许可证文件被重复读取
#define ADSERR_DEVICE_SIGNATUREINVALID (0x2E + ERR_ADSERRS)     // 签名无效
#define ADSERR_DEVICE_CERTIFICATEINVALID (0x2F + ERR_ADSERRS)   // 公钥证书无效

#define ADSERR_CLIENT_ERROR (0x40 + ERR_ADSERRS)          // 客户端错误类
#define ADSERR_CLIENT_INVALIDPARM (0x41 + ERR_ADSERRS)    // 服务调用参数无效
#define ADSERR_CLIENT_LISTEMPTY (0x42 + ERR_ADSERRS)      // 轮询列表为空
#define ADSERR_CLIENT_VARUSED (0x43 + ERR_ADSERRS)        // 变量连接已被使用
#define ADSERR_CLIENT_DUPLINVOKEID (0x44 + ERR_ADSERRS)   // invokeId 已被占用
#define ADSERR_CLIENT_SYNCTIMEOUT (0x45 + ERR_ADSERRS)    // 同步等待超时
#define ADSERR_CLIENT_W32ERROR (0x46 + ERR_ADSERRS)       // Win32 子系统错误
#define ADSERR_CLIENT_TIMEOUTINVALID (0x47 + ERR_ADSERRS) // 超时参数无效
#define ADSERR_CLIENT_PORTNOTOPEN (0x48 + ERR_ADSERRS)    // ADS 端口未打开
#define ADSERR_CLIENT_NOAMSADDR (0x49 + ERR_ADSERRS)      // 未设置 AMS 地址
#define ADSERR_CLIENT_SYNCINTERNAL (0x50 + ERR_ADSERRS)   // ADS 同步内部错误
#define ADSERR_CLIENT_ADDHASH (0x51 + ERR_ADSERRS)        // 哈希表溢出
#define ADSERR_CLIENT_REMOVEHASH (0x52 + ERR_ADSERRS)     // 哈希表中未找到键
#define ADSERR_CLIENT_NOMORESYM (0x53 + ERR_ADSERRS)      // 缓存中没有更多符号
#define ADSERR_CLIENT_SYNCRESINVALID (0x54 + ERR_ADSERRS) // 收到无效响应
#define ADSERR_CLIENT_SYNCPORTLOCKED (0x55 + ERR_ADSERRS) // 同步端口已锁定

    // =========================================================================
    // 数据结构定义
    // =========================================================================

#pragma pack(push, 1)

    /**
     * @brief ADS 设备的 NetId 结构
     */
    struct AmsNetId
    {
        uint8_t b[6];
    };

    /**
     * @brief ADS 设备的完整地址结构
     */
    struct AmsAddr
    {
        AmsNetId netId;
        uint16_t port;
    };

    /**
     * @brief 版本号结构
     */
    struct AdsVersion
    {
        uint8_t version;
        uint8_t revision;
        uint16_t build;
    };

    /**
     * @brief 目标设备 platformId 枚举定义
     */
    enum ADSPLATFORMID
    {
        ADSLITE_PLATFORM_ID_UNKNOWN = 0u,
        ADSLITE_PLATFORM_ID_WINDOWS_XP_OR_CE = 30u,
        ADSLITE_PLATFORM_ID_WINDOWS_7 = 40u,
        ADSLITE_PLATFORM_ID_WINDOWS_10 = 50u,
        ADSLITE_PLATFORM_ID_WINDOWS_11_OR_SERVER_2022 = 60u,
        ADSLITE_PLATFORM_ID_LINUX = 100u
    };

    /**
     * @brief ADS 传输模式枚举
     */
    enum ADSTRANSMODE
    {
        ADSTRANS_NOTRANS = 0,
        ADSTRANS_CLIENTCYCLE = 1,
        ADSTRANS_CLIENTONCHA = 2,
        ADSTRANS_SERVERCYCLE = 3,
        ADSTRANS_SERVERONCHA = 4,
        ADSTRANS_SERVERCYCLE2 = 5,
        ADSTRANS_SERVERONCHA2 = 6,
        ADSTRANS_CLIENT1REQ = 10,
        ADSTRANS_MAXMODES
    };

    /**
     * @brief ADS 设备状态枚举
     */
    enum ADSSTATE
    {
        ADSSTATE_INVALID = 0,
        ADSSTATE_IDLE = 1,
        ADSSTATE_RESET = 2,
        ADSSTATE_INIT = 3,
        ADSSTATE_START = 4,
        ADSSTATE_RUN = 5,
        ADSSTATE_STOP = 6,
        ADSSTATE_SAVECFG = 7,
        ADSSTATE_LOADCFG = 8,
        ADSSTATE_POWERFAILURE = 9,
        ADSSTATE_POWERGOOD = 10,
        ADSSTATE_ERROR = 11,
        ADSSTATE_SHUTDOWN = 12,
        ADSSTATE_SUSPEND = 13,
        ADSSTATE_RESUME = 14,
        ADSSTATE_CONFIG = 15,
        ADSSTATE_RECONFIG = 16,
        ADSSTATE_STOPPING = 17,
        ADSSTATE_INCOMPATIBLE = 18,
        ADSSTATE_EXCEPTION = 19,
        ADSSTATE_MAXSTATES
    };

    /**
     * @brief 通知属性结构
     */
    struct AdsNotificationAttrib
    {
        uint32_t cbLength;
        ADSTRANSMODE nTransMode;
        uint32_t nMaxDelay;
        union
        {
            uint32_t nCycleTime;
            uint32_t dwChangeFilter;
        };
    };

    /**
     * @brief 通知头结构（传递给回调函数）
     */
    struct AdsNotificationHeader
    {
        int64_t nTimeStamp;
        uint32_t hNotification;
        uint32_t cbSampleSize;
    };

#define ADSLITE_SYSTEM_ID_BUFFER_LENGTH 37u

#pragma pack(pop)

    // =========================================================================
    // 回调函数类型定义
    // =========================================================================

    /**
     * @brief 设备通知回调函数类型
     * @param[in] pAddr ADS 服务器地址
     * @param[in] pNotification 通知头指针
     * @param[in] hUser 用户自定义句柄
     */
    typedef void (*PAdsNotificationFuncEx)(const AmsAddr *pAddr,
                                           const AdsNotificationHeader *pNotification,
                                           uint32_t hUser);

    /**
     * @brief 符号标志位
     */
#define ADSSYMBOLFLAG_PERSISTENT 0x00000001
#define ADSSYMBOLFLAG_BITVALUE 0x00000002
#define ADSSYMBOLFLAG_REFERENCETO 0x0004
#define ADSSYMBOLFLAG_TYPEGUID 0x0008
#define ADSSYMBOLFLAG_TCCOMIFACEPTR 0x0010
#define ADSSYMBOLFLAG_READONLY 0x0020
#define ADSSYMBOLFLAG_CONTEXTMASK 0x0F00

    /**
     * @brief 符号信息结构
     */
    struct AdsSymbolEntry
    {
        uint32_t entryLength;
        uint32_t iGroup;
        uint32_t iOffs;
        uint32_t size;
        uint32_t dataType;
        uint32_t flags;
        uint16_t nameLength;
        uint16_t typeLength;
        uint16_t commentLength;
    };

    /**
     * @brief 文件服务打开标志位
     */
    enum AdsLiteFileOpenFlags
    {
        ADSLITE_FOPEN_READ = 1u << 0,             // 读模式
        ADSLITE_FOPEN_WRITE = 1u << 1,            // 写模式（从头覆盖）
        ADSLITE_FOPEN_APPEND = 1u << 2,           // 追加写模式
        ADSLITE_FOPEN_PLUS = 1u << 3,             // 允许读写组合
        ADSLITE_FOPEN_BINARY = 1u << 4,           // 二进制模式
        ADSLITE_FOPEN_TEXT = 1u << 5,             // 文本模式
        ADSLITE_FOPEN_ENSURE_DIR = 1u << 6,       // 自动创建父目录
        ADSLITE_FOPEN_ENABLE_DIR = 1u << 7,       // 目录操作使能（如目录删除）
        ADSLITE_FOPEN_OVERWRITE = 1u << 8,        // 允许覆盖已有文件
        ADSLITE_FOPEN_OVERWRITE_RENAME = 1u << 9, // 覆盖时重命名回退策略
        ADSLITE_FOPEN_SHIFT_OPENPATH = 16u        // openPath 字段偏移位
    };

    /**
     * @brief 系统服务索引组枚举
     */
    enum nSystemServiceIndexGroups
    {
        SYSTEMSERVICE_FOPEN = 120,
        SYSTEMSERVICE_FCLOSE = 121,
        SYSTEMSERVICE_FREAD = 122,
        SYSTEMSERVICE_FWRITE = 123,
        SYSTEMSERVICE_FDELETE = 131,
        SYSTEMSERVICE_FFILEFIND = 133,
        SYSTEMSERVICE_STARTPROCESS = 500,
        SYSTEMSERVICE_SETNUMPROC = 1200
    };

#ifdef __cplusplus
}
#endif

#endif // ADS_LITE_DEF_H
