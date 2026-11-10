/**
 * Publishes revocable cross-world function endpoints and observes Promise results. Explicit
 * pending roots must be released before their source document/world is torn down.
 */

#pragma once
#include "context/CallbackBridge.h"
#include "worlds/IsolatedWorld.h"
#include <AK/HashMap.h>
#include <LibJS/Runtime/NativeFunction.h>

namespace Web::DOM {
class Document;
}

namespace Firefly {
class FunctionBridge;

struct FunctionExport {
    NonnullRefPtr<FunctionBridge> owner;
    GC::Ref<JS::NativeFunction> function;
};

// One data-only endpoint with observed Promise results for one exact document/isolated world.
// Its native registry MUST revoke it before document/world teardown; a target
// global may otherwise retain the document through this object's explicit roots.
class FunctionBridge final : public RefCounted<FunctionBridge> {
public:
    void visit_edges(GC::Cell::Visitor&) const;

    AK_ALLOC_WITH_KMALLOC;
    static ErrorOr<FunctionExport> create(
        IsolatedWorld&, JS::FunctionObject&, Web::DOM::Document&, RefPtr<CallbackRegistry> = {});

    ~FunctionBridge();

    void revoke();

    size_t pending_count() const
    {
        return m_pending.size();
    }

    bool is_revoked() const
    {
        return m_revoked;
    }

private:
    FunctionBridge(IsolatedWorld&, JS::FunctionObject&, Web::DOM::Document&, CallbackRegistry&,
        bool own_registry);

    JS::ThrowCompletionOr<JS::Value> invoke(JS::VM&);

    struct Pending;
    JS::ThrowCompletionOr<JS::Value> observe(JS::VM&, JS::Promise&, JS::Realm&);

    JS::ThrowCompletionOr<JS::Value> settle(u64, bool fulfilled, JS::Value);

    HashMap<u64, NonnullRefPtr<Pending>> m_pending;
    u64 m_next_id { 0 };
    RefPtr<IsolatedWorld> m_source;
    GC::Root<JS::FunctionObject> m_function;
    GC::Root<Web::DOM::Document> m_document;
    bool m_revoked { false };
    RefPtr<CallbackRegistry> m_callbacks;
    bool m_own_registry { false };
    size_t m_call_depth { 0 };
};
}
