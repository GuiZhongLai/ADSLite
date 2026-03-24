#include "backend/AdsFileService.h"

#include "../standalone/wrap_endian.h"

#include <cstring>
#include <string>
#include <vector>

namespace adslite
{
    namespace file
    {
        namespace
        {
            constexpr uint32_t FILE_FIND_GENERIC = (1u << 0);
            constexpr uint32_t FILE_ATTRIBUTE_DIRECTORY = 0x10;
            constexpr uint16_t FILE_SERVICE_PORT = 10000;

            AmsAddr BuildFileServiceAddr(const AmsAddr *pAddr)
            {
                AmsAddr serviceAddr = *pAddr;
                // 文件服务固定走 10000 端口，避免调用方传入 851 导致服务不支持。
                serviceAddr.port = FILE_SERVICE_PORT;
                return serviceAddr;
            }

            void NormalizeFileFindEntry(AdsLiteFileFindData *pEntry)
            {
                pEntry->fileHandle = bhf::ads::letoh(pEntry->fileHandle);
                pEntry->fileAttributes = bhf::ads::letoh(pEntry->fileAttributes);
            }

            bool IsDirectory(const AdsLiteFileFindData &entry)
            {
                return (entry.fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            }

            bool IsSpecialEntry(const AdsLiteFileFindData &entry)
            {
                return std::strcmp(entry.fileName, ".") == 0 || std::strcmp(entry.fileName, "..") == 0;
            }

            std::string JoinPath(const std::string &basePath, const char *name)
            {
                if (basePath.empty())
                {
                    return std::string(name ? name : "");
                }

                if (basePath.back() == '/' || basePath.back() == '\\')
                {
                    return basePath + (name ? name : "");
                }

                return basePath + "/" + (name ? name : "");
            }

            std::string MakePattern(const std::string &basePath)
            {
                if (basePath.empty())
                {
                    return "*";
                }

                if (basePath.back() == '/' || basePath.back() == '\\')
                {
                    return basePath + "*";
                }

                return basePath + "/*";
            }

            std::string MakeDirMarkerPath(const char *remoteDirPath)
            {
                std::string path(remoteDirPath ? remoteDirPath : "");
                if (path.empty())
                {
                    return path;
                }

                if (path.back() == '/' || path.back() == '\\')
                {
                    return path + ".adslite_dir_marker.tmp";
                }

                return path + "/.adslite_dir_marker.tmp";
            }

            struct RecursiveNode
            {
                std::string path;
                uint32_t depth;
                bool expanded;
            };

            static int64_t FileFindFirstInternal(IAdsBackend &backend,
                                                 uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 const char *pathPattern,
                                                 uint32_t findFlags,
                                                 AdsLiteFileFindData *pEntry,
                                                 uint32_t *pIsLast);

            static int64_t FileFindNextInternal(IAdsBackend &backend,
                                                uint16_t port,
                                                const AmsAddr *pAddr,
                                                AdsLiteFileFindData *pEntry,
                                                uint32_t *pIsLast);

            int64_t DeleteTreeInternal(IAdsBackend &backend,
                                       uint16_t port,
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

                std::vector<RecursiveNode> stack;
                stack.push_back({std::string(rootPath), 0u, false});

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

                        const int64_t deleteRet = FileDelete(backend,
                                                             port,
                                                             pAddr,
                                                             node.path.c_str(),
                                                             dirDeleteFlags);
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

                    const std::string pattern = MakePattern(node.path);
                    AdsLiteFileFindData entry = {};
                    uint32_t isLast = 0;
                    int64_t ret = FileFindFirstInternal(backend,
                                                        port,
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
                            const std::string childPath = JoinPath(node.path, entry.fileName);
                            if (IsDirectory(entry))
                            {
                                stack.push_back({childPath, node.depth + 1, false});
                            }
                            else
                            {
                                ret = FileDelete(backend,
                                                 port,
                                                 pAddr,
                                                 childPath.c_str(),
                                                 fileDeleteFlags);
                                if (ret != ADSERR_NOERR && ret != ADSERR_DEVICE_NOTFOUND)
                                {
                                    return ret;
                                }
                            }
                        }

                        ret = FileFindNextInternal(backend, port, pAddr, &entry, &isLast);
                        if (ret != ADSERR_NOERR)
                        {
                            return ret;
                        }
                    }
                }

