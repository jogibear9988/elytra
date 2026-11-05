#include "Session.h"
#include "IsolatedWorld.h"
#include "BridgeData.h"
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
};
struct Session {
    HashMap<String, WorldRecord> worlds;
    RefPtr<Core::Timer> sweep_timer;
    HashTable<String> forward_events;
    size_t created { 0 };
    size_t destroyed { 0 };
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
        record->owner->detach();
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
                world->owner->detach();
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
    apply({});
    session->sweep_timer = Core::Timer::create_repeating(AK::Duration::from_milliseconds(10), [] {
        if (session) sweep_inactive();
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
    for (auto& entry : session->worlds) entry.value.owner->detach();
    delete session;
    session = nullptr;
}
}
