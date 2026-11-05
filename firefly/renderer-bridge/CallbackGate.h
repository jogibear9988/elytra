#pragma once
#include "EventValues.h"
#include <LibWeb/DOM/NodeFilter.h>
#include <LibWeb/WebIDL/AbstractOperations.h>
#include <AK/HashMap.h>
namespace Web::DOM { class MutationObserver; class Event; class DOMEventListener; class EventTarget; class AbortSignal; class TreeWalker; }
namespace Web::HTML { class EventHandler; class Timer; class WindowOrWorkerGlobalScopeMixin; }

namespace Firefly::Callbacks {
using HandlerMap = HashMap<Utf16FlyString, GC::Ref<Web::HTML::EventHandler>>;
using TimerMap = HashMap<int, GC::Ref<Web::HTML::Timer>>;
using TimerAdded = void (*)(void*, Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int, Web::HTML::Timer&);
using TimerRemoved = void (*)(void*, Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int);
using ObserverChange = void (*)(void*, Web::DOM::MutationObserver&);
using Invoke = JS::Completion (*)(void*, Web::WebIDL::CallbackType&, Optional<JS::Value>, Web::WebIDL::ExceptionBehavior, ReadonlySpan<JS::Value>);
using InvokeListener = JS::Completion (*)(void*, Web::WebIDL::CallbackType&, Web::DOM::Event&);
using InvokeHandler = JS::Completion (*)(void*, Web::WebIDL::CallbackType&, Web::DOM::Event&, bool);
using ListenerChange = void (*)(void*, Web::DOM::EventTarget&, Web::DOM::DOMEventListener&);
using ListenerSignal = void (*)(void*, Web::DOM::EventTarget&, Web::DOM::DOMEventListener&, u64);
using SelectHandlerMap = HandlerMap* (*)(void*, Web::DOM::EventTarget&);
using VisitHandlerMaps = void (*)(void*, Web::DOM::EventTarget&, GC::Cell::Visitor&);
using ClearHandlerMaps = void (*)(void*, Web::DOM::EventTarget&);
using ReportException = void (*)(void*, Web::WebIDL::CallbackType&, JS::Value);
using InvokeFilter = JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> (*)(void*, JS::Realm&, Web::DOM::NodeFilter&, Web::DOM::Node&);
using FilterCreated = void (*)(void*, Web::DOM::NodeFilter&, JS::Realm*);
using ReadFilter = JS::ThrowCompletionOr<JS::Value> (*)(void*, JS::Realm&, Web::DOM::NodeFilter*, JS::Value);
struct Gate {
    void* context { nullptr };
    Invoke invoke { nullptr };
    TimerAdded added { nullptr };
    TimerRemoved removed { nullptr };
    ObserverChange observer_added { nullptr };
    ObserverChange observer_removed { nullptr };
    InvokeListener invoke_listener { nullptr };
    ListenerChange listener_changed { nullptr };
    ListenerSignal listener_signal { nullptr };
    InvokeHandler invoke_handler { nullptr };
    SelectHandlerMap select_handler_map { nullptr };
    VisitHandlerMaps visit_handler_maps { nullptr };
    ClearHandlerMaps clear_handler_maps { nullptr };
    EventValues::Hooks event_values {};
    ReportException report_exception { nullptr };
    InvokeFilter invoke_filter { nullptr };
    FilterCreated filter_created { nullptr };
    ReadFilter read_filter { nullptr };
};
WEB_API JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> invoke_filter_original(JS::Realm&, Web::DOM::NodeFilter&, Web::DOM::Node&);
WEB_API JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> dispatch_filter(JS::Realm&, Web::DOM::NodeFilter&, Web::DOM::Node&);
WEB_API Web::DOM::NodeFilter* probe_filter(Web::DOM::TreeWalker&);
WEB_API void filter_created(Web::DOM::NodeFilter&, JS::Realm*);
WEB_API JS::ThrowCompletionOr<JS::Value> read_filter(JS::Realm&, Web::DOM::NodeFilter*, JS::Value);
WEB_API void observer_added(Web::DOM::MutationObserver&);
WEB_API void observer_removed(Web::DOM::MutationObserver&);
WEB_API void disconnect_observer(Web::DOM::MutationObserver&);
WEB_API size_t take_observer_record_count(Web::DOM::MutationObserver&);
WEB_API bool observer_has_nodes(Web::DOM::MutationObserver&);
WEB_API void timer_added(Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int, Web::HTML::Timer&);
WEB_API void timer_removed(Web::HTML::WindowOrWorkerGlobalScopeMixin&, TimerMap&, int);
WEB_API void stop_timer(Web::HTML::Timer&);
WEB_API bool timer_active(Web::HTML::Timer&);
WEB_API Gate exchange_gate(JS::VM&, Gate);
WEB_API void restore_gate(JS::VM&, void* context, Gate previous);
WEB_API void listener_changed(Web::DOM::EventTarget&, Web::DOM::DOMEventListener&);
WEB_API void listener_signal(Web::DOM::EventTarget&, Web::DOM::DOMEventListener&, u64);
WEB_API void remove_listener(Web::DOM::EventTarget&, Web::DOM::DOMEventListener&);
WEB_API bool has_listener(Web::DOM::EventTarget&, Web::DOM::DOMEventListener&);
WEB_API void remove_signal_algorithm(Web::DOM::AbortSignal&, u64);
WEB_API JS::Completion probe_report(JS::Object&);
WEB_API JS::Completion probe_handler(JS::Object&, Web::DOM::Event&);
WEB_API void clear_handler_map(Web::DOM::EventTarget&, HandlerMap&);
WEB_API GC::Ref<Web::DOM::Event> make_beforeunload_probe();
WEB_API bool is_beforeunload_event(Web::DOM::Event&);
WEB_API void report_exception_original(Web::WebIDL::CallbackType&, JS::Value);
WEB_API void dispatch_exception(Web::WebIDL::CallbackType&, JS::Value);
WEB_API JS::Completion dispatch_handler(Web::WebIDL::CallbackType&, Web::DOM::Event&, bool);
WEB_API JS::Completion invoke_handler_original(Web::WebIDL::CallbackType&, Web::DOM::Event&, bool);
WEB_API JS::Completion dispatch_listener(Web::WebIDL::CallbackType&, Web::DOM::Event&);
WEB_API JS::Realm& callback_realm(Web::WebIDL::CallbackType&);
WEB_API JS::Completion invoke_listener_original(Web::WebIDL::CallbackType&, Web::DOM::Event&);
WEB_API JS::Completion invoke_original(Web::WebIDL::CallbackType&, Optional<JS::Value>, Web::WebIDL::ExceptionBehavior, ReadonlySpan<JS::Value>);
}
