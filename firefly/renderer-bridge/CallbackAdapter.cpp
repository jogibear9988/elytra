// Compile the complete pinned upstream implementation once, with only the two
// public callback entry names adapted. No source is copied or patched.
#include "CallbackGate.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <LibWeb/WebIDL/CallbackType.h>
#include <LibWeb/HTML/WindowOrWorkerGlobalScope.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/DOM/MutationObserver.h>
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
