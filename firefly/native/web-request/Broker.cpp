/** All admission and decisions run on the native UI owner; stale ownership fails closed. */
#include "Broker.h"
#include "Headers.h"
#include <AK/Random.h>
#include <AK/ScopeGuard.h>
#include <LibWebView/WebContentPage.h>

namespace Firefly::WebRequest {
Broker::Broker(Authenticate authenticate, Send send)
    : m_authenticate(move(authenticate))
    , m_send(move(send))
{
}

Broker::~Broker()
{
    stop();
}

bool Broker::send(u64 client, Packet const& packet)
{
    auto found = m_clients.find(client);
    if (found == m_clients.end() || !found->value.channel)
        return false;
    auto encoded = encode_packet(packet);
    return !encoded.is_error() && !found->value.channel->send(encoded.value()).is_error();
}

bool Broker::configure(u32 epoch, bool before, bool headers)
{
    if (m_stopped || !epoch || epoch > 2147483647 || epoch <= m_epoch)
        return false;
    // Revoke outstanding work before publishing the new generation. No response
    // accepted under the old callbacks can authorize work after replacement.
    Vector<u32> tokens;
    for (auto const& entry : m_pending)
        tokens.append(entry.key);
    for (auto token : tokens)
        deny(token);
    m_epoch = epoch;
    m_before = before;
    m_headers = headers;
    set_policy({ epoch, before, headers });
    Vector<u64> failed;
    for (auto& entry : m_clients) {
        entry.value.logical_ids.clear();
        if (!entry.value.ready)
            continue;
        JsonObject policy;
        policy.set("before"sv, before);
        policy.set("headers"sv, headers);
        if (!send(entry.key, Packet { Kind::Policy, 0, epoch, 0, 0, 0, 0, 0, 0, move(policy) }))
            failed.append(entry.key);
    }

    for (auto id : failed)
        retire(id);
    return true;
}

ErrorOr<void> Broker::bind(WebView::WebContentPage& page)
{
    if (m_stopped || !page.is_open())
        return Error::from_string_literal("Web request channel owner unavailable");
    for (auto const& entry : m_clients)
        if (entry.value.owner.ptr() == &page.client())
            return {};
    if (m_clients.size() >= 64 || m_next_client == NumericLimits<u64>::max())
        return Error::from_string_literal("Web request channel quota exceeded");
    auto pair = TRY(IPC::Transport::create_paired());
    auto id = ++m_next_client;
    u64 high = get_random<u64>(), low = get_random<u64>();
    if (!(high || low))
        high = 1;
    auto weak = make_weak_ptr();
    auto channel = TRY(Bytes::Channel::create(move(pair.local),
        { .on_data =
                [weak, id](ReadonlyBytes bytes) {
                    if (auto self = weak.strong_ref())
                        self->receive(id, bytes);
                },
            .on_writable = {},
            .on_closed =
                [weak, id](auto) {
                    if (auto self = weak.strong_ref())
                        self->retire(id);
                } }));
    m_clients.set(id,
        { page.client(), move(channel), high, low, false, 0, {},
            MonotonicTime::now() + AK::Duration::from_seconds(10) });
    page.client().async_firefly_connect_byte_channel(move(pair.remote_handle), high, low);
    return {};
}

void Broker::receive(u64 client, ReadonlyBytes bytes)
{
    auto found = m_clients.find(client);
    if (m_stopped || found == m_clients.end())
        return;
    auto decoded = decode_packet(bytes);
    if (decoded.is_error()) {
        retire(client);
        return;
    }

    auto packet = decoded.release_value();
    if (!found->value.ready) {
        auto high = packet.payload.get_string("cookieHigh"sv);
        auto low = packet.payload.get_string("cookieLow"sv);
        auto pid = packet.payload.get_u64("pid"sv);
        if (packet.kind != Kind::Hello || !high.has_value() || !low.has_value()
            || *high != String::number(found->value.cookie_high)
            || *low != String::number(found->value.cookie_low) || !pid.has_value()
            || *pid != static_cast<u64>(found->value.owner->pid())
            || MonotonicTime::now() >= found->value.deadline) {
            retire(client);
            return;
        }

        found->value.ready = true;
        if (m_epoch) {
            JsonObject policy;
            policy.set("before"sv, m_before);
            policy.set("headers"sv, m_headers);
            if (!send(client, { Kind::Policy, 0, m_epoch, 0, 0, 0, 0, 0, 0, move(policy) }))
                retire(client);
        }

        return;
    }

    if (packet.kind == Kind::Finish) {
        if (packet.epoch != m_epoch)
            return;
        found->value.logical_ids.remove(packet.request);
        return;
    }

    if (packet.kind == Kind::Cancel) {
        Vector<u32> cancelled;
        for (auto const& entry : m_pending)
            if (entry.value.client == client && entry.value.packet.request == packet.request
                && entry.value.packet.epoch == packet.epoch
                && entry.value.packet.page == packet.page
                && entry.value.packet.navigable_namespace == packet.navigable_namespace
                && entry.value.packet.navigable_local == packet.navigable_local
                && entry.value.packet.document_namespace == packet.document_namespace
                && entry.value.packet.document_local == packet.document_local)
                cancelled.append(entry.key);
        for (auto token : cancelled) {
            m_bytes -= m_pending.get(token)->bytes;
            m_pending.remove(token);
        }

        return;
    }

    if ((packet.kind != Kind::Before && packet.kind != Kind::Headers)
        || packet.request <= found->value.last_request) {
        retire(client);
        return;
    }

    found->value.last_request = packet.request;
    auto metadata = m_authenticate(*found->value.owner, packet);
    size_t count = 0;
    for (auto const& entry : m_pending)
        count += entry.value.client == client;
    bool enabled = packet.kind == Kind::Before ? m_before : m_headers;
    if (!metadata.has_value() || packet.epoch != m_epoch || !enabled || m_pending.size() >= 64
        || count >= 16 || bytes.size() > 2 * 1024 * 1024 - m_bytes || m_next_token >= 2147483647) {
        packet.flags = packet.kind == Kind::Headers ? 1 : 0;
        packet.kind = Kind::Decision;
        packet.payload = {};
        packet.payload.set("cancel"sv, true);
        if (!send(client, packet))
            retire(client);
        return;
    }

    auto logical = packet.payload.get_u32("logicalId"sv);
    if (!logical.has_value() || !*logical || *logical > 2147483647) {
        retire(client);
        return;
    }

    auto public_id = found->value.logical_ids.get(*logical);
    if (!public_id.has_value()) {
        if (found->value.logical_ids.size() >= 1024 || m_next_public_id > 2147483645) {
            retire(client);
            return;
        }

        public_id = m_next_public_id;
        m_next_public_id += 2;
        found->value.logical_ids.set(*logical, *public_id);
    }

    metadata->set("id"sv, *public_id);
    auto token = ++m_next_token;
    metadata->set("token"sv, token);
    metadata->set("epoch"sv, m_epoch);
    metadata->set("stage"sv, packet.kind == Kind::Before ? "before"sv : "headers"sv);
    auto contents = metadata->get_i32("contentsId"sv);
    if (!contents.has_value() || *contents <= 0) {
        retire(client);
        return;
    }

    m_bytes += bytes.size();
    m_pending.set(token,
        { client, move(packet), MonotonicTime::now() + AK::Duration::from_seconds(30),
            bytes.size() });
    m_send(*contents, metadata->serialized().to_byte_string());
}

bool Broker::current(u32 token, u32 epoch)
{
    auto entry = m_pending.find(token);
    return !m_stopped && epoch == m_epoch && entry != m_pending.end()
        && entry->value.packet.epoch == epoch && MonotonicTime::now() < entry->value.deadline
        && m_clients.contains(entry->value.client)
        && m_authenticate(*m_clients.get(entry->value.client)->owner, entry->value.packet)
               .has_value();
}

bool Broker::complete(u32 token, u32 epoch, JsonObject decision)
{
    auto found = m_pending.find(token);
    if (found == m_pending.end() || found->value.packet.epoch != epoch)
        return false;
    bool request_headers = found->value.packet.kind == Kind::Before;
    auto original = found->value.packet.payload.get_object(
        request_headers ? "requestHeaders"sv : "responseHeaders"sv);
    if (validate_decision(decision, original.has_value() ? &*original : nullptr, request_headers)
            .is_error())
        return false;
    bool valid = current(token, epoch);
    auto pending = m_pending.take(token).release_value();
    m_bytes -= pending.bytes;
    auto was_headers = pending.packet.kind == Kind::Headers;
    pending.packet.kind = Kind::Decision;
    pending.packet.flags = was_headers ? 1 : 0;
    if (!valid) {
        decision = {};
        decision.set("cancel"sv, true);
    }

    pending.packet.payload = move(decision);
    if (!send(pending.client, pending.packet))
        retire(pending.client);
    return valid;
}

void Broker::deny(u32 token)
{
    auto found = m_pending.find(token);
    if (found == m_pending.end())
        return;
    JsonObject decision;
    decision.set("cancel"sv, true);
    (void)complete(token, found->value.packet.epoch, move(decision));
}

void Broker::retire(u64 client)
{
    auto retired = m_clients.take(client);
    if (!retired.has_value())
        return;
    Vector<u32> tokens;
    for (auto const& entry : m_pending)
        if (entry.value.client == client)
            tokens.append(entry.key);
    for (auto token : tokens) {
        m_bytes -= m_pending.get(token)->bytes;
        m_pending.remove(token);
    }

    retired->channel->close();
}

void Broker::sweep()
{
    Vector<u32> expired;
    for (auto const& entry : m_pending)
        if (!current(entry.key, entry.value.packet.epoch))
            expired.append(entry.key);
    for (auto token : expired)
        deny(token);
    Vector<u64> expired_clients;
    for (auto const& entry : m_clients)
        if (!entry.value.ready && MonotonicTime::now() >= entry.value.deadline)
            expired_clients.append(entry.key);
    for (auto id : expired_clients)
        retire(id);
}

void Broker::stop()
{
    if (m_stopped)
        return;
    m_stopped = true;
    set_policy({});
    revoke_weak_ptrs();
    m_pending.clear();
    m_bytes = 0;
    auto clients = move(m_clients);
    m_clients.clear();
    for (auto& entry : clients)
        entry.value.channel->close();
    m_authenticate = {};
    m_send = {};
}
}
