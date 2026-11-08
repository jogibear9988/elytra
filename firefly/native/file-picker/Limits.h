/** Shared native selection budgets; no Qt, Mite or renderer dependency. */
#pragma once
#include <AK/Types.h>

namespace Firefly::FileSelectionPolicy {
constexpr size_t max_files = 16;
constexpr size_t max_file_bytes = 4 * 1024 * 1024;
constexpr size_t max_total_bytes = 16 * 1024 * 1024;
}
