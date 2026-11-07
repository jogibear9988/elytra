/** Install/restore the WebIDL callback gate; its context is the VM-retained State. */
#include "worlds/jobs/State.h"

namespace Firefly {
WorldJobGuard::State::State(JS::VM& vm)
    : vm(vm)
    , upstream(move(vm.host_call_job_callback))
    , upstream_enqueue(move(vm.host_enqueue_promise_job))
    , upstream_rejections(move(vm.host_promise_rejection_tracker))
{
    VERIFY(upstream);
    VERIFY(upstream_enqueue);
    VERIFY(upstream_rejections);
    previous_gate = Callbacks::exchange_gate(vm,
        { this,
            +[](void* context, Web::WebIDL::CallbackType& callback, Optional<JS::Value> receiver,
                 Web::WebIDL::ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments) {
                return static_cast<State*>(context)->invoke_webidl(
                    callback, move(receiver), behavior, arguments);
            },
            +[](void* context, Web::HTML::WindowOrWorkerGlobalScopeMixin& global,
                 Callbacks::TimerMap& map, int id, Web::HTML::Timer& timer) {
                static_cast<State*>(context)->add_timer(global, map, id, timer);
            },
            +[](void* context, Web::HTML::WindowOrWorkerGlobalScopeMixin& global,
                 Callbacks::TimerMap& map,
                 int id) { static_cast<State*>(context)->remove_timer(global, map, id); },
            +[](void* context, Web::DOM::MutationObserver& observer) {
                static_cast<State*>(context)->add_observer(observer);
            },
            +[](void* context, Web::DOM::MutationObserver& observer) {
                static_cast<State*>(context)->remove_observer(observer);
            },
            +[](void* context, Web::WebIDL::CallbackType& callback, Web::DOM::Event& event) {
                return static_cast<State*>(context)->invoke_listener(callback, event);
            },
            +[](void* context, Web::DOM::EventTarget& target,
                 Web::DOM::DOMEventListener& listener) {
                static_cast<State*>(context)->change_listener(target, listener);
            },
            +[](void* context, Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener,
                 u64 id) {
                static_cast<State*>(context)->add_listener_signal(target, listener, id);
            },
            +[](void* context, Web::WebIDL::CallbackType& callback, Web::DOM::Event& event,
                 bool special) {
                return static_cast<State*>(context)->invoke_handler(callback, event, special);
            },
            +[](void* context, Web::DOM::EventTarget& target) {
                return static_cast<State*>(context)->select_handler_map(target);
            },
            +[](void* context, Web::DOM::EventTarget& target, GC::Cell::Visitor& visitor) {
                static_cast<State*>(context)->visit_handler_maps(target, visitor);
            },
            +[](void* context, Web::DOM::EventTarget& target) {
                auto& state = *static_cast<State*>(context);
                state.clear_handler_maps(nullptr, &target);
                if (state.previous_gate.clear_handler_maps)
                    state.previous_gate.clear_handler_maps(state.previous_gate.context, target);
            },
            { +[](void* context, JS::Realm& realm) {
                 return static_cast<State*>(context)->event_activity(realm);
             },
                +[](void* context, Web::DOM::Event& event, JS::Realm* writer) {
                    static_cast<State*>(context)->event_changed(event, writer);
                },
                +[](void* context, Web::DOM::Event& event, GC::Cell::Visitor& visitor) {
                    static_cast<State*>(context)->visit_event_values(event, visitor);
                },
                +[](void* context, JS::Realm& realm, Web::DOM::Event& event, JS::Value value) {
                    return static_cast<State*>(context)->read_event_value(realm, event, value);
                },
                +[](void* context, JS::Realm& realm) {
                    static_cast<State*>(context)->revoke_event_values(realm);
                } },
            +[](void* context, Web::WebIDL::CallbackType& callback, JS::Value exception) {
                static_cast<State*>(context)->report_exception(callback, exception);
            },
            +[](void* context, JS::Realm& realm, Web::DOM::NodeFilter& filter,
                 Web::DOM::Node& node) {
                return static_cast<State*>(context)->invoke_filter(realm, filter, node);
            },
            +[](void* context, Web::DOM::NodeFilter& filter, JS::Realm* writer) {
                static_cast<State*>(context)->filter_created(filter, writer);
            },
            +[](void* context, JS::Realm& realm, Web::DOM::NodeFilter* filter, JS::Value value) {
                return static_cast<State*>(context)->read_filter(realm, filter, value);
            } });
}

WorldJobGuard::State::~State()
{
    Callbacks::restore_gate(vm, this, previous_gate);
}

}
