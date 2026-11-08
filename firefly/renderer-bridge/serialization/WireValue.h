/** Private main-host wire storage is an exact native byte view, never a text representation. */
#pragma once
#include "BridgeData.h"

namespace Firefly {
JS::ThrowCompletionOr<JS::Value> wire_value(JS::Realm&, ByteString const&);

ErrorOr<ByteString> wire_bytes(JS::Value, JS::Realm&);
}
