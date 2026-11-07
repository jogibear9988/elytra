/** Use the pinned native transport; application payload never enters a text or JS value. */
#include "Channel.h"
#include <AK/NumericLimits.h>
#include <AK/ScopeGuard.h>

namespace Firefly::Bytes {
Channel::Channel(NonnullOwnPtr<IPC::Transport> transport, Handlers handlers)
    : m_transport(move(transport))
    , m_callbacks(adopt_ref(*new CallbackOwner(move(handlers))))
{
}

ErrorOr<NonnullRefPtr<Channel>> Channel::create(
    NonnullOwnPtr<IPC::Transport> transport, Handlers handlers)
{
    if (!transport->is_open() || !handlers.on_data)
        return Error::from_string_literal(
            "Byte channel requires an open owned transport and receiver");
    auto channel = adopt_ref(*new Channel(move(transport), move(handlers)));
#ifdef AK_OS_WINDOWS
    // A stalled peer must not block this owner thread in a native pipe write.
    channel->m_transport->set_disconnects_when_peer_stops_reading(true);
#endif
    channel->m_transport->set_up_read_hook([weak = channel->make_weak_ptr()] {
        if (auto owner = weak.strong_ref())
            static_cast<Channel&>(*owner).read();
    });
    return channel;
}

Channel::~Channel()
{
    finish(CloseReason::Local, false);
}

bool Channel::is_open() const
{
    return m_open && m_transport->is_open();
}

ErrorOr<void> Channel::send(ReadonlyBytes bytes)
{
    VERIFY(m_owner.is_current_thread());
    if (!is_open())
        return Error::from_string_literal("Byte channel is closed");
    if (bytes.size() > max_payload || m_sent.size() >= max_pending
        || bytes.size() > max_queued_bytes - m_sent_bytes)
        return Error::from_string_literal("Byte channel send credit/payload exhausted");
    if (m_last_sent == NumericLimits<u64>::max())
        return Error::from_string_literal("Byte channel sequence exhausted");
    auto sequence = m_last_sent + 1;
    auto packet = TRY(encode_packet(PacketKind::Data, sequence, bytes));
    if (auto result = packet.transfer_message(*m_transport); result.is_error()) {
        finish(CloseReason::Transport);
        return result.release_error();
    }

    m_sent.set(sequence, bytes.size());
    m_sent_bytes += bytes.size();
    m_last_sent = sequence;
    return {};
}

void Channel::read()
{
    VERIFY(m_owner.is_current_thread());
    if (!m_open)
        return;
    bool rejected = false;
    bool writable = false;
    auto status = m_transport->read_as_many_messages_as_possible_without_blocking([&](auto&& raw) {
        if (rejected)
            return;
        auto decoded = decode_packet(raw.bytes.bytes());
        if (!raw.attachments.is_empty() || decoded.is_error()) {
            rejected = true;
            return;
        }

        auto packet = decoded.release_value();
        if (packet.kind == PacketKind::Ack) {
            auto found = m_sent.find(packet.sequence);
            if (found == m_sent.end()) {
                rejected = true;
                return;
            }

            m_sent_bytes -= found->value;
            m_sent.remove(found);
            writable = true;
            return;
        }

        if (m_last_received == NumericLimits<u64>::max() || packet.sequence != m_last_received + 1
            || m_received_count >= max_pending
            || packet.payload.size() > max_queued_bytes - m_queued_bytes) {
            rejected = true;
            return;
        }

        m_last_received = packet.sequence;
        ++m_received_count;
        m_queued_bytes += packet.payload.size();
        // Keep the transport's native bytes/mapping alive; no extra payload copy.
        m_incoming.append({ move(raw.bytes), packet.sequence });
    });
    // Peer loss/invalid input cancels even queued application delivery.
    if (rejected || status == IPC::Transport::ShouldShutdown::Yes) {
        finish(rejected ? CloseReason::Protocol : CloseReason::Peer);
        return;
    }

    if (writable)
        notify_writable();
    schedule_dispatch();
}

void Channel::schedule_dispatch()
{
    if (m_open && !m_incoming.is_empty() && !m_dispatch_scheduled && !m_dispatching) {
        m_dispatch_scheduled = true;
        deferred_invoke([this] { dispatch(); });
    }
}

void Channel::dispatch()
{
    VERIFY(m_owner.is_current_thread());
    m_dispatch_scheduled = false;
    if (!m_open || m_dispatching)
        return;
    m_dispatching = true;
    ScopeGuard finish_dispatch([&] {
        m_dispatching = false;
        schedule_dispatch();
    });
    auto messages = move(m_incoming);
    for (auto& message : messages) {
        if (!m_open)
            return;
        auto payload = message.bytes.bytes().slice(header_size);
        auto callbacks = m_callbacks; // A callback may close/release its owner.
        callbacks->handlers.on_data(payload);
        if (!m_open)
            return;
        m_queued_bytes -= payload.size();
        --m_received_count;
        auto ack = MUST(encode_packet(PacketKind::Ack, message.sequence, {}));
        if (ack.transfer_message(*m_transport).is_error()) {
            finish(CloseReason::Transport);
            return;
        }
    }
}

void Channel::notify_writable()
{
    if (m_writable_scheduled)
        return;
    m_writable_scheduled = true;
    deferred_invoke([this] {
        m_writable_scheduled = false;
        if (!m_open)
            return;
        auto callbacks = m_callbacks;
        if (callbacks->handlers.on_writable)
            callbacks->handlers.on_writable();
    });
}

void Channel::close()
{
    finish(CloseReason::Local);
}

void Channel::finish(CloseReason reason, bool notify)
{
    VERIFY(m_owner.is_current_thread());
    if (!m_open)
        return;
    m_open = false;
    m_transport->close();
    m_incoming.clear();
    m_queued_bytes = m_sent_bytes = 0;
    m_received_count = 0;
    m_sent.clear();
    auto callbacks = move(m_callbacks);
    if (notify && callbacks && callbacks->handlers.on_closed)
        deferred_invoke(
            [callbacks = move(callbacks), reason] { callbacks->handlers.on_closed(reason); });
}
}
