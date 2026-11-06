/** Document-owned IPC capability, with per-endpoint and process request budgets and explicit
 * teardown. */
#pragma once
#include "worlds/IsolatedWorld.h"
#include <AK/HashMap.h>
#include <AK/JsonValue.h>
#include <AK/String.h>
#include <AK/Time.h>
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

    void receive(WebContent::PageClient&, Web::HTML::CrossProcessId, JsonValue const&);

private:
    IpcEndpoint(IsolatedWorld&, Web::DOM::Document&, String);

    JS::ThrowCompletionOr<JS::Value> invoke(JS::VM&);

    void settle(u32, bool, String const&);

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
