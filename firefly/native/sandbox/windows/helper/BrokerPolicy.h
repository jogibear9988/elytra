/** Native broker admission: untrusted remote Park records only, never Browser-origin sends.
 * Browser-origin retention/backpressure remains a separate Windows desktop blocker.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

namespace Firefly::WindowsBroker {
inline constexpr std::size_t owner_tokens = 64;
inline constexpr std::size_t owner_handles = 32768;
inline constexpr std::size_t remote_tokens = 192;
inline constexpr std::size_t remote_handles = 98304;

constexpr bool valid_handle_value(std::uint64_t value)
{
    // The SDK represents handles through signed int descriptors. Reject null,
    // every negative/pseudo handle and truncation aliases before DuplicateHandle.
    return value > 0 && value <= static_cast<std::uint64_t>((std::numeric_limits<int>::max)());
}

template <typename Map>
bool can_park(Map const& records, int browser, int owner, std::size_t incoming)
{
    if (owner == browser)
        return true; // Never turn remote saturation into a Browser send_sync MUST failure.
    if (owner <= 0 || incoming > owner_handles)
        return false;
    std::size_t own_count = 0, own_size = 0, total_count = 0, total_size = 0;
    for (auto const& entry : records) {
        auto const& record = entry.value;
        if (record.owner == browser)
            continue;
        auto size = record.attachments.size();

        if (++total_count >= remote_tokens || size > remote_handles - total_size)
            return false;
        total_size += size;
        if (record.owner == owner) {
            if (++own_count >= owner_tokens || size > owner_handles - own_size)
                return false;
            own_size += size;
        }
    }

    return incoming <= owner_handles - own_size && incoming <= remote_handles - total_size;
}
}
