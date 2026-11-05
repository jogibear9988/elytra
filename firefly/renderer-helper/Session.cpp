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
            auto callback = world->evaluate(uR"JS(
                (function(a) {
                    'use strict';
                    if(a.mode==='queue-jobs') {
                        const bad=()=>document.documentElement.setAttribute('data-source-job','BAD');
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
