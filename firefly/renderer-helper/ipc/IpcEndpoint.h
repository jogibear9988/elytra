/** Document-owned invoke/send capability with shared request/event budgets and explicit teardown.
 */
#pragma once
#include "native/ipc/application/Fragment.h"
#include "native/ipc/application/Message.h"
#include "native/ipc/application/WireBudget.h"
#include "native/ipc/bytes/Channel.h"
#include "native/utility/ports/Protocol.h"
#include "worlds/IsolatedWorld.h"
#include <AK/HashMap.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibJS/Runtime/PromiseCapability.h>
#include <LibWebCommon/HTML/CrossProcessId.h>

namespace Web::DOM {
class Document;
}

namespace WebContent {
class PageClient;
}

namespace Firefly {
class PortEndpoint;

class IpcEndpoint final : public RefCounted<IpcEndpoint>, public Weakable<IpcEndpoint> {
public:
    void visit_edges(GC::Cell::Visitor&) const;

    static NonnullRefPtr<IpcEndpoint> create(IsolatedWorld&, Web::DOM::Document&, String);

    ~IpcEndpoint();

    GC::Ref<JS::Object> exports();

    void revoke();

    // Called only from the trusted generated NotRouted connection handoff.
    void attach_channel(WebContent::PageClient&, IPC::TransportHandle, Bytes::IpcBinding);

    void expire();

private:
    friend class PortEndpoint;
    OwnPtr<PortEndpoint> m_ports;
    IpcEndpoint(IsolatedWorld&, Web::DOM::Document&, String);

    JS::ThrowCompletionOr<JS::Value> invoke(JS::VM&, bool event = false);

    void settle(u32, bool, ByteString const&, RefPtr<IpcWireReservation> = {});

    enum class ListenerOperation { On, Once, Remove, RemoveAll };

    JS::ThrowCompletionOr<JS::Value> listen(JS::VM&, ListenerOperation);

    void receive_event(
        u32, ByteString, RefPtr<IpcWireReservation>, Vector<Utility::Ports::Identity> = {});

    void control(String const& kind, u32 request);

    bool send_packet(ApplicationIpc::Kind, u32, bool, ByteString const&);

    void flush_packets();

    void receive_packet(ReadonlyBytes);

    void retire_channel();

    void clear_packets();

    struct Listener {
        Utf16String channel;
        GC::Root<JS::FunctionObject> callback;
        u64 id;
        bool once;
    };

    Vector<Listener> m_listeners;
    GC::Root<JS::Object> m_exports;
    u64 m_listener_next { 0 };
    u32 m_incoming_last { 0 };
    size_t m_incoming_pending { 0 };

    struct Pending {
        GC::Root<JS::PromiseCapability> capability;
        MonotonicTime deadline;
        bool settling { false };
    };

    RefPtr<IsolatedWorld> m_owner;
    GC::Root<Web::DOM::Document> m_document;
    String m_realm;
    HashMap<u32, Pending> m_pending;
    RefPtr<Bytes::Channel> m_byte_channel;
    u64 m_byte_epoch { 0 };
    bool m_byte_failed { false };

    struct QueuedPacket {
        ApplicationIpc::Kind kind;
        u32 request;
        bool success;
        ByteString wire;
        size_t offset { 0 };
        RefPtr<IpcWireReservation> reservation;
    };

    Vector<QueuedPacket> m_byte_queue;
    NonnullRefPtr<IpcWireBudget> m_wire_budget { adopt_ref(*new IpcWireBudget) };
    RefPtr<IpcWireReservation> m_receive_reservation;
    ApplicationIpc::FragmentReassembler m_reassembler;
    Optional<MonotonicTime> m_fragment_deadline;
    u32 m_next { 0 };
};
}
