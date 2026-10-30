#include "backend/BackendServices.h"

#include "backend/BackendSelector.h"

namespace adslite
{
    namespace backend
    {
        BackendServices::BackendServices(IAdsBackend &backend)
            : backend_(backend),
              targetSystemInfo_(backend),
              fileServiceClient_(backend, targetSystemInfo_),
              startProcessClient_(backend),
              ecatClient_(backend)
        {
        }

        IAdsBackend &BackendServices::Backend()
        {
            return backend_;
        }

        targetinfo::TargetSystemInfo &BackendServices::TargetInfo()
        {
            return targetSystemInfo_;
        }

        file::FileServiceClient &BackendServices::FileService()
        {
            return fileServiceClient_;
        }

        process::StartProcessClient &BackendServices::StartProcess()
        {
            return startProcessClient_;
        }

        ecat::ECatClient &BackendServices::ECat()
        {
            return ecatClient_;
        }

        BackendServices &GetBackendServices()
        {
            static BackendServices services(GetBackend());
            return services;
        }
    }
}
