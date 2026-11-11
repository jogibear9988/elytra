/** Genuine document-owned DOM endpoints backed by the private native port ledger. */
#pragma once
#include "native/utility/ports/Ledger.h"
#include "ports/Observer.h"
#include <AK/ByteString.h>
#include <AK/HashMap.h>
#include <AK/Vector.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Completion.h>
#include <memory>

namespace Web::DOM {
class Event;
class EventTarget;
}

namespace Web::HTML {
class MessagePort;
}

namespace Firefly {
class IpcEndpoint;

class PortEndpoint {
public:
    AK_ALLOC_WITH_KMALLOC;
    explicit PortEndpoint(IpcEndpoint&);
    ~PortEndpoint();
    bool receive(u32, ReadonlyBytes);
    JS::ThrowCompletionOr<JS::Value> materialize(
        JS::Realm&, Vector<Utility::Ports::Identity> const&);
    void visit_edges(GC::Cell::Visitor&) const;
    void expire();
    void writable();
    void acknowledge_transfer(u32);
    void revoke();
    void dispatch(Utility::Ports::Identity, Web::DOM::Event&);
    static void dispatch_to(IpcEndpoint&, Utility::Ports::Identity, Web::DOM::Event&);

private:
    struct Route {
        Utility::Ports::Identity identity;
        GC::Root<Web::HTML::MessagePort> forwarder;
        GC::Root<Web::DOM::EventTarget> target;
        std::shared_ptr<PortObservation::State> observation;
        std::shared_ptr<PortObservation::State> closing_observation;
        std::deque<size_t> incoming_sizes;
        Vector<ByteString> incoming;
        Vector<ByteString> outgoing;
        size_t outgoing_bytes { 0 };
        u64 dequeued { 0 };
        bool peer_closed { false };
        bool close_requested { false };
    };

    static bool validate(void*, Web::HTML::MessagePort&, JS::Realm&);
    bool live() const;
    bool send(Utility::Ports::Kind, Utility::Ports::Identity, ReadonlyBytes = {});
    void flush();
    void deliver(Route&);
    void close(Utility::Ports::Identity, bool notify);
    IpcEndpoint& m_endpoint;
    Utility::Ports::Ledger m_ledger;
    HashMap<u32, Route> m_routes;
    Vector<ByteString> m_controls;
    Vector<u32> m_transfer_acks;
    size_t m_outgoing_bytes { 0 };
    size_t m_control_bytes { 0 };
    u32 m_incoming_last { 0 };
    u32 m_next { 0 };
    bool m_stopped { false };
    bool m_flushing { false };
};
}
