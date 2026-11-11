/** Framework resource policy, independent of any application's menu contents. */
#pragma once
#include <cstddef>

namespace Firefly::MenuLimits {
inline constexpr size_t item_count = 2048;
inline constexpr size_t wire_bytes = 1024 * 1024;
}
