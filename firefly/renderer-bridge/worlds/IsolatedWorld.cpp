/**
 * Owns one isolated LibJS realm for a document. Evaluation/job scopes retain the owner while
 * detach revokes callbacks and releases realm roots.
 */

#include "worlds/IsolatedWorld.h"
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/HTML/BrowsingContext.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/HTML/WindowProxy.h>

namespace Firefly {
ErrorOr<NonnullRefPtr<IsolatedWorld>> IsolatedWorld::create(Web::HTML::Window& window)
{
    auto browsing_context = window.browsing_context();
    if (!browsing_context)
        return Error::from_string_literal("Isolated world requires a document browsing context");
    auto& vm = window.principal_realm().vm();
    GC::Root world { vm.heap().allocate<Web::Bindings::WrapperWorld>(
        Web::Bindings::WrapperWorld::Type::Extension) };
    auto mode = Web::Bindings::principal_host_defined_environment_settings_object(
        window.principal_realm())
                    .agent_cluster_cross_origin_isolation_mode();
    auto context = Web::Bindings::create_a_new_javascript_realm(
        vm, mode,
        [&](JS::Realm& realm) -> GC::Ref<JS::Object> {
            return Web::Bindings::create_global_object_wrapper(realm, GC::Ref { window });
        },
        [&](JS::Realm& realm) -> GC::Ref<JS::Object> {
            return browsing_context->window_proxy_for(*world, realm)->object();
        });
    auto owner = adopt_ref(*new IsolatedWorld(vm, move(context), *world));
    auto& realm = *owner->m_realm;
    auto intrinsics = realm.create<Web::Bindings::Intrinsics>(realm);
    realm.set_host_defined(realm.heap().allocate<Web::Bindings::HostDefined>(
        intrinsics, *world, window.principal_realm()));
    Web::Bindings::cache_global_object_wrapper(realm);
    auto* active_context = owner->m_context.ptr();
    vm.push_execution_context(*active_context);
    ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == active_context); });
    auto initialized = Web::Bindings::initialize_window_web_interfaces(window, realm);
    if (initialized.is_error())
        return Error::from_string_literal("Cannot initialize isolated window interfaces");
    return owner;
}

IsolatedWorld::IsolatedWorld(JS::VM& vm, NonnullOwnPtr<JS::ExecutionContext> context,
    GC::Ref<Web::Bindings::WrapperWorld> world)
    : m_vm(vm)
    , m_realm(*context->realm)
    , m_world(world)
    , m_context(move(context))
{
}

IsolatedWorld::~IsolatedWorld()
{
    VERIFY(!m_evaluating);
    detach();
}

JS::ThrowCompletionOr<JS::Value> IsolatedWorld::execute(
    Function<JS::ThrowCompletionOr<JS::Value>()> operation)
{
    // The renderer owner thread must have an active caller context, including
    // when asking a detached owner to reject an evaluation.
    if (is_detached())
        return m_vm.throw_completion<JS::Error>("Isolated world has been detached"_utf16);
    if (m_evaluating)
        return m_vm.throw_completion<JS::Error>(
            "Recursive isolated-world evaluation is not supported"_utf16);
    NonnullRefPtr protect { *this };
    m_evaluating = true;
    m_vm.push_execution_context(*m_context);
    ScopeGuard pop([&] {
        VERIFY(m_vm.pop_execution_context() == m_context.ptr());
        m_evaluating = false;
        if (m_detach_requested)
            detach();
    });
    return operation();
}

JS::ThrowCompletionOr<JS::Value> IsolatedWorld::evaluate(Utf16View source)
{
    return execute([&]() -> JS::ThrowCompletionOr<JS::Value> {
        auto parsed = JS::Script::parse(
            Utf16String::from_utf16(source), *m_realm, "firefly:isolated-world"sv);
        if (parsed.is_error())
            return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
        GC::Root script { parsed.release_value() };
        return m_vm.run(*script);
    });
}

JS::ThrowCompletionOr<JS::Value> IsolatedWorld::call(
    JS::FunctionObject& function, ReadonlySpan<JS::Value> arguments)
{
    if (is_detached() || function.realm() != realm())
        return m_vm.throw_completion<JS::Error>(
            "Function does not belong to a live isolated world"sv);
    GC::Root protect_function { function };
    return execute([&] { return JS::call(m_vm, function, JS::js_undefined(), arguments); });
}

JS::ThrowCompletionOr<JS::Value> IsolatedWorld::call_from_callback(
    JS::FunctionObject& function, ReadonlySpan<JS::Value> arguments)
{
    if (!m_evaluating || is_detached() || function.realm() != realm() || m_callback_depth >= 32)
        return m_vm.throw_completion<JS::Error>("Invalid nested callback entry"_utf16);
    NonnullRefPtr protect { *this };
    GC::Root protect_function { function };
    auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
    context->realm = realm();
    ++m_callback_depth;
    m_vm.push_execution_context(*context);
    ScopeGuard pop([&] {
        VERIFY(m_vm.pop_execution_context() == context.ptr());
        --m_callback_depth;
    });
    return JS::call(m_vm, function, JS::js_undefined(), arguments);
}

JS::ThrowCompletionOr<JS::Value> IsolatedWorld::run_async_frame(
    Function<JS::ThrowCompletionOr<JS::Value>()> operation)
{
    return execute(move(operation));
}

void IsolatedWorld::detach()
{
    if (!m_world || m_detaching)
        return;
    if (m_evaluating) {
        m_detach_requested = true;
        return;
    }
    // Cleanup hooks may reenter detach or release the last external reference.
    // Destruction itself has refcount zero, in which case try_ref must not revive.
    RefPtr<IsolatedWorld> protect;
    if (try_ref())
        protect = adopt_ref(*this);
    m_detaching = true;
    m_detach_requested = true;
    ScopeGuard finish([&] { m_detaching = false; });
    auto hooks = move(m_detach_hooks);
    for (auto& hook : hooks)
        hook();
    m_world->detach();
    m_nonexportable.clear();
    m_context.clear();
    m_realm = nullptr;
    m_world = nullptr;
}
}
