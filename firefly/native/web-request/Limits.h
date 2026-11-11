/** Logical identities outlive individual decision slots; active and wire quotas stay separate. */
#pragma once
#include "native/platform/Diagnostics.h"
#include <AK/Types.h>
#include <cstdio>

namespace Firefly::WebRequest {
inline constexpr size_t logical_request_limit = 16384;
enum class LogicalLimitStage : u8 { Gate, Session, Broker };

inline void logical_limit_diagnostic(LogicalLimitStage stage, size_t retained)
{
    static constexpr char const* labels[] = { "gate", "session", "broker" };
    static size_t emitted[3] {};
    auto index = static_cast<size_t>(stage);
    if (!ipc_bytes_diagnostics_enabled() || index >= 3 || emitted[index] >= 4)
        return;
    ++emitted[index];
    std::fprintf(stderr, "FIREFLY_WEB_REQUEST_LOGICAL_LIMIT stage=%s count=%zu limit=%zu\n",
        labels[index], retained, logical_request_limit);
}

// One-time former-boundary observations distinguish real retention pressure
// from a different module failure without recording any resource identity.
inline void logical_retention_diagnostic(LogicalLimitStage stage, size_t retained)
{
    static constexpr char const* labels[] = { "gate", "session", "broker" };
    static bool emitted[3][2] {};
    auto index = static_cast<size_t>(stage);
    if (!ipc_bytes_diagnostics_enabled() || index >= 3 || (retained != 1024 && retained != 1025))
        return;
    auto boundary = retained == 1024 ? 0 : 1;
    if (emitted[index][boundary])
        return;
    emitted[index][boundary] = true;
    std::fprintf(stderr, "FIREFLY_WEB_REQUEST_LOGICAL_RETAINED stage=%s count=%zu limit=%zu\n",
        labels[index], retained, logical_request_limit);
}

}
