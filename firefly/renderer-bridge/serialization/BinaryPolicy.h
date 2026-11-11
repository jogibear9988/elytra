/** Public cross-world copy budgets charge distinct complete backing stores, not just view slices.
 */
#pragma once
#include "BridgeData.h"

namespace Firefly {
inline BridgeData::Limits public_binary_limits()
{
    BridgeData::Limits limits;
    limits.binary_bytes = 64 * 1024;
    return limits;
}

// Code-OSS includes its genuine translation catalog in window configuration.
// These bounded logical messages travel as ordinary small native byte frames;
// they do not change contextBridge publication or utility-process policy.
inline BridgeData::Limits application_ipc_limits()
{
    BridgeData::Limits limits;
    limits.values = 65536;
    limits.properties = 131072;
    limits.string_code_units = 4 * 1024 * 1024;
    limits.array_length = 65536;
    limits.total_array_length = 131072;
    limits.binary_bytes = 1024 * 1024;
    limits.wire_bytes = 16 * 1024 * 1024;
    limits.wire_profile = BridgeData::WireProfile::ApplicationIpc;
    return limits;
}

// Cross-world copies need full application configuration too, but retain the
// same 1 MiB binary ceiling as application IPC and retain all publication,
// type and authority restrictions. Public ports and utility copies stay separate.
// This is a capture-only profile; it does not select the larger wire envelope.
inline BridgeData::Limits context_bridge_limits()
{
    BridgeData::Limits limits;
    limits.values = 65536;
    limits.properties = 131072;
    limits.string_code_units = 4 * 1024 * 1024;
    limits.array_length = 65536;
    limits.total_array_length = 131072;
    limits.binary_bytes = 1024 * 1024;
    return limits;
}

}
