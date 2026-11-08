/**
 * Production preload service for bounded top-level application documents. Own
 * realm/publication state, fail closed on invalid configuration and revoke inactive
 * documents.
 */

#include "RendererSession.h"
#include "context/ContextBridge.h"
#include "ipc/IpcEndpoint.h"
#include "loading/FileModules.h"
#include "worlds/WorldJobGuard.h"
#include <AK/HashMap.h>
#include <AK/JsonObject.h>
#include <AK/Utf16StringBuilder.h>
#include <LibCore/Timer.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/WebDriver/BiDi/BiDi.h>
#include <cstdio>
#include <cstdlib>

namespace WebContent {
void firefly_upstream_set_session_state(Web::WebDriver::BiDiSessionState);
}

namespace Firefly::RendererSession {
namespace {
    struct Preload {
        String source;
        String filename;
        String dirname;
        bool electron_compat { false };
    };

    struct World {
        NonnullRefPtr<IsolatedWorld> owner;
        NonnullRefPtr<ContextBridge> bridge;
        GC::Root<Web::DOM::Document> document;
        NonnullRefPtr<IpcEndpoint> ipc;

        void detach()
        {
            ipc->revoke();
            bridge->revoke();
            owner->detach();
        }
    };

    struct Session {
        HashMap<String, Preload> sources;
        HashMap<String, World> worlds;
        OwnPtr<WorldJobGuard> jobs;
        RefPtr<Core::Timer> sweep;
    };

    Session* session;

    [[noreturn]] void fail(StringView reason)
    {
        warnln("Firefly preload failed: {}", reason);
        fflush(nullptr);
        std::_Exit(1);
    }

    void sweep()
    {
        Vector<String> stale;
        for (auto const& entry : session->worlds) {
            if (!entry.value.document->is_fully_active())
                stale.append(entry.key);
            else
                entry.value.ipc->expire();
        }

        for (auto const& id : stale) {
            auto world = session->worlds.take(id);
            world->detach();
        }
    }

    void run_preload(
        IsolatedWorld& world, ContextBridge& bridge, IpcEndpoint& ipc, Preload const& preload)
    {
        auto& realm = *world.realm();
        auto& vm = realm.vm();
        GC::Root imports { JS::Object::create(realm, nullptr) };
        imports->define_direct_property("contextBridge"_utf16, bridge.exports(), {});
        imports->define_direct_property("ipcRenderer"_utf16, ipc.exports(), {});
        VERIFY(MUST(imports->set_integrity_level(JS::Object::IntegrityLevel::Frozen)));
        GC::Root require { JS::NativeFunction::create(
            realm,
            [weak = world.make_weak_ptr(), module = GC::Ref { *imports },
                electron_compat = preload.electron_compat](
                JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto owner = weak.strong_ref();
                if (!owner || owner->is_detached() || !owner->is_executing())
                    return vm.throw_completion<JS::TypeError>("Preload require is revoked"sv);
                if (vm.argument(0).is_string()
                    && (vm.argument(0).as_string().utf16_string_view() == "firefly"_utf16
                        || (electron_compat
                            && vm.argument(0).as_string().utf16_string_view() == "electron"_utf16)))
                    return module;
                auto error = JS::Error::create(
                    vm.realm(), "Module is not available in this sandboxed preload"_utf16);
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, "ERR_FIREFLY_UNSUPPORTED_API"_utf16),
                    JS::default_attributes);
                return JS::throw_completion(error);
            },
            1, "require"_utf16, GC::Ptr<JS::Realm> { realm }) };
        GC::Root module { JS::Object::create(realm, realm.intrinsics().object_prototype()) };
        module->define_direct_property("exports"_utf16,
            JS::Object::create(realm, realm.intrinsics().object_prototype()),
            JS::default_attributes);
        GC::Root bootstrap { JS::Object::create(realm, nullptr) };
        bootstrap->define_direct_property("require"_utf16, require, JS::default_attributes);
        bootstrap->define_direct_property("module"_utf16, module, JS::default_attributes);
        bootstrap->define_direct_property("filename"_utf16,
            JS::PrimitiveString::create(
                vm, Utf16String::from_utf8_with_replacement_character(preload.filename)),
            JS::default_attributes);
        bootstrap->define_direct_property("dirname"_utf16,
            JS::PrimitiveString::create(
                vm, Utf16String::from_utf8_with_replacement_character(preload.dirname)),
            JS::default_attributes);
        Utf16StringBuilder text;
        text.append(
            u"(function(b){return(function(require,module,exports,__filename,__dirname){\n"sv);
        text.append(Utf16String::from_utf8_with_replacement_character(preload.source));
        text.append(
            u"\n}).call(b.module.exports,b.require,b.module,b.module.exports,b.filename,b.dirname);})"sv);
        auto factory = world.evaluate(text.view());
        if (factory.is_error() || !factory.value().is_function())
            fail("Invalid CommonJS preload source"sv);
        auto result = factory.release_value();
        GC::Root function { result.as_function() };
        JS::Value argument { bootstrap.ptr() };
        if (world.call(*function, ReadonlySpan<JS::Value> { &argument, 1 }).is_error())
            fail("CommonJS preload evaluation rejected"sv);
    }

    void on_event(Web::HTML::Navigable const& navigable, String const& name, JsonValue params)
    {
        if (!session || (name != "script.realmCreated"sv && name != "script.realmDestroyed"sv))
            return;
        if (!params.is_object())
            fail("Invalid realm notification"sv);
        auto id = params.as_object().get_string("realm"sv);
        if (!id.has_value() || id->is_empty() || id->bytes().size() > 1024)
            fail("Invalid realm identity"sv);
        if (name == "script.realmDestroyed"sv) {
            if (auto world = session->worlds.take(*id); world.has_value())
                world->detach();
            return;
        }

        if (!navigable.is_top_level_traversable())
            return;
        auto* local
            = as_if<Web::HTML::LocalNavigable>(const_cast<Web::HTML::Navigable&>(navigable));
        if (!local || !local->active_document())
            return;
        auto document = local->active_document();
        auto const& url = document->url();
        if (url.scheme() != "file"sv && url.scheme() != "http"sv && url.scheme() != "https"sv)
            return;
        auto handle = Web::WebDriver::BiDi::navigable_id(navigable);
        auto source = session->sources.find(handle);
        if (source == session->sources.end())
            return;
        auto preload = source->value; // Keep native configuration alive across reentrant JS.
        sweep();
        if (session->worlds.contains(*id))
            return;
        if (session->worlds.size() >= 32)
            fail("Active preload world budget exceeded"sv);
        Web::HTML::TemporaryExecutionContext context(document->relevant_settings_object());
        auto owner = IsolatedWorld::create(*document->window());
        if (owner.is_error())
            fail("Cannot create preload world"sv);
        auto world = owner.release_value();
        if (session->jobs->protect(*world).is_error())
            fail("Cannot protect preload jobs"sv);
        auto bridge = ContextBridge::create(*world, *document);
        if (bridge.is_error())
            fail("Cannot create context bridge"sv);
        auto ipc = IpcEndpoint::create(*world, *document, *id);
        session->worlds.set(*id, World { world, bridge.value(), GC::Root { *document }, ipc });
        run_preload(*world, *bridge.value(), *ipc, preload);
    }
}

