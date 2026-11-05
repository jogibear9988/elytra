#include "Session.h"
#include "IsolatedWorld.h"
#include "BridgeData.h"
#include "FunctionBridge.h"
#include "WorldJobGuard.h"
#include <LibWeb/Bindings/MainThreadVM.h>
#include <AK/HashMap.h>
#include <AK/HashTable.h>
#include <AK/JsonObject.h>
#include <LibCore/System.h>
#include <LibCore/Timer.h>
#include <LibGC/Root.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/WebDriver/BiDi/BiDi.h>
#include <WebContent/PageClient.h>
#include <cstdio>
#include <cstdlib>

namespace WebContent {
void firefly_upstream_set_session_state(Web::WebDriver::BiDiSessionState);
}
namespace Firefly::RendererSession {
namespace {
struct WorldRecord {
    NonnullRefPtr<IsolatedWorld> owner;
    GC::Root<Web::DOM::Document> document;
    RefPtr<FunctionBridge> function {};
    void detach()
    {
        if (function) function->revoke();
        owner->detach();
    }
};
struct Session {
    HashMap<String, WorldRecord> worlds;
    OwnPtr<WorldJobGuard> jobs;
    RefPtr<Core::Timer> sweep_timer;
    HashTable<String> forward_events;
    size_t created { 0 };
    size_t destroyed { 0 };
    size_t reported_jobs { 0 };
    size_t reported_timers { 0 };
    size_t reported_observers { 0 };
    size_t reported_handler_maps { 0 };
    size_t reported_listeners { 0 };
    size_t reported_listener_signals { 0 };
    size_t reported_rejections { 0 };
    size_t reported_job_errors { 0 };
};
Session* session;

[[noreturn]] void fail(StringView reason)
{
    warnln("Firefly renderer world initialization failed: {}", reason);
    fflush(nullptr);
    std::_Exit(1);
}

void sweep_inactive()
{
    Vector<String> stale;
    for (auto const& entry : session->worlds) {
        if (!entry.value.document->is_fully_active()) stale.append(entry.key);
    }
    for (auto const& id : stale) {
        auto record = session->worlds.take(id);
        record->detach();
        ++session->destroyed;
        warnln("FIREFLY_WORLD_DESTROYED pid={} count={} reason=inactive", Core::System::getpid(), session->destroyed);
    }
}

void on_event(Web::HTML::Navigable const& navigable, String const& name, JsonValue params)
{
    if (!session) return;
    if (name == "script.realmCreated"sv || name == "script.realmDestroyed"sv) {
        if (!params.is_object()) fail("Missing realm event data"sv);
        auto id = params.as_object().get_string("realm"sv);
        if (!id.has_value() || id->is_empty() || id->bytes().size() > 1024) fail("Invalid realm identity"sv);
        if (name == "script.realmDestroyed"sv) {
            if (auto world = session->worlds.take(*id); world.has_value()) {
                world->detach();
                ++session->destroyed;
                warnln("FIREFLY_WORLD_DESTROYED pid={} count={}", Core::System::getpid(), session->destroyed);
            }
        } else if (!session->worlds.contains(*id)) {
            sweep_inactive();
            auto* local = as_if<Web::HTML::LocalNavigable>(const_cast<Web::HTML::Navigable&>(navigable));
            if (!local || !local->active_document()) fail("Realm event has no active local document"sv);
            if (session->worlds.size() >= 128) fail("Prototype world budget exceeded"sv);
            auto document = local->active_document();
            Web::HTML::TemporaryExecutionContext context(document->relevant_settings_object());
            auto owner = IsolatedWorld::create(*document->window());
            if (owner.is_error()) fail("Cannot create isolated world"sv);
            auto world = owner.release_value();
            if (session->jobs->protect(*world).is_error()) fail("Cannot guard world jobs"sv);
            session->worlds.set(*id, WorldRecord { world, GC::Root { *document } });
            // Compiled, harmless ordering/isolation probe only. No user preload,
            // Node binding, IPC function or general WebDriver command is exposed.
            auto result = world->evaluate(uR"JS(
                globalThis.__fireflyIsolatedMarker = 'renderer-only';
                Object.prototype.__fireflyWorldOnly = true;
                window.name = 'FIREFLY_WORLD_READY:' + location.href;
                globalThis.__fireflyCopySource = {answer:42,nested:[true]};
                __fireflyCopySource;
            )JS"sv);
            if (result.is_error()) fail("Isolated bootstrap script failed"sv);
            auto data = BridgeData::capture(result.value(), *world->realm(), {});
            if (data.is_error()) fail("Cannot capture bootstrap data"sv);
            auto copied = data.value().materialize(document->window()->principal_realm());
            if (copied.is_error()) fail("Cannot materialize bootstrap data"sv);
            document->window()->principal_realm().global_object().define_direct_property(
                "__fireflyDataProbe"_utf16, copied.value(), JS::default_attributes);
            // Prove the copy does not share state with its source.
            if (world->evaluate(u"__fireflyCopySource.answer=99"sv).is_error()) fail("Cannot mutate probe source"sv);
            auto weak_world = world->make_weak_ptr();
            GC::Root detach_probe { JS::NativeFunction::create(*world->realm(),
                [weak_world](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                    if (auto owner = weak_world.strong_ref()) owner->detach();
                    return JS::js_undefined();
                }, 0, "__fireflyDetachWorldProbe"_utf16, GC::Ptr<JS::Realm> { *world->realm() }) };
            world->realm()->global_object().define_direct_property("__fireflyDetachWorldProbe"_utf16, detach_probe.ptr(), JS::default_attributes);
            auto callback = world->evaluate(uR"JS(
                (function(a) {
                    'use strict';
                    if(a.mode==='message-events') {
                        globalThis.__sourceMessageSecret=true;
                        window.addEventListener('message',event=>{
                            let value;
                            try{value=event.data}catch(error){document.documentElement.setAttribute('data-message-input-denied',String(error instanceof TypeError));return}
                            if(value.kind==='page-input')document.documentElement.setAttribute('data-message-input',String(value===event.data && value.n===8 && Object.isFrozen(value) && Object.getPrototypeOf(value)===Object.prototype));
                            if(value.kind==='page-post'){
                                const valid=value===event.data && value.n===9 && Object.isFrozen(value) && Object.getPrototypeOf(value)===Object.prototype;
                                document.documentElement.setAttribute('data-message-post-input',String(valid));
                                window.postMessage({kind:'source-reply',valid},'*');
                            }
                            if(value.kind==='published-after-detach')document.documentElement.setAttribute('data-message-after-detach','BAD');
                        });
                        const value={kind:'source-input',n:42,items:[1,,3]};value.self=value;
                        const event=new MessageEvent('message',{data:value,origin:'spoofed',source:window});
                        window.dispatchEvent(event);value.n=99;window.dispatchEvent(event);
                        window.dispatchEvent(new MessageEvent('message',{data:()=>42}));
                        window.postMessage({kind:'source-post',n:7},'*');
                        let denied=false;try{window.postMessage(()=>42,'*')}catch(error){denied=error.name==='DataCloneError'}
                        document.documentElement.setAttribute('data-message-serialize-denied',String(denied));
                        return event.data===value;
                    }
                    if(a.mode==='published-message') {
                        window.postMessage({kind:'published-after-detach',n:12},'*');
                        __fireflyDetachWorldProbe();return true;
                    }
                    if(a.mode==='error-events') {
                        const reason={n:42};reason.self=reason;
                        let pageInput;
                        window.addEventListener('error',event=>{
                            if(event.message==='page-data')pageInput=event.error;
                            if(event.message==='page-bad'){
                                let denied=false;try{event.error}catch(error){denied=error instanceof TypeError}
                                document.documentElement.setAttribute('data-error-input-denied',String(denied));
                            }
                        });
                        window.onerror=function(message,url,line,column,error){
                            if(message==='page-data')document.documentElement.setAttribute('data-error-input',String(error===pageInput && Object.isFrozen(error) && Object.getPrototypeOf(error)===Object.prototype && error.n===8));
                            if(message==='source-data')document.documentElement.setAttribute('data-error-own',String(error===reason));
                            return true;
                        };
                        window.dispatchEvent(new Event('native-page-control'));
                        const event=new ErrorEvent('error',{message:'source-data',filename:'unit:source',lineno:7,colno:9,error:reason,cancelable:true});
                        window.dispatchEvent(event);reason.n=99;window.dispatchEvent(event);
                        window.dispatchEvent(new ErrorEvent('error',{message:'source-function',error:()=>42,cancelable:true}));
                        return event.error===reason;
                    }
                    if(a.mode==='custom-events') {
                        document.addEventListener('page-detail',event=>{
                            const value=event.detail;
                            const valid=value.n===8 && value===event.detail && Object.isFrozen(value) &&
                                Object.getPrototypeOf(value)===Object.prototype;
                            document.documentElement.setAttribute('data-custom-reverse',String(valid));
                        });
                        document.addEventListener('page-detail-bad',event=>{
                            let denied=false;
                            try{event.detail}catch(error){denied=error instanceof TypeError}
                            document.documentElement.setAttribute('data-custom-reverse-denied',String(denied));
                        });
                        const data={n:42,items:[1,,3]};data.self=data;data.alias=data.items;
                        const event=new CustomEvent('source-detail',{detail:data});
                        document.dispatchEvent(event);
                        data.n=99;
                        document.dispatchEvent(event);
                        document.dispatchEvent(new CustomEvent('source-detail-bad',{detail:()=>42}));
                        return event.detail===data;
                    }
                    if(a.mode==='start-handlers') {
                        if(document.onclick!==null || document.onkeyup!==null)throw Error('Page handler reference leaked');
                        let runs=0;
                        const handler=function(event){
                            const valid=this===document && event.target===document && Object.getPrototypeOf(event)===Event.prototype;
                            document.documentElement.setAttribute('data-source-handler',String(++runs)+':'+valid);
                            return false;
                        };
                        document.onclick=handler;
                        if(document.onclick!==handler || Object.getPrototypeOf(document.onclick)!==Function.prototype)throw Error('Source handler identity changed');
                        document.onkeydown=()=>{throw {privateCallback(){return 42}}};
                        document.onmousedown=()=>new Proxy({},{get(){document.documentElement.setAttribute('data-handler-trap','BAD');throw 1}});
                        window.onbeforeunload=()=>{document.documentElement.setAttribute('data-handler-late','BAD');return 'BAD'};
                        return true;
                    }
                    if(a.mode==='remove-self-handler') {
                        document.onclick=()=>{
                            __fireflyDetachWorldProbe();
                            window.frameElement.remove();
                            document.documentElement.setAttribute('data-after-remove','safe');
                            queueMicrotask(()=>document.documentElement.setAttribute('data-next-job','BAD'));
                            return false;
                        };
                        return true;
                    }
                    if(a.mode==='start-listeners') {
                        let count=0,gets=0;
                        const controller=new AbortController();
                        const listener=function(event){
                            const valid=this===document && event.target===document &&
                                Object.getPrototypeOf(event)===Event.prototype && !event.sourceEventPoison;
                            document.documentElement.setAttribute('data-source-listener',String(++count)+':'+valid);
                            event.preventDefault();
                        };
                        document.addEventListener('firefly-listener',listener,{signal:controller.signal});
                        document.addEventListener('firefly-listener',listener,{signal:controller.signal});
                        const object={get handleEvent(){
                            document.documentElement.setAttribute('data-source-listener-gets',String(++gets));
                            return function(event){document.documentElement.setAttribute('data-source-listener-object',String(this===object && event.currentTarget===document));};
                        }};
                        document.addEventListener('firefly-listener',object);
                        document.addEventListener('firefly-listener',()=>{throw {privateCallback(){return 42}}});
                        document.addEventListener('firefly-listener',{get handleEvent(){throw new Proxy({},{get(){throw 1}})}});
                        return true;
                    }
                    if(a.mode==='remove-self-listener') {
                        const finish=()=>{
                            __fireflyDetachWorldProbe();
                            window.frameElement.remove();
                            document.documentElement.setAttribute('data-after-remove','safe');
                            queueMicrotask(()=>document.documentElement.setAttribute('data-next-job','BAD'));
                        };
                        const callback=a.kind==='getter'?{get handleEvent(){finish();return ()=>{}}}:
                            a.kind==='proxy'?new Proxy({},{get(){return finish}}):finish;
                        document.addEventListener('firefly-inflight',callback);
                        return true;
                    }
                    if(a.mode==='start-observer') {
                        let count=0;
                        const observer=new MutationObserver(function(records,same) {
                            const valid=same===observer && this===observer &&
                                Object.getPrototypeOf(records)===Array.prototype && records.observerTrap===undefined &&
                                Object.getPrototypeOf(same)===MutationObserver.prototype && !same.observerPoison &&
                                records.every(record=>Object.getPrototypeOf(record)===MutationRecord.prototype &&
                                    !record.observerPoison && record.target===document.documentElement);
                            document.documentElement.setAttribute('data-source-observer',String(++count)+':'+valid);
                        });
                        observer.observe(document.documentElement,{attributes:true,attributeFilter:['data-observer-input']});
                        return true;
                    }
                    if(a.mode==='start-timers') {
                        globalThis.__sourceTimerTicks=0;
                        setInterval(()=>document.documentElement.setAttribute('data-source-interval',String(++__sourceTimerTicks)),0);
                        setTimeout(()=>document.documentElement.setAttribute('data-source-timeout','BAD'),60000);
                        return true;
                    }
                    if(a.mode==='detach-world') {__fireflyDetachWorldProbe();return true;}
                    if(a.mode==='source-errors') {
                        const secret={privateCallback(){return 42}};
                        globalThis.__sourceRejected=Promise.reject(secret);
                        Promise.resolve().then(()=>{throw secret});
                        (async()=>{await 0;throw secret})();
                        queueMicrotask(()=>{throw secret});
                        Promise.reject(new Proxy(secret,{get(){throw new Error('source getter must not run')}}));
                        return true;
                    }
                    if(a.mode==='handle-source-errors') {
                        __sourceRejected.catch(()=>{});
                        return true;
                    }
                    if(a.mode==='remove-self-job') {
                        const finish=()=>{
                            window.frameElement.remove();
                            document.documentElement.setAttribute('data-after-remove','safe');
                            Promise.resolve().then(()=>document.documentElement.setAttribute('data-next-job','BAD'));
                            return 42;
                        };
                        if(a.kind==='microtask')return new Promise(resolve=>queueMicrotask(()=>resolve(finish())));
                        if(a.kind==='await')return(async()=>{await 0;return finish()})();
                        if(a.kind==='generator')return(async function*(){await 0;return finish()})().next();
                        return Promise.resolve().then(finish);
                    }
                    if(a.mode==='queue-jobs') {
                        const bad=()=>document.documentElement.setAttribute('data-source-job','BAD');
                        queueMicrotask(bad);
                        queueMicrotask(bad.bind(null));
                        queueMicrotask(new Proxy(bad,{}));
                        Promise.resolve().then(bad);
                        Promise.resolve().then(bad.bind(null));
                        Promise.resolve().then(new Proxy(bad,{}));
                        Promise.resolve({then(resolve){bad();resolve(1)}});
                        (async()=>{await 0;bad()})();
                        (async function*(){await 0;bad();yield 1})().next();
                        return true;
                    }
                    if(a.mode==='pending')return new Promise(resolve=>{
                        (globalThis.__fireflyResolvers??=[]).push(resolve);
                    });
                    if(a.mode==='promise')return Promise.resolve({answer:a.n+1,nested:[true]});
                    if(a.mode==='reject')return Promise.reject('private source rejection');
                    return {answer:a.n+1, ownThis:this===undefined,
                        ownPrototype:Object.getPrototypeOf(a)===Object.prototype,
                        frozen:Object.isFrozen(a)};
                })
            )JS"sv);
            if (callback.is_error() || !callback.value().is_function()) fail("Cannot create function probe"sv);
            auto endpoint = FunctionBridge::create(*world, callback.release_value().as_function(), *document);
            if (endpoint.is_error()) fail("Cannot export function probe"sv);
            session->worlds.find(*id)->value.function = endpoint.value().owner;
            document->window()->principal_realm().global_object().define_direct_property(
                "__fireflyFunctionProbe"_utf16, endpoint.value().function, JS::default_attributes);
            ++session->created;
            warnln("FIREFLY_WORLD_CREATED pid={} count={}", Core::System::getpid(), session->created);
        }
    }
    if (session->forward_events.contains(name)) {
        auto* client = as_if<WebContent::PageClient>(const_cast<Web::HTML::Navigable&>(navigable).page().client());
        if (client) client->webdriver_bidi_event(navigable.id(), name, move(params));
    }
}
}
void initialize()
{
    if (session) fail("Session initialized twice"sv);
    session = new Session;
    session->jobs = make<WorldJobGuard>(Web::Bindings::main_thread_vm());
    apply({});
    session->sweep_timer = Core::Timer::create_repeating(AK::Duration::from_milliseconds(10), [] {
        if (session) {
            sweep_inactive();
            auto blocked = session->jobs->blocked_count();
            if (blocked != session->reported_jobs) {
                session->reported_jobs = blocked;
                warnln("FIREFLY_WORLD_JOBS_BLOCKED pid={} count={}", Core::System::getpid(), blocked);
            }
            auto timers = session->jobs->canceled_timer_count();
            if (timers != session->reported_timers) {
                session->reported_timers = timers;
                warnln("FIREFLY_WORLD_TIMERS_CANCELED pid={} count={}", Core::System::getpid(), timers);
            }
            auto observers = session->jobs->canceled_observer_count();
            if (observers != session->reported_observers) {
                session->reported_observers = observers;
                warnln("FIREFLY_WORLD_OBSERVERS_CANCELED pid={} count={}", Core::System::getpid(), observers);
            }
            auto listeners = session->jobs->canceled_listener_count();
            auto signals = session->jobs->canceled_listener_signal_count();
            if (listeners != session->reported_listeners || signals != session->reported_listener_signals) {
                session->reported_listeners = listeners;
                session->reported_listener_signals = signals;
                warnln("FIREFLY_WORLD_LISTENERS_CANCELED pid={} count={} signals={}", Core::System::getpid(), listeners, signals);
            }
            auto maps = session->jobs->canceled_handler_map_count();
            if (maps != session->reported_handler_maps) {
                session->reported_handler_maps = maps;
                warnln("FIREFLY_WORLD_HANDLER_MAPS_CLEARED pid={} count={}", Core::System::getpid(), maps);
            }
            auto rejections = session->jobs->source_rejection_count();
            auto errors = session->jobs->source_job_error_count();
            if (rejections != session->reported_rejections || errors != session->reported_job_errors) {
                session->reported_rejections = rejections;
                session->reported_job_errors = errors;
                warnln("FIREFLY_WORLD_FAILURES_REDACTED pid={} rejections={} job_errors={}", Core::System::getpid(), rejections, errors);
            }
        }
    });
    session->sweep_timer->start();
}
void apply(Web::WebDriver::BiDiSessionState state)
{
    if (!session) fail("Session state arrived before VM initialization"sv);
    session->forward_events.clear();
    if (state.is_active) {
        for (auto const& name : state.enabled_events) session->forward_events.set(name);
    }
    state.is_active = true;
    for (auto const& name : { "script.realmCreated"_string, "script.realmDestroyed"_string }) {
        if (!state.enabled_events.contains_slow(name)) state.enabled_events.append(name);
        state.enabled_event_contexts.set(name, OptionalNone {});
    }
    WebContent::firefly_upstream_set_session_state(move(state));
    Web::WebDriver::BiDi::set_event_sink({
        .on_event = on_event,
        .on_navigation_event = [](auto const& navigable, StringView name, auto const& navigation_id, auto const& url) {
            if (!session || !session->forward_events.contains(name)) return;
            auto* client = as_if<WebContent::PageClient>(const_cast<Web::HTML::Navigable&>(navigable).page().client());
            if (client) client->webdriver_bidi_navigation_event(navigable.id(), name, navigation_id, url);
        },
    });
}
void shutdown()
{
    Web::WebDriver::BiDi::set_event_sink({});
    if (!session) return;
    session->sweep_timer->stop();
    session->sweep_timer.clear();
    for (auto& entry : session->worlds) entry.value.detach();
    delete session;
    session = nullptr;
}
}