                return ADSERR_NOERR;
            }

            static int64_t FileFindFirstInternal(IAdsBackend &backend,
                                                 uint16_t port,
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

                const size_t pathLen = std::strlen(pathPattern);
                if (pathLen == 0 || pathLen > UINT32_MAX - 1)
                {
                    return ADSERR_CLIENT_INVALIDPARM;
                }

                const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);

                pEntry->fileHandle = findFlags ? findFlags : FILE_FIND_GENERIC;
                const int64_t ret = backend.SyncReadWriteReq(port,
                                                             &serviceAddr,
                                                             SYSTEMSERVICE_FFILEFIND,
                                                             pEntry->fileHandle,
                                                             sizeof(*pEntry),
                                                             pEntry,
                                                             static_cast<uint32_t>(pathLen + 1),
                                                             pathPattern,
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

            static int64_t FileFindNextInternal(IAdsBackend &backend,
                                                uint16_t port,
                                                const AmsAddr *pAddr,
                                                AdsLiteFileFindData *pEntry,
                                                uint32_t *pIsLast)
            {
                if (!pAddr || !pEntry)
                {
                    return ADSERR_CLIENT_INVALIDPARM;
                }

                const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);

                const int64_t ret = backend.SyncReadWriteReq(port,
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
        }

        int64_t FileOpen(IAdsBackend &backend,
                         uint16_t port,
                         const AmsAddr *pAddr,
                         const char *remotePath,
                         uint32_t openFlags,
                         uint32_t *pFileHandle)
        {
            if (!pAddr || !remotePath || !pFileHandle)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const size_t pathLen = std::strlen(remotePath);
            if (pathLen == 0 || pathLen > UINT32_MAX - 1)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);

            return backend.SyncReadWriteReq(port,
                                            &serviceAddr,
                                            SYSTEMSERVICE_FOPEN,
                                            openFlags,
                                            sizeof(uint32_t),
                                            pFileHandle,
                                            static_cast<uint32_t>(pathLen + 1),
                                            remotePath,
                                            nullptr);
        }

        int64_t FileClose(IAdsBackend &backend,
                          uint16_t port,
                          const AmsAddr *pAddr,
                          uint32_t fileHandle)
        {
            if (!pAddr)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);

            return backend.SyncReadWriteReq(port,
                                            &serviceAddr,
                                            SYSTEMSERVICE_FCLOSE,
                                            fileHandle,
                                            0,
                                            nullptr,
                                            0,
                                            nullptr,
                                            nullptr);
        }

        int64_t FileRead(IAdsBackend &backend,
                         uint16_t port,
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

            return backend.SyncReadWriteReq(port,
                                            &serviceAddr,
                                            SYSTEMSERVICE_FREAD,
                                            fileHandle,
                                            length,
                                            pData,
                                            0,
                                            nullptr,
                                            pBytesRead);
        }

        int64_t FileWrite(IAdsBackend &backend,
                          uint16_t port,
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

            return backend.SyncReadWriteReq(port,
                                            &serviceAddr,
                                            SYSTEMSERVICE_FWRITE,
                                            fileHandle,
                                            0,
                                            nullptr,
                                            length,
                                            pData,
                                            nullptr);
        }

