/** Trusted per-WebContents autoplay policy; no origin-global browser settings. */
#pragma once
#include <AK/Error.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibWeb/Export.h>

namespace Web::DOM {
class Document;
}

namespace Firefly::Autoplay {
enum class Policy : u8 {
    NoUserGestureRequired,
    UserGestureRequired,
    UpstreamDefault,
    // Internal fail-closed result; never accepted in a configuration snapshot.
    Blocked,
};

struct Configuration {
    // Canonical native top-level CrossProcessId: "namespace-local" (positive u64s).
    String context;
    u32 generation;
    Policy policy;
};

// Owned helpers deny missing contexts even before their first trusted snapshot.
WEB_API void initialize_owned();

WEB_API ErrorOr<void> apply(Vector<Configuration>);

WEB_API Optional<String> context_for(Web::DOM::Document const&);

WEB_API Optional<Policy> policy_for(Web::DOM::Document const&);

// Opaque admission identity: changed/removed policies cannot reuse an old audio unlock.
WEB_API u64 revision_for(Web::DOM::Document const&);

WEB_API void shutdown();
}
