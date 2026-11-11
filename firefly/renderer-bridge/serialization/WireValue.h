/** Private main-host wire storage is an exact native byte view, never a text representation. */
#pragma once
#include "BridgeData.h"

namespace Firefly {
JS::ThrowCompletionOr<JS::Value> wire_value(JS::Realm&, ByteString const&);

// Only the document application host selects the larger logical-message bound.
// Utility and main-only port callers retain the single-frame default.
enum class WireValuePolicy { SingleFrame, ApplicationMessage };
ErrorOr<ByteString> wire_bytes(
    JS::Value, JS::Realm&, WireValuePolicy = WireValuePolicy::SingleFrame);
}
