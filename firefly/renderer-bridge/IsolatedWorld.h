#pragma once
#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/OwnPtr.h>
#include <AK/RefCounted.h>
#include <AK/Utf16View.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/ExecutionContext.h>
#include <LibWeb/Bindings/WrapperWorld.h>
namespace Web::HTML { class Window; }
namespace Firefly {
// Internal renderer-side owner. This does not install privileges or a public
// preload API; cross-world values must go through a separate validated membrane.
class IsolatedWorld final : public RefCounted<IsolatedWorld> {
public:
    AK_ALLOC_WITH_KMALLOC;
    static ErrorOr<NonnullRefPtr<IsolatedWorld>> create(Web::HTML::Window&);
    ~IsolatedWorld();
    IsolatedWorld(IsolatedWorld const&) = delete;
    IsolatedWorld& operator=(IsolatedWorld const&) = delete;
    JS::ThrowCompletionOr<JS::Value> evaluate(Utf16View);
    JS::ThrowCompletionOr<JS::Value> call(JS::FunctionObject&, ReadonlySpan<JS::Value>);
    // The caller must revoke exported callbacks and stop asynchronous activity
    // before final detach; this owner currently handles synchronous evaluation.
    void detach();
    bool is_detached() const { return !m_world || m_detach_requested || m_world->is_detached(); }
    JS::Realm* realm() const { return is_detached() ? nullptr : m_realm.ptr(); }
private:
    JS::ThrowCompletionOr<JS::Value> execute(Function<JS::ThrowCompletionOr<JS::Value>()>);
    IsolatedWorld(JS::VM&, NonnullOwnPtr<JS::ExecutionContext>, GC::Ref<Web::Bindings::WrapperWorld>);
    JS::VM& m_vm;
    GC::Root<JS::Realm> m_realm;
    GC::Root<Web::Bindings::WrapperWorld> m_world;
    OwnPtr<JS::ExecutionContext> m_context;
    bool m_evaluating { false };
    bool m_detach_requested { false };
};
}
