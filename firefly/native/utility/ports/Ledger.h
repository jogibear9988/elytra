/** Native-only flow control. No JavaScript values, callbacks, or GC ownership cross this ledger. */
#pragma once
#include "Protocol.h"
#include <deque>
#include <unordered_map>

namespace Firefly::Utility::Ports {
class Ledger {
public:
    enum class State { Live, Retired, Invalid };
    State state(Identity) const;

    bool prepare_transfer(size_t, std::vector<Identity>&) const;

    bool admit_transfer(std::span<Identity const>);

    bool can_send(Identity, size_t) const;

    void sent(Identity, size_t);

    bool received(Identity, size_t);

    bool acknowledge(Identity, size_t);

    bool can_consume(Identity, size_t) const;

    void consumed(Identity, size_t);

    void close(Identity);

    void retire_all();

    void clear();

private:
    struct Route {
        std::deque<size_t> sent;
        std::deque<size_t> received;
        size_t sent_bytes {};
        size_t received_bytes {};
    };

    static constexpr size_t route_limit = 256;
    static constexpr size_t message_limit = 16;
    static constexpr size_t route_bytes_limit = 1024 * 1024;
    // Each peer can reserve its entire TX window before reading. Separate
    // directional windows make legitimate simultaneous duplex bursts admissible,
    // while the native endpoint retains at most 8 MiB across both directions.
    static constexpr size_t direction_bytes_limit = 4 * 1024 * 1024;
    std::unordered_map<uint32_t, Route> m_routes;
    uint32_t m_last {};
    size_t m_sent_bytes {};
    size_t m_received_bytes {};
};
}
