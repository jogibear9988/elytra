/** Source-to-principal snapshots apply the owning world's native capability redaction. */
#pragma once
#include "serialization/BridgeData.h"

namespace Firefly {
class IsolatedWorld;
ErrorOr<BridgeData> capture_to_principal(
    IsolatedWorld&, JS::Value, BridgeData::CaptureFunction = {});
}
