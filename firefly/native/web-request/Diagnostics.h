/** Opt-in bounded rejection phases; never include application metadata. */
#pragma once
#include "native/platform/Diagnostics.h"
#include <cstdio>

namespace Firefly::WebRequest {
inline void rejection_diagnostic(char const* stage, unsigned reason)
{
    static thread_local unsigned reports = 0;
    if (!ipc_bytes_diagnostics_enabled() || reports >= 128)
        return;
    std::fprintf(stderr, "FIREFLY_WEB_REQUEST_REJECT stage=%s reason=%u report=%u\n", stage, reason,
        ++reports);
}
}
