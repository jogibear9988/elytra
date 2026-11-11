/** Opt-in bounded native stages only; never log URLs, headers, payloads or exception text. */
#pragma once
#include "native/platform/Diagnostics.h"
#include <AK/Types.h>
#include <cstdio>
#include <cstdlib>

namespace Firefly::Protocols {
enum class DiagnosticStage : u8 {
    ClientAdmitted,
    ClientScope,
    ClientQuota,
    ClientUrl,
    ClientSent,
    ClientRevoke,
    ClientPacket,
    ClientUnknown,
    ClientResponse,
    ClientDeadline,
    DispatchNoProvider,
    DispatchScope,
    DispatchHandler,
    ResponseFail,
    ResponseCancel,
    ResponseFrame,
    ResponseCors,
    ResponseScriptMime,
    ServerPacket,
    ServerQuota,
    ServerAdmit,
    ServerAuthorize,
    ServerMetadata,
    RouteScope,
    RouteMissing,
    RouteResponse,
    Count
};

inline void diagnostic(DiagnosticStage stage, size_t count = 0, int native_errno = 0)
{
    static constexpr char const* names[] = { "client-admitted", "client-scope", "client-quota",
        "client-url", "client-sent", "client-revoke", "client-packet", "client-unknown",
        "client-response", "client-deadline", "dispatch-no-provider", "dispatch-scope",
        "dispatch-handler", "response-fail", "response-cancel", "response-frame", "response-cors",
        "response-script-mime", "server-packet", "server-quota", "server-admit", "server-authorize",
        "server-metadata", "route-scope", "route-missing", "route-response" };
    static_assert(sizeof(names) / sizeof(names[0]) == static_cast<size_t>(DiagnosticStage::Count));

    static size_t emitted[static_cast<size_t>(DiagnosticStage::Count)] {};
    auto index = static_cast<size_t>(stage);

    if (!ipc_bytes_diagnostics_enabled() || index >= static_cast<size_t>(DiagnosticStage::Count)
        || emitted[index] >= 4)
        return;
    ++emitted[index];
    std::fprintf(stderr, "FIREFLY_PROTOCOL_DIAGNOSTIC stage=%s count=%zu errno=%d\n", names[index],
        count, native_errno);
}
}
