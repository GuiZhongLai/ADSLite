#include "backend/AdsFileService.h"
#include "backend/TargetSystemInfo.h"

#include "../standalone/wrap_endian.h"
#include "standalone/Log.h"

#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace adslite
{
    namespace file
    {
        char FileServiceClient::PreferredSeparator(TargetPathStyle style)
        {
            return style == TargetPathStyle::Posix ? '/' : '\\';
        }

        bool FileServiceClient::IsSeparator(char c)
        {
            return c == '/' || c == '\\';
        }

        bool FileServiceClient::IsAsciiAlpha(char c)
        {
            return std::isalpha(static_cast<unsigned char>(c)) != 0;
        }

        FileServiceClient::TargetPathStyle FileServiceClient::PathStyleFromPlatformId(ADSPLATFORMID platformId)
        {
            switch (platformId)
            {
            case ADSLITE_PLATFORM_ID_LINUX:
                return TargetPathStyle::Posix;
            case ADSLITE_PLATFORM_ID_WINDOWS_XP_OR_CE:
            case ADSLITE_PLATFORM_ID_WINDOWS_7:
            case ADSLITE_PLATFORM_ID_WINDOWS_10:
            case ADSLITE_PLATFORM_ID_WINDOWS_11_OR_SERVER_2022:
            default:
                return TargetPathStyle::Windows;
            }
        }

        int64_t FileServiceClient::ResolveTargetPathStyle(uint16_t port,
                                                          const AmsAddr *pAddr,
                                                          TargetPathStyle *pPathStyle,
                                                          ADSPLATFORMID *pPlatformId)
        {
            if (!pPathStyle)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            ADSPLATFORMID platformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            const int64_t status = targetSystemInfo_.ReadPlatformId(port, pAddr, &platformId);
            if (status != ADSERR_NOERR)
            {
                return status;
            }

            *pPathStyle = PathStyleFromPlatformId(platformId);
            if (pPlatformId)
            {
                *pPlatformId = platformId;
            }
            return ADSERR_NOERR;
        }

        bool FileServiceClient::IsWindowsDriveRoot(const std::string &path, char sep)
        {
            return path.size() == 3 && IsAsciiAlpha(path[0]) && path[1] == ':' && path[2] == sep;
        }

        std::string FileServiceClient::NormalizePathForStyle(const char *rawPath, TargetPathStyle style)
        {
            std::string input(rawPath ? rawPath : "");
            if (input.empty())
            {
                return input;
            }

            const char sep = PreferredSeparator(style);
            const bool keepUncPrefix = style == TargetPathStyle::Windows &&
                                       input.size() >= 2 && IsSeparator(input[0]) && IsSeparator(input[1]);

            // 统一路径分隔符并折叠重复分隔符，避免同一条路径因写法不同导致行为不一致。
            std::string normalized;
            normalized.reserve(input.size());

            size_t index = 0;
            if (keepUncPrefix)
            {
                normalized.push_back(sep);
                normalized.push_back(sep);
                index = 2;
            }

            bool prevIsSep = !normalized.empty();
            for (; index < input.size(); ++index)
            {
                const char c = input[index];
                if (IsSeparator(c))
                {
                    if (!prevIsSep)
                    {
                        normalized.push_back(sep);
                        prevIsSep = true;
                    }
                    continue;
                }

                normalized.push_back(c);
                prevIsSep = false;
            }

            while (normalized.size() > 1 && IsSeparator(normalized.back()))
            {
                if (style == TargetPathStyle::Windows && IsWindowsDriveRoot(normalized, sep))
                {
                    break;
                }
                normalized.pop_back();
            }

            return normalized;
        }

        std::string FileServiceClient::GetParentDirectory(const std::string &path,
                                                          TargetPathStyle style)
        {
            if (path.empty())
            {
                return std::string();
            }

            // 根目录没有再上一层父目录，递归建目录时在此终止。
            const char sep = PreferredSeparator(style);
            if (style == TargetPathStyle::Posix && path == std::string(1, sep))
            {
                return std::string();
            }

            if (style == TargetPathStyle::Windows && IsWindowsDriveRoot(path, sep))
            {
                return std::string();
            }

            if (style == TargetPathStyle::Windows &&
                path.size() >= 2 &&
                IsSeparator(path[0]) &&
                IsSeparator(path[1]))
            {
                // UNC 共享根本身不由文件服务创建，递归时停在共享根之下。
                const size_t serverEnd = path.find(sep, 2);
                if (serverEnd == std::string::npos)
                {
                    return std::string();
                }

                const size_t shareEnd = path.find(sep, serverEnd + 1);
                if (shareEnd == std::string::npos)
                {
                    return std::string();
                }

                if (shareEnd == path.size() - 1)
                {
                    return std::string();
                }
            }

            const size_t separatorPos = path.find_last_of("/\\");
            if (separatorPos == std::string::npos)
            {
                return std::string();
            }

            if (separatorPos == 0)
            {
                return path.size() == 1 ? std::string() : path.substr(0, 1);
            }

            if (style == TargetPathStyle::Windows && separatorPos == 2 && IsAsciiAlpha(path[0]) && path[1] == ':')
            {
                return path.substr(0, 3);
            }

            return path.substr(0, separatorPos);
        }

        AmsAddr FileServiceClient::BuildFileServiceAddr(const AmsAddr *pAddr)
        {
            AmsAddr serviceAddr = *pAddr;
            // 文件服务固定走 10000 端口，避免调用方传入 851 导致服务不支持。
            serviceAddr.port = kFileServicePort;
            return serviceAddr;
        }

        void FileServiceClient::NormalizeFileFindEntry(AdsLiteFileFindData *pEntry)
        {
            pEntry->fileHandle = bhf::ads::letoh(pEntry->fileHandle);
            pEntry->fileAttributes = bhf::ads::letoh(pEntry->fileAttributes);
        }

        int64_t FileServiceClient::CloseFindHandleIfNeeded(uint16_t port,
                                                           const AmsAddr *pAddr,
                                                           const AdsLiteFileFindData &entry)
        {
            if (!pAddr || entry.fileHandle == 0)
            {
                return ADSERR_NOERR;
            }

            return FileCloseImpl(port, pAddr, entry.fileHandle);
        }

        bool FileServiceClient::IsDirectory(const AdsLiteFileFindData &entry)
        {
            return (entry.fileAttributes & kFileAttributeDirectory) != 0;
        }

        bool FileServiceClient::IsSpecialEntry(const AdsLiteFileFindData &entry)
        {
            return std::strcmp(entry.fileName, ".") == 0 || std::strcmp(entry.fileName, "..") == 0;
        }

        std::string FileServiceClient::JoinPath(const std::string &basePath,
                                                const char *name,
                                                TargetPathStyle style)
        {
            if (basePath.empty())
            {
                return NormalizePathForStyle(name ? name : "", style);
            }

            if (basePath.back() == '/' || basePath.back() == '\\')
            {
                return basePath + (name ? name : "");
            }

            return basePath + PreferredSeparator(style) + (name ? name : "");
        }

        std::string FileServiceClient::MakePattern(const std::string &basePath,
                                                   TargetPathStyle style)
        {
            if (basePath.empty())
            {
                return "*";
            }

            if (basePath.back() == '/' || basePath.back() == '\\')
            {
                return basePath + "*";
            }

            return basePath + PreferredSeparator(style) + "*";
        }

        std::string FileServiceClient::MakeDirMarkerPath(const char *remoteDirPath,
                                                         TargetPathStyle style)
        {
            std::string path = NormalizePathForStyle(remoteDirPath, style);
            if (path.empty())
            {
                return path;
            }

            if (path.back() == '/' || path.back() == '\\')
            {
                return path + ".adslite_dir_marker.tmp";
            }

            return path + PreferredSeparator(style) + ".adslite_dir_marker.tmp";
        }

        FileServiceClient::FileServiceClient(IAdsBackend &backend,
                                             targetinfo::TargetSystemInfo &targetSystemInfo)
            : backend_(backend),
              targetSystemInfo_(targetSystemInfo)
        {
        }

        int64_t FileServiceClient::FileOpen(uint16_t port,
                                            const AmsAddr *pAddr,
                                            const char *remotePath,
                                            uint32_t openFlags,
                                            uint32_t *pFileHandle)
        {
            return FileOpenImpl(port, pAddr, remotePath, openFlags, pFileHandle);
        }

        int64_t FileServiceClient::FileClose(uint16_t port,
                                             const AmsAddr *pAddr,
                                             uint32_t fileHandle)
        {
            return FileCloseImpl(port, pAddr, fileHandle);
        }

        int64_t FileServiceClient::FileRead(uint16_t port,
                                            const AmsAddr *pAddr,
                                            uint32_t fileHandle,
                                            uint32_t length,
                                            void *pData,
                                            uint32_t *pBytesRead)
        {
            return FileReadImpl(port, pAddr, fileHandle, length, pData, pBytesRead);
        }

        int64_t FileServiceClient::FileWrite(uint16_t port,
                                             const AmsAddr *pAddr,
                                             uint32_t fileHandle,
                                             const void *pData,
                                             uint32_t length)
        {
            return FileWriteImpl(port, pAddr, fileHandle, pData, length);
        }

        int64_t FileServiceClient::FileDelete(uint16_t port,
                                              const AmsAddr *pAddr,
                                              const char *remotePath,
                                              uint32_t deleteFlags)
        {
            return FileDeleteImpl(port, pAddr, remotePath, deleteFlags);
        }

        int64_t FileServiceClient::DirCreate(uint16_t port,
                                             const AmsAddr *pAddr,
                                             const char *remoteDirPath)
        {
            return DirCreateImpl(port, pAddr, remoteDirPath);
        }

        int64_t FileServiceClient::DirDelete(uint16_t port,
                                             const AmsAddr *pAddr,
                                             const char *remoteDirPath,
                                             bool deleteDirSelf)
        {
            return DirDeleteImpl(port, pAddr, remoteDirPath, deleteDirSelf);
        }

        int64_t FileServiceClient::FileRename(uint16_t port,
                                              const AmsAddr *pAddr,
                                              const char *sourcePath,
                                              const char *targetPath)
        {
            return FileRenameImpl(port, pAddr, sourcePath, targetPath);
        }

        int64_t FileServiceClient::FileList(uint16_t port,
                                            const AmsAddr *pAddr,
                                            const char *pathPattern,
                                            uint32_t findFlags,
                                            char *pNameBuffer,
                                            uint32_t nameBufferLength,
                                            uint32_t *pBytesRequired,
                                            uint32_t *pItemCount)
        {
            return FileListImpl(port,
                                pAddr,
                                pathPattern,
                                findFlags,
                                pNameBuffer,
                                nameBufferLength,
                                pBytesRequired,
                                pItemCount);
        }

        int64_t FileServiceClient::FileOpenImpl(uint16_t port,
                                                const AmsAddr *pAddr,
                                                const char *remotePath,
                                                uint32_t openFlags,
                                                uint32_t *pFileHandle)
        {
            if (!pAddr || !remotePath || !pFileHandle)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            ADSPLATFORMID platformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            int64_t ret = targetSystemInfo_.ReadPlatformId(port, pAddr, &platformId);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            const TargetPathStyle pathStyle = PathStyleFromPlatformId(platformId);
            const std::string normalizedPath = NormalizePathForStyle(remotePath, pathStyle);
            const size_t pathLen = normalizedPath.size();
            if (pathLen == 0 || pathLen > UINT32_MAX - 1)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            ret = ExecuteFileOpenImpl(port,
                                      pAddr,
                                      normalizedPath,
                                      openFlags,
                                      pFileHandle);
            if (ret == ADSERR_DEVICE_NOTFOUND && (openFlags & ADSLITE_FOPEN_ENSURE_DIR) != 0)
            {
                // ENSURE_DIR 的补建能力在目标系统上并不总是递归生效，
                // 因此这里在首次打开失败后，显式补建父目录，再重试一次原始 FOPEN。
                const std::string parentDir = GetParentDirectory(normalizedPath, pathStyle);
                if (!parentDir.empty())
                {
                    const int64_t dirRet = DirCreateImpl(port, pAddr, parentDir.c_str());
                    if (dirRet != ADSERR_NOERR)
                    {
                        return dirRet;
                    }

                    ret = ExecuteFileOpenImpl(port,
                                              pAddr,
                                              normalizedPath,
                                              openFlags,
                                              pFileHandle);
                }
            }

            if (ret != ADSERR_NOERR)
            {
                LOG_WARN("FileOpen failed ret=0x" << std::hex << ret
                                                  << ", platformId=" << std::dec << platformId
                                                  << ", path=" << normalizedPath);
            }
            return ret;
        }

        int64_t FileServiceClient::ExecuteFileOpenImpl(uint16_t port,
                                                       const AmsAddr *pAddr,
                                                       const std::string &normalizedPath,
                                                       uint32_t openFlags,
                                                       uint32_t *pFileHandle)
        {
            if (!pAddr || !pFileHandle)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            // 该函数只负责透传到底层文件服务，便于上层在失败后决定是否补建父目录再重试。
            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);
            return backend_.SyncReadWriteReq(port,
                                             &serviceAddr,
                                             SYSTEMSERVICE_FOPEN,
                                             openFlags,
                                             sizeof(uint32_t),
                                             pFileHandle,
                                             static_cast<uint32_t>(normalizedPath.size() + 1),
                                             normalizedPath.c_str(),
                                             nullptr);
        }

        int64_t FileServiceClient::FileCloseImpl(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 uint32_t fileHandle)
        {
            if (!pAddr)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);
            return backend_.SyncReadWriteReq(port,
                                             &serviceAddr,
                                             SYSTEMSERVICE_FCLOSE,
                                             fileHandle,
                                             0,
                                             nullptr,
                                             0,
                                             nullptr,
                                             nullptr);
        }

        int64_t FileServiceClient::FileReadImpl(uint16_t port,
                                                const AmsAddr *pAddr,
                                                uint32_t fileHandle,
                                                uint32_t length,
                                                void *pData,
                                                uint32_t *pBytesRead)
        {
            if (!pAddr)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            if (length == 0)
            {
                if (pBytesRead)
                {
                    *pBytesRead = 0;
                }
                return ADSERR_NOERR;
            }

            if (!pData)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);
            return backend_.SyncReadWriteReq(port,
                                             &serviceAddr,
                                             SYSTEMSERVICE_FREAD,
                                             fileHandle,
                                             length,
                                             pData,
                                             0,
                                             nullptr,
                                             pBytesRead);
        }

        int64_t FileServiceClient::FileWriteImpl(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 uint32_t fileHandle,
                                                 const void *pData,
                                                 uint32_t length)
        {
            if (!pAddr)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            if (length == 0)
            {
                return ADSERR_NOERR;
            }

            if (!pData)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);
            return backend_.SyncReadWriteReq(port,
                                             &serviceAddr,
                                             SYSTEMSERVICE_FWRITE,
                                             fileHandle,
                                             0,
                                             nullptr,
                                             length,
                                             pData,
                                             nullptr);
        }

        int64_t FileServiceClient::FileDeleteImpl(uint16_t port,
                                                  const AmsAddr *pAddr,
                                                  const char *remotePath,
                                                  uint32_t deleteFlags)
        {
            if (!pAddr || !remotePath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            ADSPLATFORMID platformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            int64_t ret = targetSystemInfo_.ReadPlatformId(port, pAddr, &platformId);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            const TargetPathStyle pathStyle = PathStyleFromPlatformId(platformId);
            const std::string normalizedPath = NormalizePathForStyle(remotePath, pathStyle);

            const size_t pathLen = normalizedPath.size();
            if (pathLen == 0 || pathLen > UINT32_MAX - 1)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);
            ret = backend_.SyncReadWriteReq(port,
                                            &serviceAddr,
                                            SYSTEMSERVICE_FDELETE,
                                            deleteFlags,
                                            0,
                                            nullptr,
                                            static_cast<uint32_t>(pathLen + 1),
                                            normalizedPath.c_str(),
                                            nullptr);
            if (ret != ADSERR_NOERR && ret != ADSERR_DEVICE_NOTFOUND)
            {
                LOG_WARN("FileDelete failed ret=0x" << std::hex << ret
                                                    << ", platformId=" << std::dec << platformId
                                                    << ", path=" << normalizedPath);
            }
            return ret;
        }

        int64_t FileServiceClient::DirCreateImpl(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 const char *remoteDirPath)
        {
            if (!pAddr || !remoteDirPath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            ADSPLATFORMID platformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            int64_t ret = targetSystemInfo_.ReadPlatformId(port, pAddr, &platformId);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            const TargetPathStyle pathStyle = PathStyleFromPlatformId(platformId);
            const std::string normalizedDirPath = NormalizePathForStyle(remoteDirPath, pathStyle);
            if (normalizedDirPath.empty())
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const std::string parentDir = GetParentDirectory(normalizedDirPath, pathStyle);
            if (!parentDir.empty())
            {
                // 先保证父目录存在，再在当前目录内创建 marker 文件。
                // 这样既保留原有“临时文件建目录”的语义，又能覆盖多级目录场景。
                ret = DirCreateImpl(port, pAddr, parentDir.c_str());
                if (ret != ADSERR_NOERR)
                {
                    return ret;
                }
            }

            const std::string markerPath = MakeDirMarkerPath(normalizedDirPath.c_str(), pathStyle);
            if (markerPath.empty())
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            // 目录创建仍保持原始语义：尝试在目标目录内创建一个临时文件，
            // 依赖文件服务在打开时自动确保该目录存在，随后再把该临时文件删掉。
            const uint32_t createFlags = ADSLITE_FOPEN_WRITE |
                                         ADSLITE_FOPEN_BINARY |
                                         ADSLITE_FOPEN_PLUS |
                                         ADSLITE_FOPEN_ENSURE_DIR;
            const uint32_t fallbackFlags = ADSLITE_FOPEN_APPEND |
                                           ADSLITE_FOPEN_BINARY |
                                           ADSLITE_FOPEN_PLUS |
                                           ADSLITE_FOPEN_ENSURE_DIR;

            uint32_t handle = 0;
            ret = ExecuteFileOpenImpl(port,
                                      pAddr,
                                      markerPath,
                                      createFlags,
                                      &handle);
            if (ret == ADSERR_DEVICE_NOTFOUND)
            {
                ret = ExecuteFileOpenImpl(port,
                                          pAddr,
                                          markerPath,
                                          fallbackFlags,
                                          &handle);
            }
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            const int64_t closeRet = FileCloseImpl(port, pAddr, handle);
            if (closeRet != ADSERR_NOERR)
            {
                return closeRet;
            }

            ret = FileDeleteImpl(port,
                                 pAddr,
                                 markerPath.c_str(),
                                 ADSLITE_FOPEN_READ);
            if (ret == ADSERR_NOERR || ret == ADSERR_DEVICE_NOTFOUND)
            {
                return ADSERR_NOERR;
            }
            return ret;
        }

        int64_t FileServiceClient::DirDeleteImpl(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 const char *remoteDirPath,
                                                 bool deleteDirSelf)
        {
            if (!pAddr || !remoteDirPath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const int64_t ret = DeleteTreeImpl(port,
                                               pAddr,
                                               remoteDirPath,
                                               ADSLITE_FOPEN_READ,
                                               ADSLITE_FOPEN_READ,
                                               ADSLITE_FOPEN_READ | ADSLITE_FOPEN_ENABLE_DIR,
                                               32,
                                               deleteDirSelf);
            if (ret == ADSERR_NOERR || ret == ADSERR_DEVICE_NOTFOUND)
            {
                return ADSERR_NOERR;
            }
            return ret;
        }

        int64_t FileServiceClient::FileRenameImpl(uint16_t port,
                                                  const AmsAddr *pAddr,
                                                  const char *sourcePath,
                                                  const char *targetPath)
        {
            if (!pAddr || !sourcePath || !targetPath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            if (std::strcmp(sourcePath, targetPath) == 0)
            {
                return ADSERR_NOERR;
            }

            const uint32_t readFlags = ADSLITE_FOPEN_READ | ADSLITE_FOPEN_BINARY;
            const uint32_t writeFlags = ADSLITE_FOPEN_WRITE |
                                        ADSLITE_FOPEN_BINARY |
                                        ADSLITE_FOPEN_PLUS |
                                        ADSLITE_FOPEN_ENSURE_DIR;

            uint32_t srcHandle = 0;
            int64_t ret = FileOpenImpl(port, pAddr, sourcePath, readFlags, &srcHandle);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            uint32_t dstHandle = 0;
            ret = FileOpenImpl(port, pAddr, targetPath, writeFlags, &dstHandle);
            if (ret != ADSERR_NOERR)
            {
                FileCloseImpl(port, pAddr, srcHandle);
                return ret;
            }

            std::vector<uint8_t> buffer(64u * 1024u);
            while (true)
            {
                uint32_t bytesRead = 0;
                ret = FileReadImpl(port,
                                   pAddr,
                                   srcHandle,
                                   static_cast<uint32_t>(buffer.size()),
                                   buffer.data(),
                                   &bytesRead);
                if (ret != ADSERR_NOERR)
                {
                    break;
                }

                if (bytesRead == 0)
                {
                    break;
                }

                ret = FileWriteImpl(port, pAddr, dstHandle, buffer.data(), bytesRead);
                if (ret != ADSERR_NOERR)
                {
                    break;
                }
            }

            const int64_t closeDst = FileCloseImpl(port, pAddr, dstHandle);
            const int64_t closeSrc = FileCloseImpl(port, pAddr, srcHandle);

            if (ret != ADSERR_NOERR)
            {
                return ret;
            }
            if (closeDst != ADSERR_NOERR)
            {
                return closeDst;
            }
            if (closeSrc != ADSERR_NOERR)
            {
                return closeSrc;
            }

            ret = FileDeleteImpl(port, pAddr, sourcePath, ADSLITE_FOPEN_READ);
            if (ret == ADSERR_NOERR || ret == ADSERR_DEVICE_NOTFOUND)
            {
                return ADSERR_NOERR;
            }
            return ret;
        }

        int64_t FileServiceClient::FileListImpl(uint16_t port,
                                                const AmsAddr *pAddr,
                                                const char *pathPattern,
                                                uint32_t findFlags,
                                                char *pNameBuffer,
                                                uint32_t nameBufferLength,
                                                uint32_t *pBytesRequired,
                                                uint32_t *pItemCount)
        {
            if (!pAddr || !pathPattern)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            uint32_t required = 1;
            uint32_t count = 0;
            uint32_t writeOffset = 0;
            if (pNameBuffer && nameBufferLength > 0)
            {
                pNameBuffer[0] = '\0';
            }

            AdsLiteFileFindData entry = {};
            uint32_t isLast = 0;
            int64_t ret = FileFindFirstImpl(port,
                                            pAddr,
                                            pathPattern,
                                            findFlags,
                                            &entry,
                                            &isLast);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            while (!isLast)
            {
                if (!IsSpecialEntry(entry))
                {
                    const uint32_t nameLen = static_cast<uint32_t>(std::strlen(entry.fileName));
                    required += nameLen + 1;
                    ++count;

                    if (pNameBuffer && writeOffset + nameLen + 1 < nameBufferLength)
                    {
                        std::memcpy(pNameBuffer + writeOffset, entry.fileName, nameLen);
                        writeOffset += nameLen;
                        pNameBuffer[writeOffset++] = '\n';
                        pNameBuffer[writeOffset] = '\0';
                    }
                }

                ret = FileFindNextImpl(port, pAddr, &entry, &isLast);
                if (ret != ADSERR_NOERR)
                {
                    CloseFindHandleIfNeeded(port, pAddr, entry);
                    return ret;
                }
            }

            ret = CloseFindHandleIfNeeded(port, pAddr, entry);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            if (pBytesRequired)
            {
                *pBytesRequired = required;
            }
            if (pItemCount)
            {
                *pItemCount = count;
            }

            if (pNameBuffer && nameBufferLength > 0)
            {
                if (nameBufferLength < required)
                {
                    return ADSERR_DEVICE_INVALIDSIZE;
                }

                if (writeOffset > 0 && pNameBuffer[writeOffset - 1] == '\n')
                {
                    pNameBuffer[writeOffset - 1] = '\0';
                }
            }

            return ADSERR_NOERR;
        }

        int64_t FileServiceClient::FileFindFirstImpl(uint16_t port,
                                                     const AmsAddr *pAddr,
                                                     const char *pathPattern,
                                                     uint32_t findFlags,
                                                     AdsLiteFileFindData *pEntry,
                                                     uint32_t *pIsLast)
        {
            if (!pAddr || !pathPattern || !pEntry)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            TargetPathStyle pathStyle = TargetPathStyle::Windows;
            ADSPLATFORMID platformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            const int64_t styleStatus = ResolveTargetPathStyle(port,
                                                               pAddr,
                                                               &pathStyle,
                                                               &platformId);
            if (styleStatus != ADSERR_NOERR)
            {
                return styleStatus;
            }

            const std::string normalizedPattern = NormalizePathForStyle(pathPattern, pathStyle);
            if (normalizedPattern.empty())
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const size_t pathLen = normalizedPattern.size();
            if (pathLen == 0 || pathLen > UINT32_MAX - 1)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);

            pEntry->fileHandle = findFlags ? findFlags : kFileFindGeneric;
            const int64_t ret = backend_.SyncReadWriteReq(port,
                                                          &serviceAddr,
                                                          SYSTEMSERVICE_FFILEFIND,
                                                          pEntry->fileHandle,
                                                          sizeof(*pEntry),
                                                          pEntry,
                                                          static_cast<uint32_t>(pathLen + 1),
                                                          normalizedPattern.c_str(),
                                                          nullptr);

            if (ret == ADSERR_DEVICE_NOTFOUND)
            {
                if (pIsLast)
                {
                    *pIsLast = 1;
                }
                return ADSERR_NOERR;
            }

            if (ret != ADSERR_NOERR)
            {
                LOG_WARN("FileFindFirstImpl failed ret=0x" << std::hex << ret
                                                           << ", platformId=" << std::dec << platformId
                                                           << ", pattern=" << normalizedPattern);
                return ret;
            }

            NormalizeFileFindEntry(pEntry);
            if (pIsLast)
            {
                *pIsLast = 0;
            }
            return ADSERR_NOERR;
        }

        int64_t FileServiceClient::FileFindNextImpl(uint16_t port,
                                                    const AmsAddr *pAddr,
                                                    AdsLiteFileFindData *pEntry,
                                                    uint32_t *pIsLast)
        {
            if (!pAddr || !pEntry)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);

            const int64_t ret = backend_.SyncReadWriteReq(port,
                                                          &serviceAddr,
                                                          SYSTEMSERVICE_FFILEFIND,
                                                          pEntry->fileHandle,
                                                          sizeof(*pEntry),
                                                          pEntry,
                                                          0,
                                                          nullptr,
                                                          nullptr);

            if (ret == ADSERR_DEVICE_NOTFOUND)
            {
                if (pIsLast)
                {
                    *pIsLast = 1;
                }
                return ADSERR_NOERR;
            }

            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            NormalizeFileFindEntry(pEntry);
            if (pIsLast)
            {
                *pIsLast = 0;
            }
            return ADSERR_NOERR;
        }

        int64_t FileServiceClient::DeleteTreeImpl(uint16_t port,
                                                  const AmsAddr *pAddr,
                                                  const char *rootPath,
                                                  uint32_t findFlags,
                                                  uint32_t fileDeleteFlags,
                                                  uint32_t dirDeleteFlags,
                                                  uint32_t maxDepth,
                                                  bool deleteRootDir)
        {
            if (!pAddr || !rootPath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            ADSPLATFORMID platformId = ADSLITE_PLATFORM_ID_UNKNOWN;
            int64_t ret = targetSystemInfo_.ReadPlatformId(port, pAddr, &platformId);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            const TargetPathStyle pathStyle = PathStyleFromPlatformId(platformId);
            const std::string normalizedRoot = NormalizePathForStyle(rootPath, pathStyle);
            if (normalizedRoot.empty())
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            std::vector<RecursiveNode> stack;
            stack.push_back({normalizedRoot, 0u, false});

            // 采用显式栈做后序遍历：先删子项，再删当前目录，避免递归调用过深。
            while (!stack.empty())
            {
                RecursiveNode node = stack.back();
                stack.pop_back();

                if (node.expanded)
                {
                    if (!deleteRootDir && node.depth == 0)
                    {
                        continue;
                    }

                    int64_t deleteRet = FileDeleteImpl(port,
                                                       pAddr,
                                                       node.path.c_str(),
                                                       dirDeleteFlags);
                    if (deleteRet == ADSERR_DEVICE_NOTFOUND)
                    {
                        // 兼容历史错误实现遗留的“同名文件占位”场景，必要时回退按文件删除。
                        deleteRet = FileDeleteImpl(port,
                                                   pAddr,
                                                   node.path.c_str(),
                                                   fileDeleteFlags);
                    }

                    if (deleteRet != ADSERR_NOERR && deleteRet != ADSERR_DEVICE_NOTFOUND)
                    {
                        return deleteRet;
                    }
                    continue;
                }

                stack.push_back({node.path, node.depth, true});

                if (node.depth > maxDepth)
                {
                    return ADSERR_CLIENT_INVALIDPARM;
                }

                const std::string pattern = MakePattern(node.path, pathStyle);
                AdsLiteFileFindData entry = {};
                uint32_t isLast = 0;
                ret = FileFindFirstImpl(port,
                                        pAddr,
                                        pattern.c_str(),
                                        findFlags,
                                        &entry,
                                        &isLast);
                if (ret != ADSERR_NOERR)
                {
                    return ret;
                }

                while (!isLast)
                {
                    if (!IsSpecialEntry(entry))
                    {
                        const std::string childPath = JoinPath(node.path, entry.fileName, pathStyle);
                        if (IsDirectory(entry))
                        {
                            stack.push_back({childPath, node.depth + 1, false});
                        }
                        else
                        {
                            ret = FileDeleteImpl(port,
                                                 pAddr,
                                                 childPath.c_str(),
                                                 fileDeleteFlags);
                            if (ret != ADSERR_NOERR && ret != ADSERR_DEVICE_NOTFOUND)
                            {
                                return ret;
                            }
                        }
                    }

                    ret = FileFindNextImpl(port, pAddr, &entry, &isLast);
                    if (ret != ADSERR_NOERR)
                    {
                        CloseFindHandleIfNeeded(port, pAddr, entry);
                        return ret;
                    }
                }

                ret = CloseFindHandleIfNeeded(port, pAddr, entry);
                if (ret != ADSERR_NOERR)
                {
                    return ret;
                }
            }

            return ADSERR_NOERR;
        }

    }
}
