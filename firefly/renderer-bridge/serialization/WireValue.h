/** Private main-host wire storage is an exact native byte view, never a text representation. */
#pragma once
#include "BridgeData.h"

namespace Firefly {
JS::ThrowCompletionOr<JS::Value> wire_value(JS::Realm&, ByteString const&);

// Native callsites select independent document-message and port-message bounds;
// other callers retain the physical single-frame default.
enum class WireValuePolicy { SingleFrame, ApplicationMessage, PortMessage };
ErrorOr<ByteString> wire_bytes(
    JS::Value, JS::Realm&, WireValuePolicy = WireValuePolicy::SingleFrame);
}
