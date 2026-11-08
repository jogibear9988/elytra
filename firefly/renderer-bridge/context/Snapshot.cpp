/** Keep redaction scoped to contextBridge; raw IPC snapshots remain data-only and strict. */
#include "Snapshot.h"
#include "worlds/IsolatedWorld.h"

namespace Firefly {
ErrorOr<BridgeData> capture_to_principal(
    IsolatedWorld& world, JS::Value value, BridgeData::CaptureFunction functions)
{
    if (world.is_detached())
        return Error::from_string_literal("Cannot snapshot a detached world");
    return BridgeData::capture(value, *world.realm(), {}, move(functions),
        [&](JS::Object& object) { return world.object_export_redacted(object); });
}
}
