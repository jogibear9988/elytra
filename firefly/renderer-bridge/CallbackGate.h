#pragma once
#include <LibWeb/WebIDL/AbstractOperations.h>
#include <AK/HashMap.h>
namespace Web::HTML { class Timer; class WindowOrWorkerGlobalScopeMixin; }

namespace Firefly::Callbacks {
using TimerMap = HashMap<int, GC::Ref<Web::HTML::Timer>>;
using TimerAdded = void (*)(void*, Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int, Web::HTML::Timer&);
using TimerRemoved = void (*)(void*, Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int);
using Invoke = JS::Completion (*)(void*, Web::WebIDL::CallbackType&, Optional<JS::Value>, Web::WebIDL::ExceptionBehavior, ReadonlySpan<JS::Value>);
struct Gate {
    void* context { nullptr };
    Invoke invoke { nullptr };
    TimerAdded added { nullptr };
    TimerRemoved removed { nullptr };
};
WEB_API void timer_added(Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int, Web::HTML::Timer&);
WEB_API void timer_removed(Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int);
WEB_API void stop_timer(Web::HTML::Timer&);
WEB_API bool timer_active(Web::HTML::Timer&);
WEB_API Gate exchange_gate(JS::VM&, Gate);
WEB_API void restore_gate(JS::VM&, void* context, Gate previous);
WEB_API JS::Completion invoke_original(Web::WebIDL::CallbackType&, Optional<JS::Value>, Web::WebIDL::ExceptionBehavior, ReadonlySpan<JS::Value>);
}
