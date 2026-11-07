/** Real upstream navigation population/fetch over native IPC; no document origin rewriting. */
#include "../ipc/SpawnBytePeer.h"
#include "Arguments.h"
#include "loading/protocol/Client.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/files/Server.h"
#include <AK/Array.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibIPC/StartupTransport.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/NavigationSourceSnapshot.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/SourceSnapshotParams.h>
#include <LibWeb/Platform/FontPlugin.h>
#ifdef AK_OS_WINDOWS
#include <LibIPC/HandleBroker.h>
#endif
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace PF = Firefly::ProtocolFetch;
namespace P = Firefly::Protocols;
namespace H = Web::HTML;

namespace {
constexpr auto prefix = "<!doctype html><title>NAVIGATION_OK</title><p>native-navigation</p>\n"sv;
constexpr size_t body_length = 256 * 1024 + 19;

struct State final : RefCounted<State> {
    Optional<H::NavigationPopulationResult> result;
    WeakPtr<PF::Response> response;
    size_t handlers { 0 };
};

ErrorOr<void> wait(JS::Realm& realm, Function<bool()> predicate)
{
    auto timer = Core::Timer::create_single_shot(AK::Duration::from_seconds(10), [] { });
    timer->start();
    H::main_thread_event_loop().spin_until(
        GC::create_function(realm.heap(), [&] { return predicate() || !timer->is_active(); }));
    if (!predicate())
        return Error::from_string_literal("Native navigation test did not settle");
    realm.vm().heap().collect_garbage();
    return {};
}

ErrorOr<void> validate(H::NavigationPopulationResult const& result, H::LocalNavigable& navigable,
    URL::URL const& url, bool success, size_t length = body_length)
{
    if (result.navigation_params.has<H::NonFetchSchemeNavigationParamsDescriptor>())
        return Error::from_string_literal("Custom navigation bypassed actual fetch");
    auto const* descriptor = result.navigation_params.get_pointer<H::NavigationParamsDescriptor>();
    bool actual_success = descriptor && !descriptor->response.network_error_message.has_value();
    if (actual_success != success)
        return Error::from_string_literal("Navigation success/refusal did not match native intent");
    if (!success)
        return {};
    auto const* bytes = descriptor->response.body.get_pointer<ByteBuffer>();
    if (!bytes || bytes->size() != length || descriptor->response.status != 200
        || descriptor->origin.serialize() != "app://bundle"sv
        || descriptor->navigable_id != navigable.id() || !descriptor->request.has_value()
        || !descriptor->request->client_is_null || !descriptor->reserved_environment.has_value()
        || !descriptor->reserved_environment->creation_url.equals(url, URL::ExcludeFragment::Yes)
        || descriptor->policy_container.csp_list.size() != 1
        || !descriptor->policy_container.csp_list[0].self_origin.is_same_origin(descriptor->origin)
        || descriptor->policy_container.csp_list[0].pre_parsed_policy_string
            != "default-src 'none'"sv)
        return Error::from_string_literal("Actual navigation body/origin/policy/identity mismatch");
    for (size_t i = 0; i < bytes->size(); ++i)
        if ((*bytes)[i]
            != (i < prefix.length() ? static_cast<u8>(prefix[i]) : static_cast<u8>('x')))
            return Error::from_string_literal("Actual navigation body bytes changed");
    return {};
}

ErrorOr<int> child(StringView bootstrap, StringView mode)
{
    std::mutex lock;
    std::condition_variable wake;
    bool finished = false;
    std::thread watchdog([&] {
        std::unique_lock guard(lock);
        if (!wake.wait_for(guard, std::chrono::seconds(40), [&] { return finished; }))
            Core::Process::terminate_immediately(1);
    });
    ScopeGuard joined = [&] {
        {
            std::lock_guard guard(lock);
            finished = true;
        }

        wake.notify_one();
        watchdog.join();
    };

    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    Array schemes { P::OriginScheme { "app"_string, true, true } };
    TRY(P::install_origin_schemes(schemes));
    GC::Root initial { Web::Bindings::create_a_principal_javascript_realm() };
    auto& page = Web::Bindings::principal_host_defined_page(*initial);
    GC::Root navigable { H::LocalTraversableNavigable::create_a_new_top_level_traversable(
        page, nullptr, {}) };
    page.set_top_level_traversable(*navigable);
    GC::Root source { *navigable->active_document() };
    auto& realm = source->relevant_settings_object().realm();
    auto url = URL::Parser::basic_parse("app://bundle/index.html"sv).release_value();
    auto expected = URL::Parser::basic_parse(
        mode == "url"sv ? "app://other/index.html"sv : "app://bundle/index.html"sv)
                        .release_value();
    auto id = Utf16String::from_utf8("native-navigation-1"sv);
    navigable->adopt_navigation_started_in_ui_process(id);
    auto client = TRY(PF::Client::create_navigation(
        TRY(IPC::take_over_startup_transport(bootstrap)), 101, *navigable, expected, id));
    auto state = adopt_ref(*new State);
    auto weak = client->make_weak_ptr();
    PF::Registration::Handler navigation_handler
        = [weak, state](auto const& params) -> ErrorOr<NonnullRefPtr<PF::Response>> {
        ++state->handlers;
        if (auto owner = weak.strong_ref()) {
            auto response = TRY(owner->fetch(params));
            state->response = response->make_weak_ptr();
            return response;
        }

        return Error::from_string_literal("Native navigation channel ended");
    };

    PF::Registration::Handler document_handler
        = [](auto const&) -> ErrorOr<NonnullRefPtr<PF::Response>> {
        return Error::from_string_literal("No document provider");
    };

    auto registration = TRY(PF::Registration::install(
        mode == "default"sv ? move(document_handler) : PF::Registration::Handler {},
        mode == "default"sv ? PF::Registration::Handler {} : move(navigation_handler)));
    ScopeGuard revoke = [&] {
        registration->revoke();
        client->revoke();
    };

    GC::Root<H::LocalNavigable> target { *navigable };
    if (mode == "scope"sv) {
        GC::Root other { Web::Bindings::create_a_principal_javascript_realm() };
        auto& other_page = Web::Bindings::principal_host_defined_page(*other);
        auto other_navigable = H::LocalTraversableNavigable::create_a_new_top_level_traversable(
            other_page, nullptr, {});
        other_page.set_top_level_traversable(other_navigable);
        other_navigable->adopt_navigation_started_in_ui_process(id);
        target = *other_navigable;
    }

    auto& population_realm = target->active_document()->relevant_settings_object().realm();
    H::NavigationPopulationRequest request {};
    request.navigable_id = target->id();
    request.navigation_id = id;
    request.history_entry.url = url;
    request.history_entry.document_state.id = { 991, 1 };
    request.history_entry.document_state.initiator_origin = source->origin();
    request.source_snapshot_params
        = H::create_navigation_source_snapshot_without_a_source_document();
    if (mode == "page"sv)
        request.source_snapshot_params.fetch_client
            = source->relevant_settings_object().serialize();
    request.csp_navigation_type = Web::ContentSecurityPolicy::Directives::NavigationType::Other;
    auto populate = [&] {
        H::TemporaryExecutionContext context { population_realm,
            H::TemporaryExecutionContext::CallbacksEnabled::Yes };
        auto snapshot = H::create_source_snapshot_params_from_navigation_source_snapshot(
            population_realm, request.source_snapshot_params);
        target->populate_session_history_entry_document(request, snapshot, false,
            GC::create_function(population_realm.heap(),
                [state](H::NavigationPopulationResult result) { state->result = move(result); }));
    };

    populate();
    if (mode.is_one_of("revoke"sv, "replace"sv)) {
        auto has_headers = [&] {
            auto response = state->response.strong_ref();
            return response && response->headers_received() && !response->finished();
        };

        TRY(wait(realm, [&] { return has_headers() || state->result.has_value(); }));
        if (!has_headers() || client->pending() != 1)
            return Error::from_string_literal("Navigation cancellation has no pending real body");
        if (mode == "revoke"sv)
            client->revoke();
        else
            navigable->adopt_navigation_started_in_ui_process(
                Utf16String::from_utf8("replacement"sv));
    }

    TRY(wait(realm, [&] { return state->result.has_value(); }));
    TRY(validate(*state->result, *target, url, mode.is_one_of("body"sv, "boundary"sv, "replay"sv),
        mode == "boundary"sv ? P::navigation_body_limit : body_length));
    if (mode == "replay"sv) {
        state->result.clear();
        populate();
        TRY(wait(realm, [&] { return state->result.has_value(); }));
        TRY(validate(*state->result, *navigable, url, false));
        if (state->handlers != 2)
            return Error::from_string_literal("One-shot replay did not reach native admission");
    }

    TRY(wait(realm, [&] { return !client->pending(); }));
    if (source->url().scheme() == "app"sv || !source->origin().is_opaque())
        return Error::from_string_literal("Test changed the source document origin");
    outln("PASS actual navigation population over native resource channel {}: body/origin/policy "
          "settlement",
        mode);
    return 0;
}

ErrorOr<void> until(Function<bool()> predicate)
{
    Core::ElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
        if (timer.elapsed_milliseconds() > 20000)
            return Error::from_string_literal("Navigation server test timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return {};
}

ErrorOr<void> parent(StringView path, StringView mode)
{
    Array<ByteString, 1> arguments { ByteString(mode) };
    auto peer = TRY(spawn_byte_peer(TRY(Core::System::current_executable_path()), arguments));
    size_t calls = 0;
    auto server = TRY(P::FileServer::create(
        move(peer->transport), 101, [&](StringView url, bool head) -> ErrorOr<P::AuthorizedFile> {
            ++calls;
            if (url != "app://bundle/index.html"sv || head)
                return Error::from_string_literal("Native navigation URL not authorized");
            return P::AuthorizedFile { ByteString(path),
                { { "Content-Security-Policy", "default-src 'none'" } } };
        }));
    ScopeGuard revoke = [&] { server->revoke(); };
    if (mode == "peerloss"sv) {
        TRY(until([&] { return server->files() || server->is_revoked(); }));
        if (server->files() != 1)
            return Error::from_string_literal("Navigation peer-loss fixture has no open file");
        server->revoke();
    }

    TRY(until([&] { return server->is_revoked(); }));
    auto expected = mode.is_one_of("default"sv, "page"sv, "url"sv, "scope"sv) ? 0u : 1u;
    if (server->files() || server->pending() || calls != expected)
        return Error::from_string_literal("Navigation file authorization/cleanup mismatch");
    if (TRY(peer->wait()) != 0)
        return Error::from_string_literal("Native navigation child failed");
    outln("PASS navigation main-side file admission and reaped child {}", mode);
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments owned;
    TRY(owned.initialize(arguments));
    arguments = owned.arguments();
    Core::EventLoop::initialize_for_current_thread();
    if (arguments.argc == 4 && arguments.strings[1] == "child"sv)
        return child(arguments.strings[2], arguments.strings[3]);
#ifdef AK_OS_WINDOWS
    IPC::HandleBroker::install(make<IPC::LocalHandleBroker>());
#endif
    if (arguments.argc != 3
        || !arguments.strings[1].is_one_of("body"sv, "boundary"sv, "default"sv, "page"sv, "url"sv,
            "scope"sv, "replay"sv, "revoke"sv, "replace"sv, "peerloss"sv, "oversize"sv))
        return Error::from_string_literal(
            "Expected native navigation mode and authorized HTML file");
    TRY(parent(arguments.strings[2], arguments.strings[1]));
    return 0;
}
