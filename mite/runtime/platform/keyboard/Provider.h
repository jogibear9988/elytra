/** Actual OS keyboard snapshots; no VM, Qt, LibWeb, Node or V8 dependency. */
#pragma once
#include <AK/Error.h>
#include <AK/JsonObject.h>

namespace Mite::Keyboard {
// Throws no callbacks and changes no OS setting. Non-macOS providers fail explicitly.
ErrorOr<JsonObject> snapshot();
}
