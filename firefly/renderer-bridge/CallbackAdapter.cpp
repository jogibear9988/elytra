// Compile the complete pinned upstream implementation once, with only the two
// public callback entry names adapted. No source is copied or patched.
#include "CallbackGate.h"
#include "HandlerStorage.h"
#include <LibWeb/HTML/EventHandler.h>
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <LibWeb/WebIDL/CallbackType.h>
#include <LibWeb/HTML/WindowOrWorkerGlobalScope.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/DOM/MutationObserver.h>
#include <LibWeb/DOM/AbortSignal.h>
#include <LibWeb/DOM/CustomEvent.h>
#include <LibWeb/HTML/BeforeUnloadEvent.h>
#include <LibWeb/HTML/EventNames.h>
// Preserve the unrelated class member declaration under this token name.
#include <LibWeb/PerformanceTimeline/PerformanceObserver.h>
namespace Web::WebIDL {
JS::Completion firefly_original_invoke_callback(CallbackType&, Optional<JS::Value>, ExceptionBehavior, ReadonlySpan<JS::Value>);
JS::Completion firefly_original_invoke_callback(CallbackType&, Optional<JS::Value>, ReadonlySpan<JS::Value>);
}
#define invoke_callback firefly_original_invoke_callback
#include <LibWeb/WebIDL/AbstractOperations.cpp>
#undef invoke_callback

namespace Firefly::Callbacks {
static thread_local NeverDestroyed<HashMap<JS::VM*, Gate>> gates;
static thread_local JS::VM* principal_handler_vm;
static thread_local Web::DOM::EventTarget* handler_override_target;
static thread_local HandlerMap* handler_override_map;
JS::Completion probe_report(JS::Object& function)
{
    GC::Root callback { function.heap().allocate<Web::WebIDL::CallbackType>(function, function.shape().realm()) };
    return Web::WebIDL::invoke_callback(*callback, JS::js_undefined(), Web::WebIDL::ExceptionBehavior::Report, {});
}
JS::Completion probe_handler(JS::Object& function, Web::DOM::Event& event)
{
    GC::Root callback { function.heap().allocate<Web::WebIDL::CallbackType>(function, function.shape().realm()) };
    return dispatch_handler(*callback, event, false);
}
void clear_handler_map(Web::DOM::EventTarget& target, HandlerMap& map)
{
    auto* previous_target = handler_override_target;
    auto* previous_map = handler_override_map;
    handler_override_target = &target;
    handler_override_map = &map;
    ScopeGuard restore([&] { handler_override_target = previous_target; handler_override_map = previous_map; });
    GC::RootVector<GC::Ref<Web::HTML::EventHandler>> roots;
    for (auto const& entry : map) roots.append(entry.value);
    for (auto const& name : map.keys()) target.set_event_handler_attribute(name, nullptr);
    VERIFY(map.is_empty());
}
void observer_added(Web::DOM::MutationObserver& observer)
{
    auto gate = gates->get(&observer.vm());
    if (gate.has_value() && gate->observer_added) gate->observer_added(gate->context, observer);
}
void observer_removed(Web::DOM::MutationObserver& observer)
{
    auto gate = gates->get(&observer.vm());
    if (gate.has_value() && gate->observer_removed) gate->observer_removed(gate->context, observer);
}
void disconnect_observer(Web::DOM::MutationObserver& observer) { observer.disconnect(); }
size_t take_observer_record_count(Web::DOM::MutationObserver& observer) { return observer.take_records().size(); }
bool observer_has_nodes(Web::DOM::MutationObserver& observer) { return !observer.node_list().is_empty(); }
void timer_added(Web::HTML::WindowOrWorkerGlobalScopeMixin& global, TimerMap& map, int id, Web::HTML::Timer& timer)
{
    auto gate = gates->get(&global.this_impl().vm());
    if (gate.has_value() && gate->added) gate->added(gate->context, global, map, id, timer);
}
void timer_removed(Web::HTML::WindowOrWorkerGlobalScopeMixin& global, TimerMap& map, int id)
{
    auto gate = gates->get(&global.this_impl().vm());
    if (gate.has_value() && gate->removed) gate->removed(gate->context, global, map, id);
}
void stop_timer(Web::HTML::Timer& timer) { timer.stop(); }
bool timer_active(Web::HTML::Timer& timer) { return timer.is_active(); }
Gate exchange_gate(JS::VM& vm, Gate next)
{
    auto old = gates->get(&vm).value_or(Gate {});
    gates->set(&vm, next);
    return old;
}
void restore_gate(JS::VM& vm, void* context, Gate previous)
{
    auto current = gates->get(&vm);
    if (!current.has_value() || current->context != context) return;
    if (previous.invoke) gates->set(&vm, previous);
    else gates->remove(&vm);
}
void listener_changed(Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener)
{
    auto gate = gates->get(&target.vm());
    if (gate.has_value() && gate->listener_changed) gate->listener_changed(gate->context, target, listener);
}
void listener_signal(Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener, u64 id)
{
    auto gate = gates->get(&target.vm());
    if (gate.has_value() && gate->listener_signal) gate->listener_signal(gate->context, target, listener, id);
}
void remove_listener(Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener) { target.remove_an_event_listener(listener); }
bool has_listener(Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener)
{
    auto entries = target.event_listener_list_matching(listener.type, {});
    return entries.contains([&](auto const& entry) { return entry.ptr() == &listener; });
}
void remove_signal_algorithm(Web::DOM::AbortSignal& signal, u64 id)
{
    // An abort in progress iterates this map; it clears it itself immediately
    // afterwards. Do not mutate its iterator from a reentrant native cleanup.
    if (!signal.aborted()) signal.remove_abort_algorithm(id);
}
bool is_beforeunload_event(Web::DOM::Event& event) { return is<Web::HTML::BeforeUnloadEvent>(event); }
GC::Ref<Web::DOM::Event> make_beforeunload_probe()
{
    return Web::HTML::BeforeUnloadEvent::create(Web::HTML::EventNames::beforeunload, {.cancelable=true}, 0);
}
void dispatch_exception(Web::WebIDL::CallbackType& callback, JS::Value exception)
{
    auto gate = gates->get(&callback.callback->vm());
    if (gate.has_value() && gate->report_exception) { gate->report_exception(gate->context, callback, exception); return; }
    report_exception_original(callback, exception);
}
JS::Completion dispatch_handler(Web::WebIDL::CallbackType& callback, Web::DOM::Event& event, bool special)
{
    auto gate = gates->get(&callback.callback->vm());
    if (gate.has_value() && gate->invoke_handler) return gate->invoke_handler(gate->context, callback, event, special);
    return invoke_handler_original(callback, event, special);
}
JS::Completion dispatch_listener(Web::WebIDL::CallbackType& callback, Web::DOM::Event& event)
{
    auto gate = gates->get(&callback.callback->vm());
    if (gate.has_value() && gate->invoke_listener) return gate->invoke_listener(gate->context, callback, event);
    return invoke_listener_original(callback, event);
}
JS::Realm& callback_realm(Web::WebIDL::CallbackType& callback) { return callback.associated_realm(); }
JS::Completion invoke_original(Web::WebIDL::CallbackType& callback, Optional<JS::Value> receiver, Web::WebIDL::ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments)
{
    return Web::WebIDL::firefly_original_invoke_callback(callback, move(receiver), behavior, arguments);
}
}
namespace Web::WebIDL {
JS::Completion invoke_callback(CallbackType& callback, Optional<JS::Value> receiver, ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments)
{
    auto gate = Firefly::Callbacks::gates->get(&callback.callback->vm());
    if (gate.has_value() && gate->invoke) return gate->invoke(gate->context, callback, move(receiver), behavior, arguments);
    return firefly_original_invoke_callback(callback, move(receiver), behavior, arguments);
}
JS::Completion invoke_callback(CallbackType& callback, Optional<JS::Value> receiver, ReadonlySpan<JS::Value> arguments)
{
    return invoke_callback(callback, move(receiver), ExceptionBehavior::NotSpecified, arguments);
}
}

