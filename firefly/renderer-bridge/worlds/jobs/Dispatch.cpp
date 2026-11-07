/** Associate jobs, WebIDL callbacks and Promise failures with their source world. */
#include "worlds/jobs/State.h"
#include <AK/ScopeGuard.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/JobCallback.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/MutationObserver.h>
#include <LibWeb/DOM/MutationRecord.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/WebIDL/CallbackType.h>

namespace Firefly {
JS::Completion WorldJobGuard::State::invoke_webidl(Web::WebIDL::CallbackType& callback,
    Optional<JS::Value> receiver, Web::WebIDL::ExceptionBehavior behavior,
    ReadonlySpan<JS::Value> arguments)
{
    auto delegate = [&] {
        if (previous_gate.invoke)
            return previous_gate.invoke(
                previous_gate.context, callback, receiver, behavior, arguments);
        return Callbacks::invoke_original(callback, receiver, behavior, arguments);
    };
    // Promise-returning and rethrowing operations need their own conversion
    // policy. This gate protects report-style function callbacks first.
    if (behavior != Web::WebIDL::ExceptionBehavior::Report
        || callback.operation_returns_promise != Web::WebIDL::OperationReturnsPromise::No)
        return delegate();
    auto& associated = callback.callback->shape().realm();
    GC::Ptr<JS::Realm> actual;
    if (callback.callback->is_function()) {
        auto function_realm
            = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
        if (!function_realm.is_error())
            actual = function_realm.value();
    }

    bool own_associated = realms.contains(associated);
    bool own_actual = actual && realms.contains(*actual);
    if (!own_associated && !own_actual)
        return in_principal(associated, [&]() -> JS::Completion { return delegate(); });
    if (own_associated && blocked_realm(associated))
        return JS::js_undefined();
    if (own_actual && actual.ptr() != &associated && blocked_realm(*actual))
        return JS::js_undefined();
    auto result = run_owned(own_associated ? owner_for(associated) : nullptr,
        own_actual ? owner_for(*actual) : nullptr, [&]() -> JS::ThrowCompletionOr<JS::Value> {
            // Capture the abrupt completion before the upstream Report path
            // can put its raw source object into a principal ErrorEvent.
            GC::RootVector<JS::Value> safe_arguments;
            for (auto value : arguments)
                safe_arguments.append(value);
            auto safe_receiver = receiver;
            if (receiver.has_value() && receiver->is_object()) {
                auto receiver_value = *receiver;
                auto* observer = Web::Bindings::impl_from<Web::DOM::MutationObserver>(
                    &receiver_value.as_object());
                if (observer && &observer->callback() == &callback) {
                    if (arguments.size() != 2 || !arguments[0].is_object())
                        return vm.throw_completion<JS::TypeError>(
                            "Invalid mutation observer delivery"sv);
                    auto source_array_value = arguments[0];
                    auto& source_array = source_array_value.as_object();
                    if (source_array.engine_class_id() != JS_LAYOUT_CLASS_ID_ARRAY
                        || JS::host_class_of(source_array))
                        return vm.throw_completion<JS::TypeError>("Invalid mutation record list"sv);
                    auto length = TRY(source_array.internal_get_own_property("length"_utf16));
                    if (!length.has_value() || !length->value.has_value()
                        || !length->value->is_number())
                        return vm.throw_completion<JS::TypeError>(
                            "Missing mutation record length"sv);
                    auto size = static_cast<u32>(length->value->as_double());
                    GC::Root array { TRY(JS::Array::create(associated, size)) };
                    auto& world = Web::Bindings::host_defined_wrapper_world(associated);
                    for (u32 i = 0; i < size; ++i) {
                        auto descriptor = TRY(source_array.ordinary_get_own_property(i));
                        if (!descriptor.has_value() || !descriptor->is_data_descriptor()
                            || !descriptor->value.has_value() || !descriptor->value->is_object())
                            return vm.throw_completion<JS::TypeError>("Invalid mutation record"sv);
                        auto value = *descriptor->value;
                        auto* record = Web::Bindings::impl_from<Web::DOM::MutationRecord>(
                            &value.as_object());
                        if (!record)
                            return vm.throw_completion<JS::TypeError>("Unknown mutation record"sv);
                        TRY(array->create_data_property_or_throw(
                            i, Web::Bindings::wrap(world, associated, GC::Ref { *record })));
                    }

                    GC::Root wrapped_observer { Web::Bindings::wrap(
                        world, associated, GC::Ref { *observer }) };
                    safe_arguments[0] = array.ptr();
                    safe_arguments[1] = wrapped_observer.ptr();
                    safe_receiver = wrapped_observer.ptr();
                }
            }

            auto completion = Callbacks::invoke_original(callback, safe_receiver,
                Web::WebIDL::ExceptionBehavior::Rethrow, safe_arguments.span());
            if (completion.type() == JS::Completion::Type::Throw)
                return JS::throw_completion(completion.release_value());
            return completion.release_value();
        });
    if (result.is_error()) {
        ++source_job_errors;
        return JS::js_undefined();
    }

    return result.release_value();
}

void WorldJobGuard::State::rejection(
    JS::Promise& promise, JS::Promise::RejectionOperation operation)
{
    // Native identities only: never read/stringify a source reason or place
    // its Promise in the principal Window's rejection-event lists. A foreign
    // Promise constructor can still be rejected by an owned source frame.
    bool owned = realms.contains(promise.shape().realm());
    if (operation == JS::Promise::RejectionOperation::Reject) {
        if (!owned) {
            vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
                if (context.realm && realms.contains(*context.realm)) {
                    owned = true;
                    return false;
                }

                return true;
            });
        }

