/** Track listener/abort subscriptions and validate handler/listener callback worlds. */
#include "worlds/jobs/State.h"
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/DOM/AbortSignal.h>
#include <LibWeb/DOM/DOMEventListener.h>
#include <LibWeb/DOM/IDLEventListener.h>
#include <LibWeb/HTML/BeforeUnloadEvent.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/WebIDL/CallbackType.h>

namespace Firefly {
void WorldJobGuard::State::release_signals(Web::DOM::DOMEventListener& listener)
{
    Vector<SignalRecord> removed;
    listener_signals.remove_all_matching([&](auto const& record) {
        if (record.listener.ptr().ptr() == &listener) {
            removed.append(record);
            return true;
        }

        return !record.realm.ptr() || !record.signal.ptr() || !record.listener.ptr();
    });
    for (auto const& record : removed)
        if (record.signal.ptr())
            Callbacks::remove_signal_algorithm(*record.signal.ptr(), record.id);
}

void WorldJobGuard::State::change_listener(
    Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener)
{
    if (listener.removed) {
        listeners.remove_all_matching(
            [&](auto const& record) { return record.listener.ptr().ptr() == &listener; });
        release_signals(listener);
    } else if (listener.callback) {
        auto& realm = Callbacks::callback_realm(listener.callback->callback());
        if (realms.contains(realm)) {
            listeners.remove_all_matching([](auto const& record) {
                return !record.realm.ptr() || !record.target.ptr() || !record.listener.ptr();
            });
            auto owner = owner_for(realm);
            if (!owner || (owner->is_detached() && !owner->is_executing())) {
                Callbacks::remove_listener(target, listener);
                ++canceled_listeners;
                return;
            }

            listeners.append({ realm, target, listener });
            return;
        }
    }

    if (previous_gate.listener_changed)
        previous_gate.listener_changed(previous_gate.context, target, listener);
}

void WorldJobGuard::State::add_listener_signal(
    Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener, u64 id)
{
    if (listener.callback && listener.signal) {
        auto& realm = Callbacks::callback_realm(listener.callback->callback());
        if (realms.contains(realm)) {
            auto owner = owner_for(realm);
            if (!owner || (owner->is_detached() && !owner->is_executing()) || listener.removed) {
                Callbacks::remove_signal_algorithm(*listener.signal, id);
                ++canceled_listener_signals;
                return;
            }

            listener_signals.remove_all_matching([](auto const& record) {
                return !record.realm.ptr() || !record.listener.ptr() || !record.signal.ptr();
            });
            listener_signals.append({ realm, listener, *listener.signal, id });
            return;
        }
    }

    if (previous_gate.listener_signal)
        previous_gate.listener_signal(previous_gate.context, target, listener, id);
}

void WorldJobGuard::State::cancel_listeners(JS::Realm& realm)
{
    // Remove metadata first so native remove/notification reentry sees a
    // consistent registry. Root all affected objects until cleanup finishes.
    struct Removal {
        GC::Root<Web::DOM::EventTarget> target;
        GC::Root<Web::DOM::DOMEventListener> listener;
    };

    struct SignalRemoval {
        GC::Root<Web::DOM::AbortSignal> signal;
        u64 id;
    };

    Vector<Removal> removed;
    Vector<SignalRemoval> signals;
    listeners.remove_all_matching([&](auto const& record) {
        if (record.realm.ptr().ptr() == &realm && record.target.ptr() && record.listener.ptr()) {
            removed.append(
                { GC::Root { *record.target.ptr() }, GC::Root { *record.listener.ptr() } });
            return true;
        }

        return !record.realm.ptr() || !record.target.ptr() || !record.listener.ptr();
    });
    listener_signals.remove_all_matching([&](auto const& record) {
        if (record.realm.ptr().ptr() == &realm && record.signal.ptr()) {
            signals.append({ GC::Root { *record.signal.ptr() }, record.id });
            return true;
        }

        return !record.realm.ptr() || !record.signal.ptr() || !record.listener.ptr();
    });
    for (auto const& signal : signals) {
        Callbacks::remove_signal_algorithm(*signal.signal, signal.id);
        ++canceled_listener_signals;
    }

    for (auto const& record : removed) {
        Callbacks::remove_listener(*record.target, *record.listener);
        ++canceled_listeners;
    }
}

JS::Completion WorldJobGuard::State::invoke_handler(
    Web::WebIDL::CallbackType& callback, Web::DOM::Event& event, bool special)
{
    auto delegate = [&] {
        if (previous_gate.invoke_handler)
            return previous_gate.invoke_handler(previous_gate.context, callback, event, special);
        return Callbacks::invoke_handler_original(callback, event, special);
    };

    auto& associated = Callbacks::callback_realm(callback);
    GC::Ptr<JS::Realm> actual;
    if (callback.callback->is_function()) {
        auto realm
            = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
        if (!realm.is_error())
            actual = realm.value();
    }

    bool own_associated = realms.contains(associated);
    bool own_actual = actual && realms.contains(*actual);
    if (!own_associated && !own_actual) {
        if (!has_source_context())
            return delegate();
        return in_principal(associated, [&]() -> JS::Completion {
            auto completion = delegate();
            // A foreign callback stored in an owned handler map also needs
            // beforeunload conversion inside its principal frame.
            if (!completion.is_error() && event.type() == Web::HTML::EventNames::beforeunload
                && Callbacks::is_beforeunload_event(event)) {
                auto value = completion.value();
                if (!value.is_nullish()) {
                    event.set_cancelled(true);
                    auto& before = static_cast<Web::HTML::BeforeUnloadEvent&>(event);
                    if (!before.return_value().is_empty())
                        return JS::Value(true);
                    auto string = value.to_utf16_string(vm);
                    if (string.is_error())
                        return string.release_error();
                    return JS::Value(JS::PrimitiveString::create(vm, string.release_value()).ptr());
                }
            }

            return completion;
        });
    }

    if (own_associated && blocked_realm(associated))
        return JS::js_undefined();
    if (own_actual && actual.ptr() != &associated && blocked_realm(*actual))
        return JS::js_undefined();
    auto result = run_owned(own_associated ? owner_for(associated) : nullptr,
        own_actual ? owner_for(*actual) : nullptr, [&]() -> JS::ThrowCompletionOr<JS::Value> {
            auto completion = Callbacks::invoke_handler_original(callback, event, special);
            if (completion.is_error())
                return JS::throw_completion(completion.release_value());
            auto value = completion.release_value();
            // No source object may escape for conversion after the owner
            // frame ends. Ordinary/error handlers consume strict booleans.
            if (event.type() == Web::HTML::EventNames::beforeunload
                && Callbacks::is_beforeunload_event(event)) {
                if (value.is_nullish())
                    return JS::js_undefined();
                // Upstream sets this before ToString, including when it
                // throws. Keep that order while protecting conversion code.
                event.set_cancelled(true);
                auto& before = static_cast<Web::HTML::BeforeUnloadEvent&>(event);
                if (!before.return_value().is_empty())
                    return JS::Value(true);
                return JS::PrimitiveString::create(vm, TRY(value.to_utf16_string(vm)));
            }

            return value.is_boolean() ? value : JS::js_undefined();
        });
    if (result.is_error()) {
        ++source_job_errors;
        return JS::js_undefined();
    }

    return result.release_value();
}

JS::Completion WorldJobGuard::State::invoke_listener(
    Web::WebIDL::CallbackType& callback, Web::DOM::Event& event)
{
    auto delegate = [&] {
        if (previous_gate.invoke_listener)
            return previous_gate.invoke_listener(previous_gate.context, callback, event);
        return Callbacks::invoke_listener_original(callback, event);
    };

    auto& associated = Callbacks::callback_realm(callback);
    GC::Ptr<JS::Realm> actual;
    if (callback.callback->is_function()) {
        auto realm
            = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
        if (!realm.is_error())
            actual = realm.value();
    }

    bool own_associated = realms.contains(associated);
    bool own_actual = actual && realms.contains(*actual);
    if (!own_associated && !own_actual)
        return in_principal(associated, [&]() -> JS::Completion { return delegate(); });
    if (own_associated && blocked_realm(associated))
        return JS::js_undefined();
    if (own_actual && actual.ptr() != &associated && blocked_realm(*actual))
        return JS::js_undefined();
    // Guard before event/receiver wrapping, and retain ownership through
    // upstream handleEvent lookup/getters/proxy traps and invocation.
    auto result = run_owned(own_associated ? owner_for(associated) : nullptr,
        own_actual ? owner_for(*actual) : nullptr, [&]() -> JS::ThrowCompletionOr<JS::Value> {
            auto completion = Callbacks::invoke_listener_original(callback, event);
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

}