        int64_t FileDelete(IAdsBackend &backend,
                           uint16_t port,
                           const AmsAddr *pAddr,
                           const char *remotePath,
                           uint32_t deleteFlags)
        {
            if (!pAddr || !remotePath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const size_t pathLen = std::strlen(remotePath);
            if (pathLen == 0 || pathLen > UINT32_MAX - 1)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const AmsAddr serviceAddr = BuildFileServiceAddr(pAddr);

            return backend.SyncReadWriteReq(port,
                                            &serviceAddr,
                                            SYSTEMSERVICE_FDELETE,
                                            deleteFlags,
                                            0,
                                            nullptr,
                                            static_cast<uint32_t>(pathLen + 1),
                                            remotePath,
                                            nullptr);
        }

        int64_t DirCreate(IAdsBackend &backend,
                          uint16_t port,
                          const AmsAddr *pAddr,
                          const char *remoteDirPath)
        {
            if (!pAddr || !remoteDirPath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const std::string markerPath = MakeDirMarkerPath(remoteDirPath);
            if (markerPath.empty())
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const uint32_t createFlags = ADSLITE_FOPEN_WRITE |
                                         ADSLITE_FOPEN_BINARY |
                                         ADSLITE_FOPEN_PLUS |
                                         ADSLITE_FOPEN_ENSURE_DIR;
            const uint32_t fallbackFlags = ADSLITE_FOPEN_APPEND |
                                           ADSLITE_FOPEN_BINARY |
                                           ADSLITE_FOPEN_PLUS |
                                           ADSLITE_FOPEN_ENSURE_DIR;

            uint32_t handle = 0;
            int64_t ret = FileOpen(backend,
                                   port,
                                   pAddr,
                                   markerPath.c_str(),
                                   createFlags,
                                   &handle);
            if (ret == ADSERR_DEVICE_NOTFOUND)
            {
                ret = FileOpen(backend,
                               port,
                               pAddr,
                               markerPath.c_str(),
                               fallbackFlags,
                               &handle);
            }
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            const int64_t closeRet = FileClose(backend, port, pAddr, handle);
            if (closeRet != ADSERR_NOERR)
            {
                return closeRet;
            }

            ret = FileDelete(backend,
                             port,
                             pAddr,
                             markerPath.c_str(),
                             ADSLITE_FOPEN_READ);
            if (ret == ADSERR_NOERR || ret == ADSERR_DEVICE_NOTFOUND)
            {
                return ADSERR_NOERR;
            }
            return ret;
        }

        int64_t DirDelete(IAdsBackend &backend,
                          uint16_t port,
                          const AmsAddr *pAddr,
                          const char *remoteDirPath,
                          bool deleteDirSelf)
        {
            if (!pAddr || !remoteDirPath)
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const int64_t ret = DeleteTreeInternal(backend,
                                                   port,
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

        int64_t FileRename(IAdsBackend &backend,
                           uint16_t port,
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
            int64_t ret = FileOpen(backend, port, pAddr, sourcePath, readFlags, &srcHandle);
            if (ret != ADSERR_NOERR)
            {
                return ret;
            }

            uint32_t dstHandle = 0;
            ret = FileOpen(backend, port, pAddr, targetPath, writeFlags, &dstHandle);
            if (ret != ADSERR_NOERR)
            {
                FileClose(backend, port, pAddr, srcHandle);
                return ret;
            }

            std::vector<uint8_t> buffer(64u * 1024u);
            while (true)
            {
                uint32_t bytesRead = 0;
                ret = FileRead(backend,
                               port,
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

                ret = FileWrite(backend, port, pAddr, dstHandle, buffer.data(), bytesRead);
                if (ret != ADSERR_NOERR)
                {
                    break;
                }
            }

            const int64_t closeDst = FileClose(backend, port, pAddr, dstHandle);
            const int64_t closeSrc = FileClose(backend, port, pAddr, srcHandle);

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

            ret = FileDelete(backend, port, pAddr, sourcePath, ADSLITE_FOPEN_READ);
            if (ret == ADSERR_NOERR || ret == ADSERR_DEVICE_NOTFOUND)
            {
                return ADSERR_NOERR;
            }
            return ret;
        }

        int64_t FileList(IAdsBackend &backend,
                         uint16_t port,
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
            int64_t ret = FileFindFirstInternal(backend, port, pAddr, pathPattern, findFlags, &entry, &isLast);
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

                ret = FileFindNextInternal(backend, port, pAddr, &entry, &isLast);
                if (ret != ADSERR_NOERR)
                {
                    return ret;
                }
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

    }
}
