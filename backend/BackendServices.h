#pragma once

#include "AdsECatService.h"
#include "AdsFileService.h"
#include "AdsStartProcessService.h"
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
            process::StartProcessClient &StartProcess();
            ecat::ECatClient &ECat();

        private:
            IAdsBackend &backend_;
            targetinfo::TargetSystemInfo targetSystemInfo_;
            file::FileServiceClient fileServiceClient_;
            process::StartProcessClient startProcessClient_;
            ecat::ECatClient ecatClient_;
        };

        BackendServices &GetBackendServices();
    }
}
