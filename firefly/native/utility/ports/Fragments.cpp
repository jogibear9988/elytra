/** Bounded native reassembly, independent of VM/GC and transport ownership. */
#include "Fragments.h"
#include <algorithm>

namespace Firefly::Utility::Ports {
namespace {
    bool same(Identity left, Identity right)
    {
        return left.id == right.id && left.generation == right.generation;
    }

    bool valid(Identity route)
    {
        return route.id && route.id <= identity_limit && route.generation
            && route.generation <= identity_limit;
    }

    uint32_t read32(std::span<char const> bytes, size_t offset)
    {
        return read_size(bytes.subspan(offset, 4));
    }
}

size_t fragment_count(size_t size)
{
    if (!size || size > logical_limit)
        return 0;
    if (size <= physical_limit - header_size)
        return 1;
    return (size + fragment_payload_limit - 1) / fragment_payload_limit;
}

size_t fragment_wire_bytes(size_t size)
{
    auto count = fragment_count(size);
    if (!count)
        return 0;
    return size + count * (header_size + (count == 1 ? 0 : fragment_header_size));
}

FragmentFrame encode_next(
    Identity route, uint32_t serial, std::span<char const> payload, size_t offset)
{
    if (!valid(route) || !fragment_count(payload.size()) || offset >= payload.size())
        return {};
    if (payload.size() <= physical_limit - header_size) {
        if (offset)
            return {};
        return { encode(Kind::Data, route, {}, payload), payload.size(), true };
    }

    if (!serial || offset % fragment_payload_limit)
        return {};
    auto count = std::min(fragment_payload_limit, payload.size() - offset);
    std::vector<char> fragment;
    fragment.reserve(fragment_header_size + count);
    for (auto field :
        { serial, static_cast<uint32_t>(payload.size()), static_cast<uint32_t>(offset) }) {
        auto bytes = size_payload(field);
        fragment.insert(fragment.end(), bytes.begin(), bytes.end());
    }

    fragment.insert(fragment.end(), payload.begin() + offset, payload.begin() + offset + count);
    auto next = offset + count;
    return { encode(Kind::Fragment, route, {}, fragment), next, next == payload.size() };
}

Reassembler::Result Reassembler::accept(Packet const& packet, Ledger& ledger, Clock::time_point now)
{
    if (packet.kind != Kind::Fragment || !valid(packet.route) || !packet.ports.empty()
        || packet.payload.size() <= fragment_header_size
        || packet.payload.size() > physical_limit - header_size
        || ledger.state(packet.route) != Ledger::State::Live)
        return {};
    auto serial = read32(packet.payload, 0);
    auto total = read32(packet.payload, 4);
    auto offset = read32(packet.payload, 8);
    auto chunk = packet.payload.subspan(fragment_header_size);
    if (!serial || total <= physical_limit - header_size || total > logical_limit || offset >= total
        || chunk.size() > total - offset)
        return {};
    auto found = m_routes.find(packet.route.id);
    if (found != m_routes.end() && !same(found->second.identity, packet.route))
        return {};
    if (found == m_routes.end() || found->second.bytes.empty()) {
        if (offset || (found != m_routes.end() && serial <= found->second.last_serial))
            return {};
        // This is the only receive debit for the whole logical message. Neither
        // continuation fragments nor completion acknowledge or reserve again.
        if (!ledger.received(packet.route, total))
            return {};
        auto& state = m_routes[packet.route.id];
        state.identity = packet.route;
        state.serial = serial;
        state.total = total;
        state.offset = 0;
        state.deadline = now + std::chrono::seconds(30);
        state.bytes.resize(total);
        found = m_routes.find(packet.route.id);
    }

    auto& state = found->second;
    if (now >= state.deadline || state.serial != serial || state.total != total
        || state.offset != offset)
        return {};
    std::copy(chunk.begin(), chunk.end(), state.bytes.begin() + offset);
    state.offset += chunk.size();
    if (state.offset != state.total)
        return { Status::Pending, {} };
    state.last_serial = state.serial;
    auto bytes = std::move(state.bytes);
    state.bytes.clear();
    state.serial = 0;
    state.total = 0;
    state.offset = 0;
    return { Status::Complete, std::move(bytes) };
}

bool Reassembler::active(Identity identity) const
{
    auto found = m_routes.find(identity.id);
    return found != m_routes.end() && same(found->second.identity, identity)
        && !found->second.bytes.empty();
}

bool Reassembler::expired(Clock::time_point now) const
{
    return std::ranges::any_of(m_routes, [&](auto const& entry) {
        return !entry.second.bytes.empty() && now >= entry.second.deadline;
    });
}

void Reassembler::close(Identity identity)
{
    auto found = m_routes.find(identity.id);
    if (found != m_routes.end() && same(found->second.identity, identity))
        m_routes.erase(found);
}

void Reassembler::clear()
{
    m_routes.clear();
}
}
