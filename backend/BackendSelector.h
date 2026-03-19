#pragma once

#include "backend/IAdsBackend.h"

namespace adslite
{
    namespace backend
    {
        IAdsBackend &GetBackend();
        const char *GetBackendName();
        const char *GetBackendSelectionReason();
    }
}
