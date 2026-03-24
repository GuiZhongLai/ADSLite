#pragma once

#include "../AdsLiteDef.h"
#include "IAdsBackend.h"

#include <cstdint>

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

        int64_t FileOpen(IAdsBackend &backend,
                         uint16_t port,
                         const AmsAddr *pAddr,
                         const char *remotePath,
                         uint32_t openFlags,
                         uint32_t *pFileHandle);

        int64_t FileClose(IAdsBackend &backend,
                          uint16_t port,
                          const AmsAddr *pAddr,
                          uint32_t fileHandle);

        int64_t FileRead(IAdsBackend &backend,
                         uint16_t port,
                         const AmsAddr *pAddr,
                         uint32_t fileHandle,
                         uint32_t length,
                         void *pData,
                         uint32_t *pBytesRead);

        int64_t FileWrite(IAdsBackend &backend,
                          uint16_t port,
                          const AmsAddr *pAddr,
                          uint32_t fileHandle,
                          const void *pData,
                          uint32_t length);

        int64_t FileDelete(IAdsBackend &backend,
                           uint16_t port,
                           const AmsAddr *pAddr,
                           const char *remotePath,
                           uint32_t deleteFlags);

        int64_t DirCreate(IAdsBackend &backend,
                          uint16_t port,
                          const AmsAddr *pAddr,
                          const char *remoteDirPath);

        int64_t DirDelete(IAdsBackend &backend,
                          uint16_t port,
                          const AmsAddr *pAddr,
                          const char *remoteDirPath,
                          bool deleteDirSelf);

        int64_t FileRename(IAdsBackend &backend,
                           uint16_t port,
                           const AmsAddr *pAddr,
                           const char *sourcePath,
                           const char *targetPath);

        int64_t FileList(IAdsBackend &backend,
                         uint16_t port,
                         const AmsAddr *pAddr,
                         const char *pathPattern,
                         uint32_t findFlags,
                         char *pNameBuffer,
                         uint32_t nameBufferLength,
                         uint32_t *pBytesRequired,
                         uint32_t *pItemCount);

    }
}
