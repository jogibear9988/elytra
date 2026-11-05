// Compile the complete pinned upstream implementation once, with only the two
// public callback entry names adapted. No source is copied or patched.
#include "CallbackGate.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <LibWeb/WebIDL/CallbackType.h>
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