        if (owned) {
            redacted_promises.set(promise);
            ++source_rejections;
            return;
        }
    } else if (redacted_promises.remove(promise) || owned) {
        return;
    }

    upstream_rejections(promise, operation);
}

RefPtr<IsolatedWorld> WorldJobGuard::State::owner_for(JS::Realm& realm)
{
    auto owner = realms.get(realm);
    return owner.has_value() ? owner->strong_ref() : nullptr;
}

JS::ThrowCompletionOr<JS::Value> WorldJobGuard::State::run_owned(RefPtr<IsolatedWorld> first,
    RefPtr<IsolatedWorld> second, Function<JS::ThrowCompletionOr<JS::Value>()> operation)
{
    auto inner = [&]() -> JS::ThrowCompletionOr<JS::Value> {
        if (second && second != first && !second->is_executing())
            return second->run_async_frame([&] { return operation(); });
        return operation();
    };

    if (first && !first->is_executing())
        return first->run_async_frame([&] { return inner(); });
    return inner();
}

bool WorldJobGuard::State::blocked_realm(JS::Realm& realm)
{
    auto owner = owner_for(realm);
    auto& world = Web::Bindings::host_defined_wrapper_world(realm);
    auto document = Web::Bindings::principal_host_defined_environment_settings_object(realm)
                        .responsible_document();
    if (!owner || owner->is_detached() || world.is_detached() || !document
        || !document->is_fully_active()) {
        if (owner)
            owner->detach();
        else
            world.detach();
        ++blocked;
        return true;
    }

    return false;
}

void WorldJobGuard::State::enqueue(JS::PromiseJob job, GC::Ptr<JS::Realm> realm)
{
    GC::Ptr<JS::Realm> origin;
    vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
        if (context.realm && realms.contains(*context.realm)) {
            origin = context.realm;
            return false;
        }

        return true;
    });
    bool owned_realm = realm && realms.contains(*realm);
    if (!owned_realm && !origin) {
        upstream_enqueue(move(job), realm);
        return;
    }

    if (origin && blocked_realm(*origin))
        return;
    if (owned_realm && realm != origin && blocked_realm(*realm))
        return;
    NonnullRefPtr self { *this };
    auto script_or_module = vm.get_active_script_or_module();
    // PromiseJob cannot be wrapped through its private constructor. Queue
    // only our owned-world jobs via the public HTML microtask/context APIs;
    // the upstream hook retains all other jobs and the VM's queue contract.
    Web::HTML::queue_a_microtask(nullptr,
        GC::create_function(
            vm.heap(), [self, realm, origin, owned_realm, job = move(job), script_or_module] {
                if (origin && self->blocked_realm(*origin))
                    return;
                if (owned_realm && realm != origin && self->blocked_realm(*realm))
                    return;
                // Acquire ownership before context setup can allocate. Never run
                // an unprotected job if an owner vanished after the first check.
                auto origin_owner = origin ? self->owner_for(*origin) : nullptr;
                auto realm_owner = owned_realm ? self->owner_for(*realm) : nullptr;
                if (origin && !origin_owner) {
                    self->blocked_realm(*origin);
                    return;
                }

                if (owned_realm && !realm_owner) {
                    self->blocked_realm(*realm);
                    return;
                }

                auto execution_realm = realm ? realm : origin;
                Web::HTML::TemporaryExecutionContext context(
                    *execution_realm, Web::HTML::TemporaryExecutionContext::CallbacksEnabled::Yes);
                auto& execution = Web::Bindings::principal_host_defined_environment_settings_object(
                    *execution_realm)
                                      .realm_execution_context();
                auto previous = execution.script_or_module;
                execution.script_or_module = script_or_module;
                ScopeGuard restore([&] { execution.script_or_module = previous; });
                auto result = self->run_owned(
                    move(origin_owner), move(realm_owner), [&] { return job.run(); });
                // The ordinary reporter may dispatch an ErrorEvent with the raw
                // source object at the shared principal Window. Record only the
                // fact of failure; no source reason/stack enters the page.
                if (result.is_error())
                    ++self->source_job_errors;
            }));
}

JS::ThrowCompletionOr<JS::Value> WorldJobGuard::State::invoke(
    JS::JobCallback& callback, JS::Value receiver, ReadonlySpan<JS::Value> arguments)
{
    // This engine operation unwraps bound functions/proxies without property
    // access or traps. Revoked proxies already cannot execute: delegate their
    // normal error path to the original hook.
    auto realm = JS::get_function_realm(vm, callback.callback());
    if (!realm.is_error() && realms.contains(*realm.value())) {
        if (blocked_realm(*realm.value()))
            return JS::js_undefined();
        return run_owned(owner_for(*realm.value()), nullptr,
            [&] { return upstream(callback, receiver, arguments); });
    }

    return upstream(callback, receiver, arguments);
}

}
