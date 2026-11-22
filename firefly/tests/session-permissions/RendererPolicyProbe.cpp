/** Actual LibWeb permission queries/requests, no UI, device or OS authorization. */
#include "permissions/Hooks.h"
#include <AK/ScopeGuard.h>
#include <LibCore/EventLoop.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Script.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/Platform/FontPlugin.h>
#include <chrono>

namespace H = Web::HTML;
namespace P = Firefly::RendererPermissions;

namespace {
bool allow_query(Web::DOM::Document&, u64, StringView)
{
    return true;
}

ErrorOr<void> require(bool condition, StringView label)
{
    if (!condition) {
        warnln("FAIL renderer permission policy: {}", label);
        return Error::from_string_literal("Renderer permission policy assertion failed");
    }

    return {};
}

ErrorOr<JS::Value> run(JS::Realm& realm, Utf16View source)
{
    H::TemporaryExecutionContext execution { realm,
        H::TemporaryExecutionContext::CallbacksEnabled::Yes };
    auto parsed = JS::Script::parse(Utf16String::from_utf16(source), realm);
    if (parsed.is_error())
        return Error::from_string_literal("Invalid permission component script");
    GC::Root script { parsed.release_value() };
    auto result = realm.vm().run(*script);
    if (result.is_error())
        return Error::from_string_literal("Permission component script threw");
    return result.release_value();
}

ErrorOr<void> denied(JS::Promise& promise, Core::EventLoop& loop)
{
    // The headless realm factory leaves a testing context current. Suspend
    // it while the HTML event loop runs its normal microtask checkpoints.
    auto& vm = Web::Bindings::main_thread_vm();
    vm.save_execution_context_stack();
    vm.clear_execution_context_stack();
    ScopeGuard restore_context([&] { vm.restore_execution_context_stack(); });

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (promise.state() == JS::Promise::State::Pending
        && std::chrono::steady_clock::now() < deadline) {
        H::main_thread_event_loop().process();
        loop.pump(Core::EventLoop::WaitMode::PollForEvents);
    }

    TRY(require(promise.state() == JS::Promise::State::Fulfilled, "request settles"sv));
    TRY(require(promise.result().is_string()
            && promise.result().as_string().utf16_string() == "denied"_utf16,
        "independent request decision is denied"sv));
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    auto& loop = Core::EventLoop::initialize_for_current_thread();
    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    GC::Root initial { Web::Bindings::create_a_principal_javascript_realm() };
    auto& page = Web::Bindings::principal_host_defined_page(*initial);
    auto traversable
        = H::LocalTraversableNavigable::create_a_new_top_level_traversable(page, nullptr, {});
    page.set_top_level_traversable(traversable);
    GC::Root document { *traversable->active_document() };
    auto url = URL::Parser::basic_parse("https://permission-component.example/"sv).release_value();
    document->set_url(url);
    document->set_origin(url.origin());
    auto& settings = document->relevant_settings_object();
    settings.creation_url = url;
    settings.top_level_creation_url = url;
    settings.top_level_origin = url.origin();
    auto& realm = document->window()->principal_realm();

    TRY(require(firefly_renderer_notification_request_state(&settings) == 255,
        "unowned notification requests select the upstream algorithm"sv));
    TRY(require(!firefly_renderer_permissions_active(), "unowned session remains inactive"sv));
    P::enable();
    P::set_query(allow_query);
    ScopeGuard cleanup([] { P::set_query(nullptr); });
    TRY(require(P::apply_policy(1, true, false), "install query-only policy"sv));
    auto query = TRY(run(realm, u"Notification.permission === 'granted'"sv));
    TRY(require(query.is_boolean() && query.as_bool(), "query reports the application grant"sv));
    auto value = TRY(run(realm, u"Notification.requestPermission()"sv));
    TRY(require(
        value.is_object() && is<JS::Promise>(value.as_object()), "real notification promise"sv));
    GC::Root no_handler { static_cast<JS::Promise&>(value.as_object()) };
    TRY(denied(*no_handler, loop));
    outln("PASS query grant without request handler cannot grant a request");

    TRY(require(P::apply_policy(2, true, true), "install independent request policy"sv));
    value = TRY(run(realm, u"Notification.requestPermission()"sv));
    GC::Root no_activation { static_cast<JS::Promise&>(value.as_object()) };
    TRY(denied(*no_activation, loop));
    TRY(require(!document->window()->has_transient_activation(), "no fabricated activation"sv));
    outln("PASS query grant cannot bypass native notification activation");

    // Drive the real native Window activation algorithm, not a scripted event.
    // Complete Page's actual first pending request through its normal UI answer
    // entry point; no browser/device provider or cross-process claim is made.
    document->window()->notify_about_user_activation();
    value = TRY(run(realm, u"Notification.requestPermission()"sv));
    GC::Root requested { static_cast<JS::Promise&>(value.as_object()) };
    auto& promise = *requested;
    TRY(require(promise.state() == JS::Promise::State::Pending,
        "activated request waits for its own answer"sv));
    TRY(require(
        !document->window()->has_transient_activation(), "request consumes native activation"sv));
    page.permission_request_completed(0, Web::Permissions::EffectiveState::Denied);
    TRY(denied(promise, loop));
    query = TRY(run(realm, u"Notification.permission === 'granted'"sv));
    TRY(require(
        query.is_boolean() && query.as_bool(), "request denial does not overwrite query policy"sv));
    outln("PASS actual request denial stays independent from a query grant");
    return 0;
}
