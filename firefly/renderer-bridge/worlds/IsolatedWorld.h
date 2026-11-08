/**
 * Owns one isolated LibJS realm for a document. Evaluation/job scopes retain the owner while
 * detach revokes callbacks and releases realm roots.
 */

#pragma once
#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/OwnPtr.h>
#include <AK/RefCounted.h>
#include <AK/Utf16View.h>
#include <AK/Vector.h>
#include <AK/Weakable.h>
#include <LibGC/Root.h>
#include <LibGC/Weak.h>
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/ExecutionContext.h>
#include <LibWeb/Bindings/WrapperWorld.h>

namespace Web::HTML {
class Window;
}

namespace Firefly {
// Internal renderer-side owner. This does not install privileges or a public
// preload API; cross-world values must go through a separate validated membrane.
class IsolatedWorld final : public RefCounted<IsolatedWorld>, public Weakable<IsolatedWorld> {
public:
    AK_ALLOC_WITH_KMALLOC;
    static ErrorOr<NonnullRefPtr<IsolatedWorld>> create(Web::HTML::Window&);

    ~IsolatedWorld();

    IsolatedWorld(IsolatedWorld const&) = delete;
    IsolatedWorld& operator=(IsolatedWorld const&) = delete;
    JS::ThrowCompletionOr<JS::Value> evaluate(Utf16View);

    JS::ThrowCompletionOr<JS::Value> call(JS::FunctionObject&, ReadonlySpan<JS::Value>);
    // Internal job-dispatch scope. Keep the owner/context alive while an engine
    // job runs; ordinary evaluate/call still reject recursive entry.
    JS::ThrowCompletionOr<JS::Value> run_async_frame(Function<JS::ThrowCompletionOr<JS::Value>()>);

    // Native capabilities must not be published wholesale through contextBridge.
    void block_function_export(JS::FunctionObject&);

    bool function_export_blocked(JS::FunctionObject&) const;

    // Redact only natively registered capability objects when copying to the
    // principal realm. Weak entries neither retain modules nor match reused cells.
    void redact_object_export(JS::Object&);

    bool object_export_redacted(JS::Object&) const;

    bool is_executing() const
    {
        return m_evaluating;
    }

    // The caller must revoke exported callbacks before final detach. Synchronous
    // evaluation and guarded jobs defer physical teardown until their frame exits.
    void detach();

    void add_detach_hook(Function<void()> hook)
    {
        m_detach_hooks.append(move(hook));
    }

    bool is_detached() const
    {
        return !m_world || m_detach_requested || m_world->is_detached();
    }

    JS::Realm* realm() const
    {
        return is_detached() ? nullptr : m_realm.ptr();
    }

private:
    friend class FunctionBridge;
    // Only a validated principal callback scope may request this nested entry.
    JS::ThrowCompletionOr<JS::Value> call_from_callback(
        JS::FunctionObject&, ReadonlySpan<JS::Value>);

    JS::ThrowCompletionOr<JS::Value> execute(Function<JS::ThrowCompletionOr<JS::Value>()>);

    IsolatedWorld(
        JS::VM&, NonnullOwnPtr<JS::ExecutionContext>, GC::Ref<Web::Bindings::WrapperWorld>);

    JS::VM& m_vm;
    GC::Root<JS::Realm> m_realm;
    GC::Root<Web::Bindings::WrapperWorld> m_world;
    OwnPtr<JS::ExecutionContext> m_context;
    Vector<Function<void()>> m_detach_hooks;
    Vector<GC::Root<JS::FunctionObject>> m_nonexportable;
    Vector<GC::Weak<JS::Object>> m_redacted;
    size_t m_callback_depth { 0 };
    bool m_evaluating { false };
    bool m_detach_requested { false };
    bool m_detaching { false };
};
}
