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
}
