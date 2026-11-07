/**
 * Private VM-retained world-guard state. Registries stay weak; callback hooks
 * and queued jobs retain this single owner through VM teardown. Member units
 * implement separate policies without changing registration or revocation order.
 */
#pragma once
#include "callbacks/CallbackGate.h"
#include "worlds/WorldJobGuard.h"
#include <AK/WeakPtr.h>
#include <LibGC/WeakHashMap.h>
#include <LibGC/WeakHashSet.h>
#include <LibJS/Runtime/Promise.h>

namespace Firefly {
struct WorldJobGuard::State : public RefCounted<State> {
    JS::VM& vm;
    // Saved hooks receive all unowned work; wrappers never substitute a second VM queue.
    Function<JS::ThrowCompletionOr<JS::Value>(JS::JobCallback&, JS::Value, ReadonlySpan<JS::Value>)>
        upstream;
    Function<void(JS::PromiseJob, GC::Ptr<JS::Realm>)> upstream_enqueue;
    Function<void(JS::Promise&, JS::Promise::RejectionOperation)> upstream_rejections;
    // Registration never roots a realm/world. Execution acquires temporary strong owners.
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

    // Values are traced through the native Event visitor, not by rooting weak registry keys.
    struct EventCopy {
        GC::Weak<JS::Realm> target;
        JS::Value value;
    };

    struct EventRecord {
        GC::Weak<Web::DOM::Event> event;
        GC::Weak<JS::Realm> writer;
        bool writer_owned { false };
        bool revoked { false };
        Vector<EventCopy> copies;
    };

    Vector<EventRecord> event_values;

    struct FilterRecord {
        GC::Weak<Web::DOM::NodeFilter> filter;
        GC::Weak<JS::Realm> writer;
        bool revoked { false };
    };

    Vector<FilterRecord> filters;
    size_t canceled_listeners { 0 };
    size_t canceled_listener_signals { 0 };
    size_t canceled_observers { 0 };
    size_t canceled_timers { 0 };
    size_t blocked { 0 };
    size_t source_rejections { 0 };
    size_t source_job_errors { 0 };

    // One callback-gate context for every policy unit; restore it only at State teardown.
    explicit State(JS::VM&);

    ~State();

    // Principal callbacks run with source execution contexts saved/restored by the VM.
    bool has_source_context();

    JS::Completion in_principal(JS::Realm& realm, Function<JS::Completion()> operation);

    void report_exception(Web::WebIDL::CallbackType& callback, JS::Value exception);

    // Event values preserve same-world identity or bounded snapshots; revoke before release.
    EventValues::Activity event_activity(JS::Realm& realm);

    void event_changed(Web::DOM::Event& event, JS::Realm* writer);

    void visit_event_values(Web::DOM::Event& event, GC::Cell::Visitor& visitor);

    void revoke_event_values(JS::Realm& realm);

    JS::ThrowCompletionOr<JS::Value> read_event_value(
        JS::Realm& target, Web::DOM::Event& event, JS::Value value);

    // Private handler maps stay separate from principal EventTarget storage.
    Callbacks::HandlerMap* select_handler_map(Web::DOM::EventTarget& target);

    void visit_handler_maps(Web::DOM::EventTarget& target, GC::Cell::Visitor& visitor);

    void clear_handler_maps(JS::Realm* realm, Web::DOM::EventTarget* target);

    // Track exact listener/signal identities for targeted cancellation without public events.
    void release_signals(Web::DOM::DOMEventListener& listener);

    void change_listener(Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener);

    void add_listener_signal(
        Web::DOM::EventTarget& target, Web::DOM::DOMEventListener& listener, u64 id);

    void cancel_listeners(JS::Realm& realm);

    JS::Completion invoke_handler(
        Web::WebIDL::CallbackType& callback, Web::DOM::Event& event, bool special);

    JS::Completion invoke_listener(Web::WebIDL::CallbackType& callback, Web::DOM::Event& event);

    // Traversal provenance checks must not execute foreign getters/proxy traps.
    void filter_created(Web::DOM::NodeFilter& filter, JS::Realm* writer);

    JS::ThrowCompletionOr<JS::Value> read_filter(
        JS::Realm& reader, Web::DOM::NodeFilter* filter, JS::Value value);

    JS::ThrowCompletionOr<Web::DOM::NodeFilter::Result> invoke_filter(
        JS::Realm& caller, Web::DOM::NodeFilter& filter, Web::DOM::Node& node);

    // Detach cancels native observer/timer work before realm owners are released.
    void add_observer(Web::DOM::MutationObserver& observer);

    void remove_observer(Web::DOM::MutationObserver& observer);

    void cancel_observers(JS::Realm& realm);

    void cancel_timer(TimerRecord const& record);

    void cancel_timers(JS::Realm& realm);

    void add_timer(Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map,
        int id, Web::HTML::Timer& timer);

    void remove_timer(
        Web::HTML::WindowOrWorkerGlobalScopeMixin& global, Callbacks::TimerMap& map, int id);

    // Admit source work under a live owner; keep failures and raw reasons out of the page.
    JS::Completion invoke_webidl(Web::WebIDL::CallbackType& callback, Optional<JS::Value> receiver,
        Web::WebIDL::ExceptionBehavior behavior, ReadonlySpan<JS::Value> arguments);

    void rejection(JS::Promise& promise, JS::Promise::RejectionOperation operation);

    RefPtr<IsolatedWorld> owner_for(JS::Realm& realm);

    JS::ThrowCompletionOr<JS::Value> run_owned(RefPtr<IsolatedWorld> first,
        RefPtr<IsolatedWorld> second, Function<JS::ThrowCompletionOr<JS::Value>()> operation);

    bool blocked_realm(JS::Realm& realm);

    void enqueue(JS::PromiseJob job, GC::Ptr<JS::Realm> realm);

    JS::ThrowCompletionOr<JS::Value> invoke(
        JS::JobCallback& callback, JS::Value receiver, ReadonlySpan<JS::Value> arguments);
};
}