namespace Firefly::Handlers {
PrincipalScope::PrincipalScope(JS::VM& vm) : m_previous(Callbacks::principal_handler_vm) { Callbacks::principal_handler_vm = &vm; }
PrincipalScope::~PrincipalScope() { Callbacks::principal_handler_vm = m_previous; }
Callbacks::HandlerMap& select_map(Web::DOM::EventTarget& target, Callbacks::HandlerMap& principal)
{
    if (Callbacks::handler_override_target == &target) return *Callbacks::handler_override_map;
    if (Callbacks::principal_handler_vm == &target.vm()) return principal;
    auto gate = Callbacks::gates->get(&target.vm());
    if (gate.has_value() && gate->select_handler_map) {
        if (auto* map = gate->select_handler_map(gate->context, target)) return *map;
    }
    return principal;
}
JS::Realm& listener_realm(Web::DOM::EventTarget& target, JS::Realm& fallback)
{
    if (Callbacks::principal_handler_vm == &target.vm()) return fallback;
    auto gate = Callbacks::gates->get(&target.vm());
    if (gate.has_value() && gate->select_handler_map && gate->select_handler_map(gate->context, target))
        return *target.vm().current_realm();
    return fallback;
}
void visit_maps(Web::DOM::EventTarget& target, GC::Cell::Visitor& visitor)
{
    auto gate = Callbacks::gates->get(&target.vm());
    if (gate.has_value() && gate->visit_handler_maps) gate->visit_handler_maps(gate->context, target, visitor);
}
void clear_maps(Web::DOM::EventTarget& target)
{
    auto gate = Callbacks::gates->get(&target.vm());
    if (gate.has_value() && gate->clear_handler_maps) gate->clear_handler_maps(gate->context, target);
}
}

namespace Firefly::EventValues {
Activity activity(JS::Realm& realm)
{
    auto gate = Callbacks::gates->get(&realm.vm());
    if (gate.has_value() && gate->event_values.activity) return gate->event_values.activity(gate->context, realm);
    return Activity::Unregistered;
}
void changed(Web::DOM::Event& event, JS::Realm* writer)
{
    auto gate = Callbacks::gates->get(&event.vm());
    if (gate.has_value() && gate->event_values.changed) gate->event_values.changed(gate->context, event, writer);
}
void visit(Web::DOM::Event& event, GC::Cell::Visitor& visitor)
{
    auto gate = Callbacks::gates->get(&event.vm());
    if (gate.has_value() && gate->event_values.visit) gate->event_values.visit(gate->context, event, visitor);
}
JS::ThrowCompletionOr<JS::Value> read(JS::Realm& realm, Web::DOM::Event& event, JS::Value value)
{
    auto gate = Callbacks::gates->get(&event.vm());
    if (gate.has_value() && gate->event_values.read) return gate->event_values.read(gate->context, realm, event, value);
    return value;
}
void revoke(JS::Realm& realm)
{
    auto gate = Callbacks::gates->get(&realm.vm());
    if (gate.has_value() && gate->event_values.revoke) gate->event_values.revoke(gate->context, realm);
}
}
