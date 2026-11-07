/** Owned native byte transport with bounded application queues and acknowledgement credit. */
#pragma once
#include "Packet.h"
#include <AK/HashMap.h>
#include <AK/RefPtr.h>
#include <AK/ThreadID.h>
#include <AK/Vector.h>
#include <LibCore/EventReceiver.h>
#include <LibIPC/ReceivedMessageBytes.h>
#include <LibIPC/Transport.h>

namespace Firefly::Bytes {
class Channel final : public Core::EventReceiver {
    C_OBJECT_ABSTRACT(Channel);

public:
    enum class CloseReason { Local, Peer, Protocol, Transport };

    struct Handlers {
        // Borrowed only during this callback. Copy bytes if retaining them.
        Function<void(ReadonlyBytes)> on_data;
        Function<void()> on_writable;
        Function<void(CloseReason)> on_closed;
    };

    static ErrorOr<NonnullRefPtr<Channel>> create(NonnullOwnPtr<IPC::Transport>, Handlers);

    ~Channel();

    ErrorOr<void> send(ReadonlyBytes);

    void close();

    bool is_open() const;

    size_t pending_send_count() const
    {
        return m_sent.size();
    }

    size_t queued_receive_bytes() const
    {
        return m_queued_bytes;
    }

private:
    struct CallbackOwner : RefCounted<CallbackOwner> {
        explicit CallbackOwner(Handlers value)
            : handlers(move(value))
        {
        }

        Handlers handlers;
    };

    struct Incoming {
        IPC::ReceivedMessageBytes bytes;
        u64 sequence;
    };

    Channel(NonnullOwnPtr<IPC::Transport>, Handlers);

    void read();

    void dispatch();

    void schedule_dispatch();

    void notify_writable();

    void finish(CloseReason, bool notify = true);

    NonnullOwnPtr<IPC::Transport> m_transport;
    AK::ThreadID m_owner { AK::ThreadID::current() };
    RefPtr<CallbackOwner> m_callbacks;
    Vector<Incoming> m_incoming;
    HashMap<u64, size_t> m_sent;
    size_t m_queued_bytes { 0 };
    size_t m_received_count { 0 };
    size_t m_sent_bytes { 0 };
    u64 m_last_sent { 0 };
    u64 m_last_received { 0 };
    bool m_open { true };
    bool m_dispatch_scheduled { false };
    bool m_dispatching { false };
    bool m_writable_scheduled { false };
};
}
