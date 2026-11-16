/** Logical DATA is reserved once; physical fragments never acknowledge delivery. */
#pragma once
#include "Ledger.h"
#include <chrono>
#include <unordered_map>

namespace Firefly::Utility::Ports {
constexpr size_t fragment_header_size = 12;
constexpr size_t fragment_payload_limit = physical_limit - header_size - fragment_header_size;

// Invalid/empty/oversized input returns zero; ordinary DATA occupies one frame.
size_t fragment_count(size_t);

size_t fragment_wire_bytes(size_t);

struct FragmentFrame {
    std::vector<char> bytes;
    size_t next_offset {};
    bool complete {};
};

// Invalid input yields an empty frame. Small DATA remains the original envelope.
FragmentFrame encode_next(Identity, uint32_t serial, std::span<char const>, size_t offset);

class Reassembler {
public:
    using Clock = std::chrono::steady_clock;
    Reassembler() = default;
    Reassembler(Reassembler const&) = delete;
    Reassembler& operator=(Reassembler const&) = delete;
    Reassembler(Reassembler&&) = default;
    Reassembler& operator=(Reassembler&&) = default;
    enum class Status { Invalid, Pending, Complete };

    struct Result {
        Status status { Status::Invalid };
        std::vector<char> payload;
    };

    // Fragment packets only. The first valid fragment debits received(total)
    // BEFORE full-buffer allocation. Complete payload already owns that debit:
    // caller must NOT call received() again; ACK only after actual consumption.
    // Invalid retains any previous reservation until caller closes the ledger
    // route/channel. close/clear release storage, not the ledger's reservation.
    Result accept(Packet const&, Ledger&, Clock::time_point = Clock::now());

    bool active(Identity) const;

    bool expired(Clock::time_point = Clock::now()) const;

    void close(Identity);

    void clear();

private:
    struct State {
        Identity identity {};
        uint32_t last_serial {};
        uint32_t serial {};
        uint32_t total {};
        size_t offset {};
        Clock::time_point deadline {};
        std::vector<char> bytes;
    };

    std::unordered_map<uint32_t, State> m_routes;
};
}
