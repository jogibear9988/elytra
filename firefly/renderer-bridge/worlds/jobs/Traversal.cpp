/** Guard NodeFilter identity, callback-world entry and revoked/foreign references. */
#include "worlds/jobs/State.h"
#include <AK/ScopeGuard.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/NodeFilter.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/WebIDL/CallbackType.h>

namespace Firefly {
void WorldJobGuard::State::filter_created(Web::DOM::NodeFilter& filter, JS::Realm* writer)
{
    filters.remove_all_matching([](auto const& record) { return !record.filter.ptr(); });
    filters.append({ filter, writer, false });
    if (previous_gate.filter_created)
        previous_gate.filter_created(previous_gate.context, filter, writer);
}

JS::ThrowCompletionOr<JS::Value> WorldJobGuard::State::read_filter(
    JS::Realm& reader, Web::DOM::NodeFilter* filter, JS::Value value)
{
    using enum EventValues::Activity;
    auto reject = [&]() -> JS::ThrowCompletionOr<JS::Value> {
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
        context->realm = &reader;
        vm.push_execution_context(*context);
        ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == context.ptr()); });
        return JS::throw_completion(
            JS::TypeError::create(reader, u"Firefly node filter cannot cross worlds"sv));
    };

    auto activity = EventValues::activity(reader);
    if (activity == Revoked || activity == Exiting)
        return reject();
    if (filter) {
        filters.remove_all_matching([](auto const& record) { return !record.filter.ptr(); });
        FilterRecord* origin = nullptr;
        for (auto& record : filters)
            if (record.filter.ptr().ptr() == filter) {
                origin = &record;
                break;
            }

        if (!origin) {
            if (previous_gate.read_filter)
                return previous_gate.read_filter(previous_gate.context, reader, filter, value);
            return reject();
        }

        auto& callback = filter->callback();
        auto& associated = Callbacks::callback_realm(callback);
        GC::Ptr<JS::Realm> actual;
        if (callback.callback->is_function()) {
            auto realm
                = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
            if (realm.is_error())
                return reject();
            actual = realm.value();
        }

        auto* writer = origin ? origin->writer.ptr().ptr() : nullptr;
        bool owned = activity != Unregistered || EventValues::activity(associated) != Unregistered
            || (actual && EventValues::activity(*actual) != Unregistered)
            || (writer && EventValues::activity(*writer) != Unregistered)
            || (origin && origin->revoked);
        // A foreign prototype can give a Source object a principal shape.
        // Native construction provenance must agree with same-world bypass.
        if (owned
            && (!origin || origin->revoked || writer != &reader || &associated != &reader
                || (actual && actual.ptr() != &reader)))
            return reject();
    }

    if (previous_gate.read_filter)
        return previous_gate.read_filter(previous_gate.context, reader, filter, value);
    return value;
}

JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> WorldJobGuard::State::invoke_filter(
    JS::Realm& caller, Web::DOM::NodeFilter& filter, Web::DOM::Node& node)
{
    auto& callback = filter.callback();
    auto& associated = Callbacks::callback_realm(callback);
    GC::Ptr<JS::Realm> actual;
    if (callback.callback->is_function()) {
        auto realm
            = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
        if (!realm.is_error())
            actual = realm.value();
    }

    auto delegate = [&]() -> JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> {
        if (previous_gate.invoke_filter)
            return previous_gate.invoke_filter(previous_gate.context, caller, filter, node);
        return Callbacks::invoke_filter_original(caller, filter, node);
    };

    auto invoke = [&]() -> JS::ThrowCompletionOr<JS::Value> {
        // Include native node wrapping, operation lookup and ToInt32 inside
        // the owner frame; conversion can execute arbitrary JS as well.
        return JS::Value(static_cast<i32>(TRY(delegate())));
    };

    bool own_associated = realms.contains(associated);
    bool own_actual = actual && realms.contains(*actual);
    if (!own_associated && !own_actual) {
        auto result = in_principal(associated, [&]() -> JS::Completion { return invoke(); });
        if (result.is_error())
            return result.release_error();
        return static_cast<Web::DOM::NodeFilter::Result>(result.value().as_i32());
    }

    auto reject = [&]() -> JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> {
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
        context->realm = &caller;
        vm.push_execution_context(*context);
        ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == context.ptr()); });
        return JS::throw_completion(
            JS::TypeError::create(caller, u"Firefly node filter is unavailable"sv));
    };

    if ((own_associated && blocked_realm(associated))
        || (own_actual && actual.ptr() != &associated && blocked_realm(*actual)))
        return reject();
    auto result = run_owned(own_associated ? owner_for(associated) : nullptr,
        own_actual ? owner_for(*actual) : nullptr, [&] { return invoke(); });
    if (result.is_error()) {
        ++source_job_errors;
        return reject();
    }

    return static_cast<Web::DOM::NodeFilter::Result>(result.value().as_i32());
}

}
