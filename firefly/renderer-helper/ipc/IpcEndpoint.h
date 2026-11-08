/** Document-owned invoke/send capability with shared request/event budgets and explicit teardown.
 */
#pragma once
#include "worlds/IsolatedWorld.h"
#include <AK/HashMap.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Vector.h>
#include <LibJS/Runtime/PromiseCapability.h>
#include <LibWebCommon/HTML/CrossProcessId.h>

namespace Web::DOM {
class Document;
}

namespace WebContent {
class PageClient;
}

namespace Firefly {
class IpcEndpoint final : public RefCounted<IpcEndpoint>, public Weakable<IpcEndpoint> {
public:
    static NonnullRefPtr<IpcEndpoint> create(IsolatedWorld&, Web::DOM::Document&, String);

    ~IpcEndpoint();

    GC::Ref<JS::Object> exports();

    void revoke();

    void expire();

    void receive(
        WebContent::PageClient&, Web::HTML::CrossProcessId, JsonValue const&, bool event = false);

private:
    IpcEndpoint(IsolatedWorld&, Web::DOM::Document&, String);

    JS::ThrowCompletionOr<JS::Value> invoke(JS::VM&, bool event = false);

    void settle(u32, bool, String const&);

    enum class ListenerOperation { On, Once, Remove, RemoveAll };

    JS::ThrowCompletionOr<JS::Value> listen(JS::VM&, ListenerOperation);

    void receive_event(u32, String);

    void control(String const& kind, u32 request);

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
    };

    RefPtr<IsolatedWorld> m_owner;
    GC::Root<Web::DOM::Document> m_document;
    String m_realm;
    HashMap<u32, Pending> m_pending;
    u32 m_next { 0 };
};
}
