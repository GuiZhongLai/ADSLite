#include "backend/BackendServices.h"

#include "backend/BackendSelector.h"

namespace adslite
{
    namespace backend
    {
        BackendServices::BackendServices(IAdsBackend &backend)
            : backend_(backend),
              targetSystemInfo_(backend),
              fileServiceClient_(backend, targetSystemInfo_)
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

        BackendServices &GetBackendServices()
        {
            static BackendServices services(GetBackend());
            return services;
        }
    }
}
