/** Credits are refunded only by exact FIFO consumption acknowledgements, never by JS claims alone.
 */
#include "Ledger.h"
#include <cassert>

namespace Firefly::Utility::Ports {
Ledger::State Ledger::state(Identity route) const
{
    if (!route.id || route.id > m_last || route.generation != 1)
        return State::Invalid;
    return m_routes.contains(route.id) ? State::Live : State::Retired;
}

bool Ledger::prepare_transfer(size_t count, std::vector<Identity>& identities) const
{
    if (!count || count > 16 || count > route_limit - m_routes.size()
        || count > identity_limit - m_last)
        return false;
    identities.clear();
    for (size_t index = 0; index < count; ++index)
        identities.push_back({ m_last + static_cast<uint32_t>(index) + 1, 1 });
    return true;
}

bool Ledger::admit_transfer(std::span<Identity const> identities)
{
    std::vector<Identity> expected;
    if (!prepare_transfer(identities.size(), expected))
        return false;
    for (size_t index = 0; index < identities.size(); ++index)
        if (identities[index].id != expected[index].id || identities[index].generation != 1)
            return false;
    for (auto identity : identities) {
        m_routes.emplace(identity.id, Route {});
        m_last = identity.id;
    }

    return true;
}

bool Ledger::can_send(Identity identity, size_t size) const
{
    if (state(identity) != State::Live)
        return false;
    auto const& route = m_routes.at(identity.id);
    return size <= frame_limit - header_size && route.sent.size() < message_limit
        && size <= route_bytes_limit - route.sent_bytes
        && size <= direction_bytes_limit - m_sent_bytes;
}

void Ledger::sent(Identity identity, size_t size)
{
    assert(can_send(identity, size));
    auto& route = m_routes.at(identity.id);
    route.sent.push_back(size);
    route.sent_bytes += size;
    m_sent_bytes += size;
}

bool Ledger::received(Identity identity, size_t size)
{
    if (state(identity) != State::Live)
        return false;
    auto& route = m_routes.at(identity.id);
    if (size > frame_limit - header_size || route.received.size() >= message_limit
        || size > route_bytes_limit - route.received_bytes
        || size > direction_bytes_limit - m_received_bytes)
        return false;
    route.received.push_back(size);
    route.received_bytes += size;
    m_received_bytes += size;
    return true;
}

bool Ledger::acknowledge(Identity identity, size_t size)
{
    if (state(identity) != State::Live)
        return false;
    auto& route = m_routes.at(identity.id);
    if (route.sent.empty() || route.sent.front() != size)
        return false;
    route.sent.pop_front();
    route.sent_bytes -= size;
    m_sent_bytes -= size;
    return true;
}

bool Ledger::can_consume(Identity identity, size_t size) const
{
    if (state(identity) != State::Live)
        return false;
    auto const& received = m_routes.at(identity.id).received;
    return !received.empty() && received.front() == size;
}

void Ledger::consumed(Identity identity, size_t size)
{
    assert(can_consume(identity, size));
    auto& route = m_routes.at(identity.id);
    route.received.pop_front();
    route.received_bytes -= size;
    m_received_bytes -= size;
}

void Ledger::close(Identity identity)
{
    if (state(identity) != State::Live)
        return;
    auto const& route = m_routes.at(identity.id);
    m_sent_bytes -= route.sent_bytes;
    m_received_bytes -= route.received_bytes;
    m_routes.erase(identity.id);
}

void Ledger::retire_all()
{
    m_routes.clear();
    m_sent_bytes = 0;
    m_received_bytes = 0;
    // Preserve issued identities while the exit callback's fd3 is still draining.
}

void Ledger::clear()
{
    retire_all();
    m_last = 0;
}
}
