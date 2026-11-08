/**
 * Publishes bounded immutable application APIs from an isolated preload. Validation and
 * callback registration must succeed atomically before the page sees an API.
 */

#pragma once
#include "context/FunctionBridge.h"
#include <AK/Weakable.h>

namespace Firefly {
// Native renderer publication engine. Public preload selection/IPC transport
// are separate integration steps; no main-process privileges are installed here.
class ContextBridge final : public RefCounted<ContextBridge>, public Weakable<ContextBridge> {
public:
    AK_ALLOC_WITH_KMALLOC;
    static ErrorOr<NonnullRefPtr<ContextBridge>> create(IsolatedWorld&, Web::DOM::Document&);

    ~ContextBridge();

    GC::Ref<JS::Object> exports();

    void revoke();

    size_t callback_count();

    size_t callback_proxy_count();

    size_t function_count() const
    {
        return m_functions.size();
    }

private:
    ContextBridge(IsolatedWorld&, Web::DOM::Document&, CallbackRegistry&);

    JS::ThrowCompletionOr<JS::Value> expose(JS::VM&);

    RefPtr<IsolatedWorld> m_source;
    RefPtr<CallbackRegistry> m_callbacks;
    GC::Root<Web::DOM::Document> m_document;
    Vector<NonnullRefPtr<FunctionBridge>> m_functions;
    size_t m_publications { 0 };
    bool m_revoked { false };
    bool m_publishing { false };
};
}