void initialize()
{
    if (session)
        fail("Duplicate preload service"sv);
    FileModules::set_enabled(true);
    session = new Session;
    session->jobs = make<WorldJobGuard>(Web::Bindings::main_thread_vm());
    apply({});
    session->sweep = Core::Timer::create_repeating(AK::Duration::from_milliseconds(10), [] {
        if (session)
            sweep();
    });
    session->sweep->start();
}

void apply(Web::WebDriver::BiDiSessionState state)
{
    if (!session)
        fail("Preload configuration before VM initialization"sv);
    if (!state.enabled_events.is_empty() || !state.enabled_event_contexts.is_empty()
        || !state.network_intercepts.is_empty() || !state.network_default_headers.is_empty()
        || !state.network_user_context_headers.is_empty()
        || !state.network_navigable_headers.is_empty()
        || state.network_default_cache_behavior != "default"sv
        || !state.network_navigable_cache_behavior.is_empty() || state.preload_scripts.size() > 16)
        fail("Unsupported preload service configuration"sv);
    HashMap<String, Preload> sources;
    size_t total = 0;
    for (auto const& script : state.preload_scripts) {
        if (script.id != "firefly.preload.v1"sv || !script.contexts.has_value()
            || script.contexts->size() != 1 || script.function_declaration.bytes().size() > 65536
            || !script.arguments.is_object() || script.sandbox.has_value()
            || !script.user_contexts.is_empty())
            fail("Invalid preload configuration"sv);
        auto const& handle = script.contexts->first();
        auto filename = script.arguments.as_object().get_string("filename"sv),
             dirname = script.arguments.as_object().get_string("dirname"sv);
        auto electron_compat = script.arguments.as_object().get_bool("electronCompat"sv);
        if (handle.is_empty() || handle.bytes().size() > 1024 || sources.contains(handle)
            || !filename.has_value() || !dirname.has_value() || filename->contains('\0')
            || dirname->contains('\0') || filename->bytes().size() > 4096
            || dirname->bytes().size() > 4096
            || (script.arguments.as_object().size() != 2
                && (script.arguments.as_object().size() != 3 || !electron_compat.has_value())))
            fail("Invalid preload metadata"sv);
        total += script.function_declaration.bytes().size();
        if (total > 1024 * 1024)
            fail("Preload source budget exceeded"sv);
        sources.set(handle,
            Preload { script.function_declaration, *filename, *dirname,
                electron_compat.value_or(false) });
    }

    session->sources = move(sources);
    state.preload_scripts
        .clear(); // Never run application source as a principal-world WebDriver preload.
    state.is_active = true;
    for (auto name : { "script.realmCreated"_string, "script.realmDestroyed"_string }) {
        state.enabled_events.append(name);
        state.enabled_event_contexts.set(name, OptionalNone {});
    }

    WebContent::firefly_upstream_set_session_state(move(state));
    Web::WebDriver::BiDi::set_event_sink({ .on_event = on_event, .on_navigation_event = {} });
}

void shutdown()
{
    FileModules::set_enabled(false);
    Web::WebDriver::BiDi::set_event_sink({});
    if (!session)
        return;
    session->sweep->stop();
    for (auto& entry : session->worlds)
        entry.value.detach();
    delete session;
    session = nullptr;
}
}

namespace Firefly::RendererSession {
bool receive_ipc_reply(WebContent::PageClient& page, u64, Web::HTML::CrossProcessId navigable,
    String const& name, JsonValue const& params)
{
    if (!name.starts_with_bytes("firefly.ipc."sv))
        return false;
    if (!name.is_one_of("firefly.ipc.reply.v1"sv, "firefly.ipc.event.v1"sv) || !session
        || !params.is_object())
        return true;
    auto realm = params.as_object().get_string("realm"sv);
    if (!realm.has_value() || realm->bytes().size() > 1024)
        return true;
    auto found = session->worlds.find(*realm);
    if (found != session->worlds.end())
        found->value.ipc->receive(page, navigable, params, name == "firefly.ipc.event.v1"sv);
    return true;
}
}
