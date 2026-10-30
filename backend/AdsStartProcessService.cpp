#include "backend/AdsStartProcessService.h"

#include "../standalone/wrap_endian.h"
#include "standalone/Log.h"

#include <cstring>
#include <limits>
#include <vector>

namespace adslite
{
    namespace process
    {
        StartProcessClient::StartProcessClient(IAdsBackend &backend)
            : backend_(backend)
        {
        }

        AmsAddr StartProcessClient::BuildSystemServiceAddr(const AmsAddr *pAddr)
        {
            AmsAddr serviceAddr = *pAddr;
            // 与文件服务一致：系统服务固定走 10000，避免调用方传入 PLC 口（如 851）。
            serviceAddr.port = kSystemServicePort;
            return serviceAddr;
        }

        int64_t StartProcessClient::StartProcess(uint16_t port,
                                                 const AmsAddr *pAddr,
                                                 const char *applicationPath,
                                                 const char *commandLine,
                                                 const char *workingDirectory,
                                                 uint32_t flags)
        {
            if (!pAddr || !applicationPath || applicationPath[0] == '\0')
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            const char *safeCommandLine = commandLine ? commandLine : "";
            const char *safeWorkingDirectory = workingDirectory ? workingDirectory : "";

            const size_t appLen = std::strlen(applicationPath);
            const size_t dirLen = std::strlen(safeWorkingDirectory);
            const size_t cmdLen = std::strlen(safeCommandLine);

            if (appLen > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
                dirLen > static_cast<size_t>(std::numeric_limits<uint32_t>::max()) ||
                cmdLen > static_cast<size_t>(std::numeric_limits<uint32_t>::max()))
            {
                return ADSERR_CLIENT_INVALIDPARM;
            }

            // AdsTool 布局：3 个 LE uint32（不含 NUL 的长度）+ path\0 + cwd\0 + args\0
            const uint32_t leAppLen = bhf::ads::htole(static_cast<uint32_t>(appLen));
            const uint32_t leDirLen = bhf::ads::htole(static_cast<uint32_t>(dirLen));
            const uint32_t leCmdLen = bhf::ads::htole(static_cast<uint32_t>(cmdLen));

            std::vector<uint8_t> payload;
            payload.reserve(sizeof(uint32_t) * 3 + appLen + dirLen + cmdLen + 3);

            const uint8_t *headerBytes = reinterpret_cast<const uint8_t *>(&leAppLen);
            payload.insert(payload.end(), headerBytes, headerBytes + sizeof(uint32_t));
            headerBytes = reinterpret_cast<const uint8_t *>(&leDirLen);
            payload.insert(payload.end(), headerBytes, headerBytes + sizeof(uint32_t));
            headerBytes = reinterpret_cast<const uint8_t *>(&leCmdLen);
            payload.insert(payload.end(), headerBytes, headerBytes + sizeof(uint32_t));

            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t *>(applicationPath),
                           reinterpret_cast<const uint8_t *>(applicationPath) + appLen);
            payload.push_back(0);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t *>(safeWorkingDirectory),
                           reinterpret_cast<const uint8_t *>(safeWorkingDirectory) + dirLen);
            payload.push_back(0);
            payload.insert(payload.end(),
                           reinterpret_cast<const uint8_t *>(safeCommandLine),
                           reinterpret_cast<const uint8_t *>(safeCommandLine) + cmdLen);
            payload.push_back(0);

            // AdsTool: indexOffset = hidden ? (1 << 16) : 0
            const uint32_t indexOffset =
                ((flags & ADSLITE_STARTPROCESS_HIDDEN) != 0) ? (1u << 16) : 0u;

            const AmsAddr serviceAddr = BuildSystemServiceAddr(pAddr);
            const int64_t ret = backend_.SyncWriteReq(port,
                                                      &serviceAddr,
                                                      SYSTEMSERVICE_STARTPROCESS,
                                                      indexOffset,
                                                      static_cast<uint32_t>(payload.size()),
                                                      payload.data());
            if (ret != ADSERR_NOERR)
            {
                LOG_WARN("StartProcess failed ret=0x" << std::hex << ret
                                                     << ", path=" << applicationPath);
            }
            return ret;
        }
    }
}
