/** Document-wide weak callback identity cache. Proxies own live callbacks; the cache never does. */
#pragma once
#include "worlds/IsolatedWorld.h"
#include <AK/HashMap.h>
#include <AK/WeakPtr.h>
#include <AK/Weakable.h>
#include <LibGC/Weak.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PromiseCapability.h>

namespace Web::DOM {
class Document;
}

namespace Firefly {
class CallbackEndpoint;

struct CallbackExport {
    GC::Ref<JS::NativeFunction> function;
    RefPtr<CallbackEndpoint> created;
};

class CallbackRegistry final : public RefCounted<CallbackRegistry>,
                               public Weakable<CallbackRegistry> {
public:
    static ErrorOr<NonnullRefPtr<CallbackRegistry>> create(IsolatedWorld&, Web::DOM::Document&);

    ErrorOr<CallbackExport> proxy(JS::FunctionObject&);

    void discard(CallbackEndpoint&);

    void revoke();

    bool active() const;

    size_t size();

    size_t proxy_count();

    RefPtr<IsolatedWorld> world;
    GC::Root<Web::DOM::Document> document;
    size_t principal_depth { 0 };
    size_t pending { 0 };

private:
    struct Entry {
        GC::Weak<JS::FunctionObject> original;
        GC::Weak<JS::NativeFunction> proxy;
        WeakPtr<CallbackEndpoint> owner;
    };

    CallbackRegistry(IsolatedWorld&, Web::DOM::Document&);

    void sweep();

    Vector<Entry> m_entries;
    bool m_revoked { false };
};

class CallbackEndpoint final : public RefCounted<CallbackEndpoint>,
                               public Weakable<CallbackEndpoint> {
public:
    void visit_edges(GC::Cell::Visitor&) const;

    ~CallbackEndpoint();

    static ErrorOr<CallbackExport> create(CallbackRegistry&, JS::FunctionObject&);

    void revoke();

    bool has_pending() const
    {
        return !m_pending.is_empty();
    }

private:
    CallbackEndpoint(CallbackRegistry&, JS::FunctionObject&);

    JS::ThrowCompletionOr<JS::Value> invoke(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> observe(JS::Promise&, JS::Realm&);

    JS::ThrowCompletionOr<JS::Value> settle(u64, bool, JS::Value);

    struct Pending {
        GC::Root<JS::PromiseCapability> capability;
        GC::Root<JS::Realm> target;
    };

    RefPtr<CallbackRegistry> m_registry;
    GC::Root<JS::FunctionObject> m_function;
    HashMap<u64, Pending> m_pending;
    u64 m_next { 0 };
};
}
