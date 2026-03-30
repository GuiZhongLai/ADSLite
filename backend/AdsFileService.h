#pragma once

#include "../AdsLiteDef.h"
#include "IAdsBackend.h"
#include "TargetSystemInfo.h"

#include <cstdint>
#include <string>

namespace adslite
{
    namespace file
    {
        struct AdsLiteFileFindData
        {
            uint32_t fileHandle;
            uint32_t fileAttributes;
            uint64_t reserved1[5];
            char fileName[260];
            char reserved2[14];
            uint16_t reserved3;
        };

        class FileServiceClient
        {
        public:
            FileServiceClient(IAdsBackend &backend,
                              targetinfo::TargetSystemInfo &targetSystemInfo);

            int64_t FileOpen(uint16_t port,
                             const AmsAddr *pAddr,
                             const char *remotePath,
                             uint32_t openFlags,
                             uint32_t *pFileHandle);

            int64_t FileClose(uint16_t port,
                              const AmsAddr *pAddr,
                              uint32_t fileHandle);

            int64_t FileRead(uint16_t port,
                             const AmsAddr *pAddr,
                             uint32_t fileHandle,
                             uint32_t length,
                             void *pData,
                             uint32_t *pBytesRead);

            int64_t FileWrite(uint16_t port,
                              const AmsAddr *pAddr,
                              uint32_t fileHandle,
                              const void *pData,
                              uint32_t length);

            int64_t FileDelete(uint16_t port,
                               const AmsAddr *pAddr,
                               const char *remotePath,
                               uint32_t deleteFlags);

            int64_t DirCreate(uint16_t port,
                              const AmsAddr *pAddr,
                              const char *remoteDirPath);

            int64_t DirDelete(uint16_t port,
                              const AmsAddr *pAddr,
                              const char *remoteDirPath,
                              bool deleteDirSelf);

            int64_t FileRename(uint16_t port,
                               const AmsAddr *pAddr,
                               const char *sourcePath,
                               const char *targetPath);

            int64_t FileList(uint16_t port,
                             const AmsAddr *pAddr,
                             const char *pathPattern,
                             uint32_t findFlags,
                             char *pNameBuffer,
                             uint32_t nameBufferLength,
                             uint32_t *pBytesRequired,
                             uint32_t *pItemCount);

        private:
            enum class TargetPathStyle
            {
                Windows,
                Posix,
            };

            struct RecursiveNode
            {
                std::string path;
                uint32_t depth;
                bool expanded;
            };

            static constexpr uint32_t kFileFindGeneric = (1u << 0);
            static constexpr uint32_t kFileAttributeDirectory = 0x10;
            static constexpr uint16_t kFileServicePort = 10000;

            int64_t FileOpenImpl(uint16_t port,
                                 const AmsAddr *pAddr,
                                 const char *remotePath,
                                 uint32_t openFlags,
                                 uint32_t *pFileHandle);

            int64_t FileCloseImpl(uint16_t port,
                                  const AmsAddr *pAddr,
                                  uint32_t fileHandle);

            int64_t FileReadImpl(uint16_t port,
                                 const AmsAddr *pAddr,
                                 uint32_t fileHandle,
                                 uint32_t length,
                                 void *pData,
                                 uint32_t *pBytesRead);

            int64_t FileWriteImpl(uint16_t port,
                                  const AmsAddr *pAddr,
                                  uint32_t fileHandle,
                                  const void *pData,
                                  uint32_t length);

            int64_t FileDeleteImpl(uint16_t port,
                                   const AmsAddr *pAddr,
                                   const char *remotePath,
                                   uint32_t deleteFlags);

            int64_t DirCreateImpl(uint16_t port,
                                  const AmsAddr *pAddr,
                                  const char *remoteDirPath);

            int64_t DirDeleteImpl(uint16_t port,
                                  const AmsAddr *pAddr,
                                  const char *remoteDirPath,
                                  bool deleteDirSelf);

            int64_t FileRenameImpl(uint16_t port,
                                   const AmsAddr *pAddr,
                                   const char *sourcePath,
                                   const char *targetPath);

            int64_t FileListImpl(uint16_t port,
                                 const AmsAddr *pAddr,
                                 const char *pathPattern,
                                 uint32_t findFlags,
                                 char *pNameBuffer,
                                 uint32_t nameBufferLength,
                                 uint32_t *pBytesRequired,
                                 uint32_t *pItemCount);

            int64_t FileFindFirstImpl(uint16_t port,
                                      const AmsAddr *pAddr,
                                      const char *pathPattern,
                                      uint32_t findFlags,
                                      AdsLiteFileFindData *pEntry,
                                      uint32_t *pIsLast);

            int64_t FileFindNextImpl(uint16_t port,
                                     const AmsAddr *pAddr,
                                     AdsLiteFileFindData *pEntry,
                                     uint32_t *pIsLast);

            int64_t DeleteTreeImpl(uint16_t port,
                                   const AmsAddr *pAddr,
                                   const char *rootPath,
                                   uint32_t findFlags,
                                   uint32_t fileDeleteFlags,
                                   uint32_t dirDeleteFlags,
                                   uint32_t maxDepth,
                                   bool deleteRootDir);

            // 直接向底层文件服务发起 FOPEN 请求，不附带父目录补建逻辑。
            int64_t ExecuteFileOpenImpl(uint16_t port,
                                        const AmsAddr *pAddr,
                                        const std::string &normalizedPath,
                                        uint32_t openFlags,
                                        uint32_t *pFileHandle);

            // 解析目标平台并映射为路径风格，供文件服务路径标准化使用。
            int64_t ResolveTargetPathStyle(uint16_t port,
                                           const AmsAddr *pAddr,
                                           TargetPathStyle *pPathStyle,
                                           ADSPLATFORMID *pPlatformId = nullptr);

            // 以下工具函数统一处理路径分隔符、路径拼接和文件枚举结果。
            static char PreferredSeparator(TargetPathStyle style);
            static bool IsSeparator(char c);
            static bool IsAsciiAlpha(char c);
            static TargetPathStyle PathStyleFromPlatformId(ADSPLATFORMID platformId);
            static bool IsWindowsDriveRoot(const std::string &path, char sep);
            static std::string NormalizePathForStyle(const char *rawPath, TargetPathStyle style);
            // 从规范化路径中提取父目录，驱动器根目录和路径根返回空字符串。
            static std::string GetParentDirectory(const std::string &path,
                                                  TargetPathStyle style);
            static AmsAddr BuildFileServiceAddr(const AmsAddr *pAddr);
            static void NormalizeFileFindEntry(AdsLiteFileFindData *pEntry);
            int64_t CloseFindHandleIfNeeded(uint16_t port,
                                            const AmsAddr *pAddr,
                                            const AdsLiteFileFindData &entry);
            static bool IsDirectory(const AdsLiteFileFindData &entry);
            static bool IsSpecialEntry(const AdsLiteFileFindData &entry);
            static std::string JoinPath(const std::string &basePath,
                                        const char *name,
                                        TargetPathStyle style);
            static std::string MakePattern(const std::string &basePath,
                                           TargetPathStyle style);
            static std::string MakeDirMarkerPath(const char *remoteDirPath,
                                                 TargetPathStyle style);

            IAdsBackend &backend_;
            targetinfo::TargetSystemInfo &targetSystemInfo_;
        };

    }
}
