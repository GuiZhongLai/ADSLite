#pragma once

#include "AdsFileService.h"
#include "IAdsBackend.h"
#include "TargetSystemInfo.h"

namespace adslite
{
    namespace backend
    {
        class BackendServices
        {
        public:
            explicit BackendServices(IAdsBackend &backend);

            IAdsBackend &Backend();
            targetinfo::TargetSystemInfo &TargetInfo();
            file::FileServiceClient &FileService();

        private:
            IAdsBackend &backend_;
            targetinfo::TargetSystemInfo targetSystemInfo_;
            file::FileServiceClient fileServiceClient_;
        };

        BackendServices &GetBackendServices();
    }
}
