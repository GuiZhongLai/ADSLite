#pragma once

#include "../AdsLiteDef.h"
#include "IAdsBackend.h"

#include <cstdint>

namespace adslite
{
    namespace process
    {
        class StartProcessClient
        {
        public:
            explicit StartProcessClient(IAdsBackend &backend);

            // 在目标设备上启动进程（走系统服务口 10000 / SYSTEMSERVICE_STARTPROCESS）。
            // applicationPath 必填；commandLine / workingDirectory 可为 nullptr（按空串发送）。
            // flags 目前支持 ADSLITE_STARTPROCESS_HIDDEN（Windows 目标隐藏窗口）。
            int64_t StartProcess(uint16_t port,
                                 const AmsAddr *pAddr,
                                 const char *applicationPath,
                                 const char *commandLine,
                                 const char *workingDirectory,
                                 uint32_t flags);

        private:
            static constexpr uint16_t kSystemServicePort = 10000;

            static AmsAddr BuildSystemServiceAddr(const AmsAddr *pAddr);

            IAdsBackend &backend_;
        };
    }
}
