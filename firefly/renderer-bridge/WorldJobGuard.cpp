#include "WorldJobGuard.h"
#include "CallbackGate.h"
#include <LibWeb/WebIDL/CallbackType.h>
#include <LibGC/WeakHashMap.h>
#include <LibGC/WeakHashSet.h>
#include <AK/WeakPtr.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/JobCallback.h>
#include <LibJS/Runtime/Promise.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/MutationObserver.h>
#include <LibWeb/DOM/DOMEventListener.h>
#include <LibWeb/DOM/IDLEventListener.h>
#include <LibWeb/DOM/AbortSignal.h>
#include <LibWeb/DOM/MutationRecord.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibWeb/HTML/BeforeUnloadEvent.h>
#include <LibWeb/HTML/EventHandler.h>
#include <LibWeb/HTML/EventNames.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/HTML/Timer.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <AK/ScopeGuard.h>

namespace Firefly {
struct WorldJobGuard::State : public RefCounted<State> {
    JS::VM& vm;
    Function<JS::ThrowCompletionOr<JS::Value>(JS::JobCallback&, JS::Value, ReadonlySpan<JS::Value>)> upstream;
    Function<void(JS::PromiseJob, GC::Ptr<JS::Realm>)> upstream_enqueue;
    Function<void(JS::Promise&, JS::Promise::RejectionOperation)> upstream_rejections;
    GC::WeakHashMap<JS::Realm, WeakPtr<IsolatedWorld>> realms;
    GC::WeakHashSet<JS::Promise> redacted_promises;
    Callbacks::Gate previous_gate;
    struct TimerRecord {
        GC::Weak<JS::Realm> realm;
        GC::Weak<Web::HTML::Window> global;
        GC::Weak<Web::HTML::Timer> timer;
        Callbacks::TimerMap* map;
        int id;
    };
    Vector<TimerRecord> timers;
    struct ObserverRecord {
        GC::Weak<JS::Realm> realm;
        GC::Weak<Web::DOM::MutationObserver> observer;
    };
    Vector<ObserverRecord> observers;
    struct ListenerRecord {
        GC::Weak<JS::Realm> realm;
        GC::Weak<Web::DOM::EventTarget> target;
        GC::Weak<Web::DOM::DOMEventListener> listener;
    };
    struct SignalRecord {
        GC::Weak<JS::Realm> realm;
        GC::Weak<Web::DOM::DOMEventListener> listener;
        GC::Weak<Web::DOM::AbortSignal> signal;
        u64 id;
    };
    Vector<ListenerRecord> listeners;
    Vector<SignalRecord> listener_signals;
    struct HandlerRecord {
        GC::Weak<JS::Realm> realm;
        GC::Weak<Web::DOM::EventTarget> target;
        OwnPtr<Callbacks::HandlerMap> map;
    };
    Vector<HandlerRecord> handler_maps;
    size_t canceled_handler_maps { 0 };
    size_t canceled_listeners { 0 };
    size_t canceled_listener_signals { 0 };
    size_t canceled_observers { 0 };
    size_t canceled_timers { 0 };
    size_t blocked { 0 };
    size_t source_rejections { 0 };
    size_t source_job_errors { 0 };
    explicit State(JS::VM& vm)
        : vm(vm), upstream(move(vm.host_call_job_callback)), upstream_enqueue(move(vm.host_enqueue_promise_job)),
          upstream_rejections(move(vm.host_promise_rejection_tracker))
    {
        VERIFY(upstream); VERIFY(upstream_enqueue); VERIFY(upstream_rejections);
        previous_gate = Callbacks::exchange_gate(vm, {this,+[](void* context, Web::WebIDL::CallbackType& callback, Optional<JS::Value> receiver, Web::WebIDL::ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments) {
            return static_cast<State*>(context)->invoke_webidl(callback, move(receiver), behavior, arguments);
        },+[](void* context, Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id, Web::HTML::Timer& timer) {
            static_cast<State*>(context)->add_timer(global, map, id, timer);
        },+[](void* context, Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id) {
            static_cast<State*>(context)->remove_timer(global, map, id);
        },+[](void* context, Web::DOM::MutationObserver& observer) {
            static_cast<State*>(context)->add_observer(observer);
        },+[](void* context, Web::DOM::MutationObserver& observer) {
            static_cast<State*>(context)->remove_observer(observer);
        },+[](void* context, Web::WebIDL::CallbackType& callback, Web::DOM::Event& event) {
            return static_cast<State*>(context)->invoke_listener(callback, event);
        },+[](void* context, Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener) {
            static_cast<State*>(context)->change_listener(target, listener);
        },+[](void* context, Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener, u64 id) {
            static_cast<State*>(context)->add_listener_signal(target, listener, id);
        },+[](void* context, Web::WebIDL::CallbackType& callback, Web::DOM::Event& event, bool special) {
            return static_cast<State*>(context)->invoke_handler(callback, event, special);
        },+[](void* context, Web::DOM::EventTarget& target) {
            return static_cast<State*>(context)->select_handler_map(target);
        },+[](void* context, Web::DOM::EventTarget& target, GC::Cell::Visitor& visitor) {
            static_cast<State*>(context)->visit_handler_maps(target, visitor);
        },+[](void* context, Web::DOM::EventTarget& target) {
            auto& state = *static_cast<State*>(context);
            state.clear_handler_maps(nullptr, &target);
            if (state.previous_gate.clear_handler_maps) state.previous_gate.clear_handler_maps(state.previous_gate.context, target);
        }});
    }
    ~State() { Callbacks::restore_gate(vm, this, previous_gate); }
    Callbacks::HandlerMap* select_handler_map(Web::DOM::EventTarget& target)
    {
        auto* realm = vm.current_realm();
        if (!realm || !realms.contains(*realm)) {
            if (previous_gate.select_handler_map) return previous_gate.select_handler_map(previous_gate.context, target);
            return nullptr;
        }
        handler_maps.remove_all_matching([](auto const& record) { return !record.realm.ptr() || !record.target.ptr(); });
        for (auto& record : handler_maps)
            if (record.realm.ptr().ptr() == realm && record.target.ptr().ptr() == &target) return record.map.ptr();
        auto map = make<Callbacks::HandlerMap>();
        auto* result = map.ptr();
        handler_maps.append({ *realm, target, move(map) });
        auto owner = owner_for(*realm);
        if (!owner || (owner->is_detached() && !owner->is_executing())) {
            // A foreign direct call into an already revoked realm must never
            // fall back to the page map. Clear any late writes after return.
            NonnullRefPtr self { *this };
            Web::HTML::queue_a_microtask(nullptr, GC::create_function(vm.heap(),
                [self, realm_root = GC::Ref { *realm }, target_root = GC::Ref { target }] {
                    self->clear_handler_maps(realm_root.ptr(), target_root.ptr());
                }));
        }
        return result;
    }
    void visit_handler_maps(Web::DOM::EventTarget& target, GC::Cell::Visitor& visitor)
    {
        for (auto const& record : handler_maps)
            if (record.target.ptr().ptr() == &target && record.realm.ptr()) visitor.visit(*record.map);
        if (previous_gate.visit_handler_maps) previous_gate.visit_handler_maps(previous_gate.context, target, visitor);
    }
    void clear_handler_maps(JS::Realm* realm, Web::DOM::EventTarget* target)
    {
        struct Removal { GC::Root<Web::DOM::EventTarget> target; OwnPtr<Callbacks::HandlerMap> map; };
        Vector<Removal> removed;
        handler_maps.remove_all_matching([&](auto& record) {
            if ((!realm || record.realm.ptr().ptr() == realm) && (!target || record.target.ptr().ptr() == target) && record.target.ptr()) {
                removed.append({ GC::Root { *record.target.ptr() }, move(record.map) }); return true;
            }
            return !record.realm.ptr() || !record.target.ptr();
        });
        // Native EventTargets normally trace these maps. Once removed from the
        // registry, keep all handler cells rooted through native cleanup/reentry.
        Vector<GC::Root<Web::HTML::EventHandler>> roots;
        for (auto const& record : removed) for (auto const& entry : *record.map) roots.append(GC::Root { entry.value });
        for (auto const& record : removed) { Callbacks::clear_handler_map(*record.target, *record.map); ++canceled_handler_maps; }
    }
    void release_signals(Web::DOM::DOMEventListener& listener)
    {
        Vector<SignalRecord> removed;
        listener_signals.remove_all_matching([&](auto const& record) {
            if (record.listener.ptr().ptr() == &listener) { removed.append(record); return true; }
            return !record.realm.ptr() || !record.signal.ptr() || !record.listener.ptr();
        });
        for (auto const& record : removed) if (record.signal.ptr()) Callbacks::remove_signal_algorithm(*record.signal.ptr(), record.id);
    }
    void change_listener(Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener)
    {
        if (listener.removed) {
            listeners.remove_all_matching([&](auto const& record) { return record.listener.ptr().ptr() == &listener; });
            release_signals(listener);
        } else if (listener.callback) {
            auto& realm = Callbacks::callback_realm(listener.callback->callback());
            if (realms.contains(realm)) {
                listeners.remove_all_matching([](auto const& record) { return !record.realm.ptr() || !record.target.ptr() || !record.listener.ptr(); });
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
        if (previous_gate.listener_changed) previous_gate.listener_changed(previous_gate.context, target, listener);
    }
    void add_listener_signal(Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener, u64 id)
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
                listener_signals.remove_all_matching([](auto const& record) { return !record.realm.ptr() || !record.listener.ptr() || !record.signal.ptr(); });
                listener_signals.append({ realm, listener, *listener.signal, id });
                return;
            }
        }
        if (previous_gate.listener_signal) previous_gate.listener_signal(previous_gate.context, target, listener, id);
    }
    void cancel_listeners(JS::Realm& realm)
    {
        // Remove metadata first so native remove/notification reentry sees a
        // consistent registry. Root all affected objects until cleanup finishes.
        struct Removal { GC::Root<Web::DOM::EventTarget> target; GC::Root<Web::DOM::DOMEventListener> listener; };
        struct SignalRemoval { GC::Root<Web::DOM::AbortSignal> signal; u64 id; };
        Vector<Removal> removed;
        Vector<SignalRemoval> signals;
        listeners.remove_all_matching([&](auto const& record) {
            if (record.realm.ptr().ptr() == &realm && record.target.ptr() && record.listener.ptr()) {
                removed.append({ GC::Root { *record.target.ptr() }, GC::Root { *record.listener.ptr() } }); return true;
            }
            return !record.realm.ptr() || !record.target.ptr() || !record.listener.ptr();
        });
        listener_signals.remove_all_matching([&](auto const& record) {
            if (record.realm.ptr().ptr() == &realm && record.signal.ptr()) {
                signals.append({ GC::Root { *record.signal.ptr() }, record.id }); return true;
            }
            return !record.realm.ptr() || !record.signal.ptr() || !record.listener.ptr();
        });
        for (auto const& signal : signals) { Callbacks::remove_signal_algorithm(*signal.signal, signal.id); ++canceled_listener_signals; }
        for (auto const& record : removed) { Callbacks::remove_listener(*record.target, *record.listener); ++canceled_listeners; }
    }
    JS::Completion invoke_handler(Web::WebIDL::CallbackType& callback, Web::DOM::Event& event, bool special)
    {
        auto delegate = [&] {
            if (previous_gate.invoke_handler) return previous_gate.invoke_handler(previous_gate.context, callback, event, special);
            return Callbacks::invoke_handler_original(callback, event, special);
        };
        auto& associated = Callbacks::callback_realm(callback);
        GC::Ptr<JS::Realm> actual;
        if (callback.callback->is_function()) {
            auto realm = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
            if (!realm.is_error()) actual = realm.value();
        }
        bool own_associated = realms.contains(associated);
        bool own_actual = actual && realms.contains(*actual);
        if (!own_associated && !own_actual) return delegate();
        if (own_associated && blocked_realm(associated)) return JS::js_undefined();
        if (own_actual && actual.ptr() != &associated && blocked_realm(*actual)) return JS::js_undefined();
        auto result = run_owned(own_associated ? owner_for(associated) : nullptr, own_actual ? owner_for(*actual) : nullptr,
            [&]() -> JS::ThrowCompletionOr<JS::Value> {
                auto completion = Callbacks::invoke_handler_original(callback, event, special);
                if (completion.is_error()) return JS::throw_completion(completion.release_value());
                auto value = completion.release_value();
                // No source object may escape for conversion after the owner
                // frame ends. Ordinary/error handlers consume strict booleans.
                if (event.type() == Web::HTML::EventNames::beforeunload && Callbacks::is_beforeunload_event(event)) {
                    if (value.is_nullish()) return JS::js_undefined();
                    // Upstream sets this before ToString, including when it
                    // throws. Keep that order while protecting conversion code.
                    event.set_cancelled(true);
                    auto& before = static_cast<Web::HTML::BeforeUnloadEvent&>(event);
                    if (!before.return_value().is_empty()) return JS::Value(true);
                    return JS::PrimitiveString::create(vm, TRY(value.to_utf16_string(vm)));
                }
                return value.is_boolean() ? value : JS::js_undefined();
            });
        if (result.is_error()) { ++source_job_errors; return JS::js_undefined(); }
        return result.release_value();
    }
    JS::Completion invoke_listener(Web::WebIDL::CallbackType& callback, Web::DOM::Event& event)
    {
        auto delegate = [&] {
            if (previous_gate.invoke_listener) return previous_gate.invoke_listener(previous_gate.context, callback, event);
            return Callbacks::invoke_listener_original(callback, event);
        };
        auto& associated = Callbacks::callback_realm(callback);
        GC::Ptr<JS::Realm> actual;
        if (callback.callback->is_function()) {
            auto realm = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
            if (!realm.is_error()) actual = realm.value();
        }
        bool own_associated = realms.contains(associated);
        bool own_actual = actual && realms.contains(*actual);
        if (!own_associated && !own_actual) return delegate();
        if (own_associated && blocked_realm(associated)) return JS::js_undefined();
        if (own_actual && actual.ptr() != &associated && blocked_realm(*actual)) return JS::js_undefined();
        // Guard before event/receiver wrapping, and retain ownership through
        // upstream handleEvent lookup/getters/proxy traps and invocation.
        auto result = run_owned(own_associated ? owner_for(associated) : nullptr, own_actual ? owner_for(*actual) : nullptr,
            [&]() -> JS::ThrowCompletionOr<JS::Value> {
                auto completion = Callbacks::invoke_listener_original(callback, event);
                if (completion.type() == JS::Completion::Type::Throw) return JS::throw_completion(completion.release_value());
                return completion.release_value();
            });
        if (result.is_error()) { ++source_job_errors; return JS::js_undefined(); }
        return result.release_value();
    }
    void add_observer(Web::DOM::MutationObserver& observer)
    {
        auto& callback = observer.callback();
        auto& realm = callback.callback->shape().realm();
        if (!realms.contains(realm)) {
            if (previous_gate.observer_added) previous_gate.observer_added(previous_gate.context, observer);
            return;
        }
        for (auto const& record : observers) if (record.observer.ptr().ptr() == &observer) return;
        observers.remove_all_matching([](auto const& record) { return !record.realm.ptr() || !record.observer.ptr(); });
        auto owner = owner_for(realm);
        if (!owner || (owner->is_detached() && !owner->is_executing())) {
            Callbacks::disconnect_observer(observer);
            ++canceled_observers;
            return;
        }
        observers.append({realm, observer});
    }
    void remove_observer(Web::DOM::MutationObserver& observer)
    {
        observers.remove_all_matching([&](auto const& record) { return record.observer.ptr().ptr() == &observer; });
        if (previous_gate.observer_removed) previous_gate.observer_removed(previous_gate.context, observer);
    }
    void cancel_observers(JS::Realm& realm)
    {
        Vector<GC::Root<Web::DOM::MutationObserver>> removed;
        observers.remove_all_matching([&](auto const& record) {
            if (record.realm.ptr().ptr() == &realm && record.observer.ptr()) { removed.append(GC::Root { *record.observer.ptr() }); return true; }
            return !record.realm.ptr() || !record.observer.ptr();
        });
        for (auto& observer : removed) { Callbacks::disconnect_observer(*observer); ++canceled_observers; }
    }
    void cancel_timer(TimerRecord const& record)
    {
        if (!record.global.ptr() || !record.timer.ptr()) return;
        GC::Root global { *record.global.ptr() };
        GC::Root timer { *record.timer.ptr() };
        Callbacks::stop_timer(*timer);
        auto current = record.map->get(record.id);
        if (current.has_value() && current->ptr() == timer.ptr()) global->clear_timeout(record.id);
        ++canceled_timers;
    }
    void cancel_timers(JS::Realm& realm)
    {
        Vector<TimerRecord> removed;
        timers.remove_all_matching([&](auto const& record) {
            if (record.realm.ptr().ptr() == &realm) { removed.append(record); return true; }
            return !record.realm.ptr() || !record.global.ptr() || !record.timer.ptr();
        });
        // Native clear_timeout reenters the map observer: remove metadata first.
        for (auto const& record : removed) cancel_timer(record);
    }
    void add_timer(Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id, Web::HTML::Timer& timer)
    {
        for (auto const& record : timers) {
            if (record.map == &map && record.id == id && record.timer.ptr().ptr() == &timer) return; // interval rearm
        }
        timers.remove_all_matching([&](auto const& record) { return (record.map == &map && record.id == id) || !record.global.ptr() || !record.timer.ptr(); });
        GC::Ptr<JS::Realm> origin;
        vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
            if (context.realm && realms.contains(*context.realm)) { origin = context.realm; return false; }
            return true;
        });
        auto* window = as_if<Web::HTML::Window>(global.this_impl());
        if (!origin || !window) {
            if (previous_gate.added) previous_gate.added(previous_gate.context, global, map, id, timer);
            return;
        }
        TimerRecord record { *origin, *window, timer, &map, id };
        auto owner = owner_for(*origin);
        if (!owner || (owner->is_detached() && !owner->is_executing())) {
            // The upstream path starts a newly inserted Timer after map.set.
            // Cancel at the next checkpoint, with roots, rather than stopping
            // before start and leaving an active orphan when it subsequently arms.
            NonnullRefPtr self { *this };
            Web::HTML::queue_a_microtask(nullptr, GC::create_function(vm.heap(),
                [self, record, global_root = GC::Ref { *window }, timer_root = GC::Ref { timer }] {
                    (void)global_root; (void)timer_root;
                    self->cancel_timer(record);
                }));
            return;
        }
        timers.append(move(record));
    }
    void remove_timer(Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id)
    {
        timers.remove_all_matching([&](auto const& record) { return record.map == &map && record.id == id; });
        if (previous_gate.removed) previous_gate.removed(previous_gate.context, global, map, id);
    }
    JS::Completion invoke_webidl(Web::WebIDL::CallbackType& callback, Optional<JS::Value> receiver, Web::WebIDL::ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments)
    {
        auto delegate = [&] {
            if (previous_gate.invoke) return previous_gate.invoke(previous_gate.context, callback, receiver, behavior, arguments);
            return Callbacks::invoke_original(callback, receiver, behavior, arguments);
        };
        // Promise-returning and rethrowing operations need their own conversion
        // policy. This gate protects report-style function callbacks first.
        if (behavior != Web::WebIDL::ExceptionBehavior::Report || callback.operation_returns_promise != Web::WebIDL::OperationReturnsPromise::No) return delegate();
        auto& associated = callback.callback->shape().realm();
        GC::Ptr<JS::Realm> actual;
        if (callback.callback->is_function()) {
            auto function_realm = JS::get_function_realm(vm, static_cast<JS::FunctionObject&>(*callback.callback));
            if (!function_realm.is_error()) actual = function_realm.value();
        }
        bool own_associated = realms.contains(associated);
        bool own_actual = actual && realms.contains(*actual);
        if (!own_associated && !own_actual) return delegate();
        if (own_associated && blocked_realm(associated)) return JS::js_undefined();
        if (own_actual && actual.ptr() != &associated && blocked_realm(*actual)) return JS::js_undefined();
        auto result = run_owned(own_associated ? owner_for(associated) : nullptr, own_actual ? owner_for(*actual) : nullptr,
            [&]() -> JS::ThrowCompletionOr<JS::Value> {
                // Capture the abrupt completion before the upstream Report path
                // can put its raw source object into a principal ErrorEvent.
                GC::RootVector<JS::Value> safe_arguments;
                for (auto value : arguments) safe_arguments.append(value);
                auto safe_receiver = receiver;
                if (receiver.has_value() && receiver->is_object()) {
                    auto receiver_value = *receiver;
                    auto* observer = Web::Bindings::impl_from<Web::DOM::MutationObserver>(&receiver_value.as_object());
                    if (observer && &observer->callback() == &callback) {
                        if (arguments.size() != 2 || !arguments[0].is_object())
                            return vm.throw_completion<JS::TypeError>("Invalid mutation observer delivery"sv);
                        auto source_array_value = arguments[0];
                        auto& source_array = source_array_value.as_object();
                        if (source_array.engine_class_id() != JS_LAYOUT_CLASS_ID_ARRAY || JS::host_class_of(source_array))
                            return vm.throw_completion<JS::TypeError>("Invalid mutation record list"sv);
                        auto length = TRY(source_array.internal_get_own_property("length"_utf16));
                        if (!length.has_value() || !length->value.has_value() || !length->value->is_number())
                            return vm.throw_completion<JS::TypeError>("Missing mutation record length"sv);
                        auto size = static_cast<u32>(length->value->as_double());
                        GC::Root array { TRY(JS::Array::create(associated, size)) };
                        auto& world = Web::Bindings::host_defined_wrapper_world(associated);
                        for (u32 i = 0; i < size; ++i) {
                            auto descriptor = TRY(source_array.ordinary_get_own_property(i));
                            if (!descriptor.has_value() || !descriptor->is_data_descriptor() || !descriptor->value.has_value() || !descriptor->value->is_object())
                                return vm.throw_completion<JS::TypeError>("Invalid mutation record"sv);
                            auto value = *descriptor->value;
                            auto* record = Web::Bindings::impl_from<Web::DOM::MutationRecord>(&value.as_object());
                            if (!record) return vm.throw_completion<JS::TypeError>("Unknown mutation record"sv);
                            TRY(array->create_data_property_or_throw(i, Web::Bindings::wrap(world, associated, GC::Ref { *record })));
                        }
                        GC::Root wrapped_observer { Web::Bindings::wrap(world, associated, GC::Ref { *observer }) };
                        safe_arguments[0] = array.ptr();
                        safe_arguments[1] = wrapped_observer.ptr();
                        safe_receiver = wrapped_observer.ptr();
                    }
                }
                auto completion = Callbacks::invoke_original(callback, safe_receiver, Web::WebIDL::ExceptionBehavior::Rethrow, safe_arguments.span());
                if (completion.type() == JS::Completion::Type::Throw) return JS::throw_completion(completion.release_value());
                return completion.release_value();
            });
        if (result.is_error()) { ++source_job_errors; return JS::js_undefined(); }
        return result.release_value();
    }
    void rejection(JS::Promise& promise, JS::Promise::RejectionOperation operation)
    {
        // Native identities only: never read/stringify a source reason or place
        // its Promise in the principal Window's rejection-event lists. A foreign
        // Promise constructor can still be rejected by an owned source frame.
        bool owned = realms.contains(promise.shape().realm());
        if (operation == JS::Promise::RejectionOperation::Reject) {
            if (!owned) {
                vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
                    if (context.realm && realms.contains(*context.realm)) { owned = true; return false; }
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
    RefPtr<IsolatedWorld> owner_for(JS::Realm& realm)
    {
        auto owner = realms.get(realm);
        return owner.has_value() ? owner->strong_ref() : nullptr;
    }
    JS::ThrowCompletionOr<JS::Value> run_owned(RefPtr<IsolatedWorld> first, RefPtr<IsolatedWorld> second,
        Function<JS::ThrowCompletionOr<JS::Value>()> operation)
    {
        auto inner = [&]() -> JS::ThrowCompletionOr<JS::Value> {
            if (second && second != first && !second->is_executing()) return second->run_async_frame([&] { return operation(); });
            return operation();
        };
        if (first && !first->is_executing()) return first->run_async_frame([&] { return inner(); });
        return inner();
    }
    bool blocked_realm(JS::Realm& realm)
    {
        auto owner = owner_for(realm);
        auto& world = Web::Bindings::host_defined_wrapper_world(realm);
        auto document = Web::Bindings::principal_host_defined_environment_settings_object(realm).responsible_document();
        if (!owner || owner->is_detached() || world.is_detached() || !document || !document->is_fully_active()) {
            if (owner) owner->detach();
            else world.detach();
            ++blocked;
            return true;
        }
        return false;
    }
    void enqueue(JS::PromiseJob job, GC::Ptr<JS::Realm> realm)
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
        if (origin && blocked_realm(*origin)) return;
        if (owned_realm && realm != origin && blocked_realm(*realm)) return;
        NonnullRefPtr self { *this };
        auto script_or_module = vm.get_active_script_or_module();
        // PromiseJob cannot be wrapped through its private constructor. Queue
        // only our owned-world jobs via the public HTML microtask/context APIs;
        // the upstream hook retains all other jobs and the VM's queue contract.
        Web::HTML::queue_a_microtask(nullptr, GC::create_function(vm.heap(),
            [self, realm, origin, owned_realm, job = move(job), script_or_module] {
                if (origin && self->blocked_realm(*origin)) return;
                if (owned_realm && realm != origin && self->blocked_realm(*realm)) return;
                // Acquire ownership before context setup can allocate. Never run
                // an unprotected job if an owner vanished after the first check.
                auto origin_owner = origin ? self->owner_for(*origin) : nullptr;
                auto realm_owner = owned_realm ? self->owner_for(*realm) : nullptr;
                if (origin && !origin_owner) { self->blocked_realm(*origin); return; }
                if (owned_realm && !realm_owner) { self->blocked_realm(*realm); return; }
                auto execution_realm = realm ? realm : origin;
                Web::HTML::TemporaryExecutionContext context(*execution_realm, Web::HTML::TemporaryExecutionContext::CallbacksEnabled::Yes);
                auto& execution = Web::Bindings::principal_host_defined_environment_settings_object(*execution_realm).realm_execution_context();
                auto previous = execution.script_or_module;
                execution.script_or_module = script_or_module;
                ScopeGuard restore([&] { execution.script_or_module = previous; });
                auto result = self->run_owned(move(origin_owner), move(realm_owner), [&] { return job.run(); });
                // The ordinary reporter may dispatch an ErrorEvent with the raw
                // source object at the shared principal Window. Record only the
                // fact of failure; no source reason/stack enters the page.
                if (result.is_error()) ++self->source_job_errors;
            }));
    }

    JS::ThrowCompletionOr<JS::Value> invoke(JS::JobCallback& callback, JS::Value receiver, ReadonlySpan<JS::Value> arguments)
    {
        // This engine operation unwraps bound functions/proxies without property
        // access or traps. Revoked proxies already cannot execute: delegate their
        // normal error path to the original hook.
        auto realm = JS::get_function_realm(vm, callback.callback());
        if (!realm.is_error() && realms.contains(*realm.value())) {
            if (blocked_realm(*realm.value())) return JS::js_undefined();
            return run_owned(owner_for(*realm.value()), nullptr, [&] { return upstream(callback, receiver, arguments); });
        }
        return upstream(callback, receiver, arguments);
    }
};
WorldJobGuard::WorldJobGuard(JS::VM& vm)
    : m_state(adopt_ref(*new State(vm)))
{
    // Keep the state alive in the VM, not through the renderer session's address.
    // Session shutdown must not reopen queued callbacks by uninstalling the hook.
    vm.host_call_job_callback = [state = m_state](JS::JobCallback& callback, JS::Value receiver, ReadonlySpan<JS::Value> arguments) {
        return state->invoke(callback, receiver, arguments);
    };
    vm.host_enqueue_promise_job = [state = m_state](JS::PromiseJob job, GC::Ptr<JS::Realm> realm) {
        state->enqueue(move(job), realm);
    };
    vm.host_promise_rejection_tracker = [state = m_state](JS::Promise& promise, JS::Promise::RejectionOperation operation) {
        state->rejection(promise, operation);
    };
}
WorldJobGuard::~WorldJobGuard() = default;
ErrorOr<void> WorldJobGuard::protect(IsolatedWorld& owner)
{
    auto* realm = owner.realm();
    if (!realm || &realm->vm() != &m_state->vm || Web::Bindings::host_defined_wrapper_world(*realm).is_main_world())
        return Error::from_string_literal("Job guard requires a live isolated world in its VM");
    if (m_state->realms.contains(*realm)) return {};
    m_state->realms.set(*realm, owner.make_weak_ptr());
    owner.add_detach_hook([state = m_state, realm = GC::Weak<JS::Realm> { *realm }] {
        if (realm.ptr()) { state->cancel_timers(*realm.ptr()); state->cancel_observers(*realm.ptr()); state->cancel_listeners(*realm.ptr()); state->clear_handler_maps(realm.ptr().ptr(), nullptr); }
    });
    return {};
}
Vector<GC::Root<Web::HTML::Timer>> WorldJobGuard::owned_timer_snapshot() const
{
    Vector<GC::Root<Web::HTML::Timer>> result;
    for (auto const& record : m_state->timers) {
        if (record.timer.ptr()) result.append(GC::Root { *record.timer.ptr() });
    }
    return result;
}
Vector<GC::Root<Web::DOM::MutationObserver>> WorldJobGuard::owned_observer_snapshot() const
{
    Vector<GC::Root<Web::DOM::MutationObserver>> result;
    for (auto const& record : m_state->observers) if (record.observer.ptr()) result.append(GC::Root { *record.observer.ptr() });
    return result;
}
Vector<GC::Root<Web::DOM::DOMEventListener>> WorldJobGuard::owned_listener_snapshot() const
{
    Vector<GC::Root<Web::DOM::DOMEventListener>> result;
    for (auto const& record : m_state->listeners) if (record.listener.ptr()) result.append(GC::Root { *record.listener.ptr() });
    return result;
}
size_t WorldJobGuard::owned_handler_map_count() const { return m_state->handler_maps.size(); }
size_t WorldJobGuard::canceled_handler_map_count() const { return m_state->canceled_handler_maps; }
size_t WorldJobGuard::owned_listener_count() const { return m_state->listeners.size(); }
size_t WorldJobGuard::owned_listener_signal_count() const { return m_state->listener_signals.size(); }
size_t WorldJobGuard::canceled_listener_count() const { return m_state->canceled_listeners; }
size_t WorldJobGuard::canceled_listener_signal_count() const { return m_state->canceled_listener_signals; }
size_t WorldJobGuard::owned_observer_count() const { return m_state->observers.size(); }
size_t WorldJobGuard::canceled_observer_count() const { return m_state->canceled_observers; }
size_t WorldJobGuard::owned_timer_count() const { return m_state->timers.size(); }
size_t WorldJobGuard::canceled_timer_count() const { return m_state->canceled_timers; }
size_t WorldJobGuard::blocked_count() const { return m_state->blocked; }
size_t WorldJobGuard::source_rejection_count() const { return m_state->source_rejections; }
size_t WorldJobGuard::source_job_error_count() const { return m_state->source_job_errors; }
}
