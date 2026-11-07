/** Load a real custom-origin DOM through the production helper and trusted native channel. */
#include "../tests/protocol/Arguments.h"
#include "DocumentResources.h"
#include "native/ipc/TransportHooks.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/files/Server.h"
#include <AK/LexicalPath.h>
#include <AK/WeakPtr.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Promise.h>
#include <LibCore/System.h>
#include <LibCore/Timer.h>
#include <LibGfx/SystemTheme.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWebCommon/WebView/Utilities.h>
#include <LibWebView/Application.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/HeadlessWebView.h>

namespace Firefly {
namespace {
    ByteString fixture;
    ByteString mode;

    bool document_mode()
    {
        return mode.is_one_of(
            "resources"sv, "document-id"sv, "document-missing"sv, "document-duplicate"sv, "away"sv);
    }

    bool bundle_allowed()
    {
        return mode.is_one_of("resources"sv, "document-duplicate"sv, "away"sv);
    }

    struct State final : RefCounted<State>, Weakable<State> {
        WebView::HeadlessWebView* view { nullptr };
        Core::EventLoop* loop { nullptr };
        Vector<NonnullRefPtr<Protocols::FileServer>> servers;
        RefPtr<Bytes::Channel> duplicate;
        RefPtr<DocumentResources> documents;
        bool navigated_away { false };
        size_t calls { 0 }, handoffs { 0 };
        pid_t source_pid { -1 };
        bool active { true }, finished { false }, verified { false }, duplicate_closed { false };
        bool retirement_checked { false };

        void fail(StringView reason)
        {
            if (!active || finished)
                return;
            warnln("Native resource renderer probe: {}", reason);
            finished = true;
            loop->quit(1);
        }

        void complete()
        {
            if (!active || finished || !verified || (mode == "duplicate"sv && !duplicate_closed))
                return;
            for (auto const& server : servers) {
                if (server->pending() || server->files()) {
                    fail("Loaded document retained pending native file jobs"sv);
                    return;
                }

                if (!server->is_revoked())
                    return; // The helper must retire its completed scope itself.
            }

            if (documents) {
                if (documents->failed()) {
                    fail("Duplicate document channel admitted"sv);
                    return;
                }

                if (!documents->ready(navigated_away))
                    return;
            }

            if (!retirement_checked) {
                retirement_checked = true;
                verified = false;
                // ViewImplementation clears its one pending info slot only after
                // invoking our callback. Start the follow-up outside that callback.
                NonnullRefPtr self { *this };
                Core::deferred_invoke([self] {
                    if (self->active && !self->finished)
                        self->verify();
                });
                return;
            }

            bool allowed = mode == "body"sv || mode == "duplicate"sv || document_mode();
            if (calls != static_cast<size_t>(allowed ? 1 : 0)
                || handoffs
                    != static_cast<size_t>(mode.is_one_of("missing"sv, "cancel"sv) ? 0 : 1)) {
                fail("Unexpected native file admissions/handoffs"sv);
                return;
            }

            if (documents)
                outln("DOCUMENT_RENDERER_{} mode={} resource_requests={} teardown={}",
                    bundle_allowed() ? "OK"sv : "DENIED"sv, mode, documents->requests(),
                    navigated_away);
            outln("RESOURCE_PARENT_PID={} RESOURCE_SOURCE_PID={} RESOURCE_DOCUMENT_PID={}",
                Core::System::getpid(), source_pid, view->web_content_pid());
            outln("RESOURCE_RENDERER_{} mode={} file_admissions={}", allowed ? "OK"sv : "DENIED"sv,
                mode, calls);
            finished = true;
            loop->quit(0);
        }

        void verify()
        {
            NonnullRefPtr self { *this };
            view->request_internal_page_info(WebView::PageInfoType::Text)
                ->when_resolved([self](auto const& text) {
                    if (!self->active || self->finished)
                        return;
                    if (document_mode()) {
                        bool marker = text.contains(
                            "RESOURCE_BUNDLE_OK origin=app://bundle secure=true css=loaded answer=42 bytes=4194323 head=true denied=true capabilities_absent=true"sv);
                        if (!self->navigated_away && bundle_allowed() && !marker
                            && text.contains("RESOURCE_BUNDLE_PENDING"sv)) {
                            // Module evaluation can await fetch after the load event.
                            Core::deferred_invoke([self] {
                                if (self->active && !self->finished)
                                    self->verify();
                            });
                            return;
                        }

                        if ((self->navigated_away && !text.contains("RESOURCE_DOCUMENT_REPLACED"sv))
                            || (!self->navigated_away && marker != bundle_allowed())) {
                            warnln("Document bundle text: {}", text);
                            self->fail("Document bundle module/CSS/fetch/identity mismatch"sv);
                            return;
                        }

                        if (mode == "away"sv && !self->navigated_away) {
                            self->navigated_away = true;
                            Core::deferred_invoke([self] {
                                if (self->active)
                                    self->view->load(URL::Parser::basic_parse(
                                        "data:text/html,<body>RESOURCE_DOCUMENT_REPLACED</body>"sv)
                                            .release_value());
                            });
                            return;
                        }

                        self->verified = true;
                        self->complete();
                        return;
                    }

                    bool allowed = mode == "body"sv || mode == "duplicate"sv;
                    bool marker = text.contains(
                        "RESOURCE_DOM_OK origin=app://bundle secure=true capabilities_absent=true"sv);
                    if (mode == "cancel"sv && !text.contains("RESOURCE_CANCELLED_ALIVE"sv)) {
                        self->fail("Replacement document did not survive reentrant cancellation"sv);
                        return;
                    }

                    if (marker != allowed) {
                        warnln("Resource renderer document: {}", text);
                        self->fail("Real DOM origin/capability marker mismatch"sv);
                        return;
                    }

                    self->verified = true;
                    self->complete();
                })
                .when_rejected(
                    [self](auto const&) { self->fail("Document observation rejected"sv); });
        }

        void send(WebView::WebContentPage& page,
            Web::HTML::NavigationPopulationRequest const& request, IPC::TransportHandle handle,
            u64 epoch, bool invalid)
        {
            page.client().async_firefly_connect_navigation(move(handle),
                page.id().value() + (mode == "page"sv ? 9999 : 0),
                request.navigable_id.namespace_id, request.navigable_id.local_id,
                invalid ? Utf16String::from_utf8("not-current"sv) : request.navigation_id,
                request.history_entry.url.serialize(), epoch);
        }

        ErrorOr<void> prepare(
            WebView::WebContentPage& page, Web::HTML::NavigationPopulationRequest const& request)
        {
            if (mode.is_one_of("cancel"sv, "away"sv)
                && request.history_entry.url.scheme() == "data"sv)
                return {};
            if (!active || request.history_entry.url.serialize() != "app://bundle/index.html"sv
                || request.source_snapshot_params.fetch_client.has_value()
                || page.id() != view->page().id()
                || request.navigable_id != view->traversable().id())
                return Error::from_string_literal("Unexpected native navigation preparation scope");
            if (mode == "cancel"sv) {
                source_pid = view->web_content_pid();
                view->traversable().did_cancel_navigation(request.navigation_id);
                view->load(URL::Parser::basic_parse(
                    "data:text/html,<body>RESOURCE_CANCELLED_ALIVE</body>"sv)
                        .release_value());
                if (request.history_entry.url.serialize() != "app://bundle/index.html"sv)
                    return Error::from_string_literal("Reentrant hook lost its retained request");
                return {};
            }

            if (handoffs++)
                return Error::from_string_literal("Repeated native navigation preparation");
            source_pid = view->web_content_pid();
            auto pair = TRY(IPC::Transport::create_paired());
            auto weak = make_weak_ptr();
            auto server = TRY(Protocols::FileServer::create(move(pair.local), 1,
                [weak](StringView url, bool head) -> ErrorOr<Protocols::AuthorizedFile> {
                    auto self = weak.strong_ref();
                    if (!self || !self->active || head || url != "app://bundle/index.html"sv)
                        return Error::from_string_literal("Native fixture URL is not authorized");
                    ++self->calls;
                    return Protocols::AuthorizedFile { fixture,
                        { { "Content-Security-Policy",
                              document_mode() ? "default-src 'none'; script-src 'self'; style-src "
                                                "'self'; connect-src 'self'"
                                              : "default-src 'none'; script-src 'unsafe-inline'" },
                            { "Cache-Control", "no-store" } } };
                }));
            servers.append(move(server));
            send(page, request, move(pair.remote_handle), 1, mode == "id"sv);
            if (mode == "duplicate"sv) {
                auto second = TRY(IPC::Transport::create_paired());
                duplicate = TRY(Bytes::Channel::create(move(second.local),
                    {
                        .on_data =
                            [weak](auto) {
                                if (auto self = weak.strong_ref())
                                    self->fail("Duplicate native channel admitted"sv);
                            },
                        .on_writable = {},
                        .on_closed =
                            [weak](auto reason) {
                                if (auto self = weak.strong_ref()) {
                                    if (!self->active)
                                        return;
                                    if (reason != Bytes::Channel::CloseReason::Peer) {
                                        self->fail("Duplicate native channel did not close as a "
                                                   "refused peer"sv);
                                        return;
                                    }

                                    self->duplicate_closed = true;
                                    self->complete();
                                }
                            },
                    }));
                send(page, request, move(second.remote_handle), 2, false);
            }

            return {};
        }
    };
}

class ResourceRendererApplication final : public WebView::Application {
    WEB_VIEW_APPLICATION(ResourceRendererApplication)
public:
    ResourceRendererApplication()
        : WebView::Application(ByteString(FIREFLY_RESOURCE_HELPER_DIRECTORY))
    {
    }

    ErrorOr<int> run_probe()
    {
        auto theme
            = LexicalPath::join(WebView::s_ladybird_resource_root, "themes"sv, "Default.ini"sv);
        auto view = WebView::HeadlessWebView::create(
            TRY(Gfx::load_system_theme(theme.string())), { 800, 600 });
        auto state = adopt_ref(*new State);
        state->view = view.ptr();
        state->loop = m_loop;
        if (document_mode())
            state->documents = adopt_ref(*new DocumentResources(fixture, mode));
        auto weak = state->make_weak_ptr();
        ScopeGuard cleanup = [&] {
            state->active = false;
            set_native_navigation_handler({});
            set_native_document_handler({});
            if (state->documents)
                state->documents->revoke();
            view->on_load_finish = {};
            view->on_close = {};
            view->on_web_content_crashed = {};
            for (auto& server : state->servers)
                server->revoke();
            if (state->duplicate)
                state->duplicate->close();
        };

        if (mode != "missing"sv)
            set_native_navigation_handler([weak](auto& page, auto const& request) {
                if (auto self = weak.strong_ref())
                    if (auto result = self->prepare(page, request); result.is_error()) {
                        warnln("Navigation bootstrap preparation: {}", result.error());
                        self->fail("Native navigation preparation failed"sv);
                    }
            });
        if (state->documents)
            set_native_document_handler(
                [weak](auto& page, auto const& request, auto const& result) {
                    if (auto self = weak.strong_ref())
                        if (auto prepared = self->documents->prepare(page, request, result);
                            prepared.is_error()) {
                            warnln("Document bootstrap preparation: {}", prepared.error());
                            self->fail("Document bootstrap failed"sv);
                        }
                });
        auto timer = Core::Timer::create_single_shot(
            AK::Duration::from_seconds(30), [state] { state->fail("Timeout"sv); });
        auto poll = Core::Timer::create_repeating(
            AK::Duration::from_milliseconds(20), [state] { state->complete(); });
        poll->start();
        view->on_close = [state] { state->fail("Unexpected view close"sv); };
        view->on_web_content_crashed = [state](auto) { state->fail("Owned helper crashed"sv); };
        bool loaded = false;
        view->on_load_finish = [state, &loaded](auto const& url) {
            if (mode == "away"sv && state->navigated_away && url.scheme() == "data"sv) {
                state->verify();
                return;
            }

            if (mode == "cancel"sv && url.scheme() != "data"sv)
                return;
            if (!state->active || loaded)
                return;
            loaded = true;
            state->verify();
        };

        view->load(URL::Parser::basic_parse("app://bundle/index.html"sv).release_value());
        timer->start();
        return m_loop->exec();
    }

private:
    void create_platform_options(WebView::BrowserOptions& options, WebView::RequestServerOptions&,
        WebView::WebContentOptions&) override
    {
        options.headless_mode = WebView::HeadlessMode::Text;
        options.disable_sql_database = WebView::DisableSQLDatabase::Yes;
        options.enable_content_blocker = WebView::EnableContentBlocker::No;
    }

    Core::EventLoop& create_platform_event_loop() override
    {
        m_loop = &Core::EventLoop::initialize_for_current_thread();
        return *m_loop;
    }

    bool should_coordinate_browser_process() const override
    {
        return false;
    }

    Core::EventLoop* m_loop { nullptr };
};
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments owned;
    TRY(owned.initialize(arguments));
    arguments = owned.arguments();
    if (arguments.argc != 3
        || !arguments.strings[1].is_one_of("body"sv, "duplicate"sv, "id"sv, "page"sv, "missing"sv,
            "cancel"sv, "resources"sv, "document-id"sv, "document-missing"sv,
            "document-duplicate"sv, "away"sv))
        return Error::from_string_literal(
            "Expected resource renderer mode and authorized HTML path");
    Firefly::mode = arguments.strings[1];
    Firefly::fixture = arguments.strings[2];
    Array schemes { Firefly::Protocols::OriginScheme { "app"_string, true, true } };
    TRY(Firefly::Protocols::install_origin_schemes(schemes));
    Vector<StringView> filtered { arguments.strings[0], "--temporary-profile"sv,
        "app://bundle/index.html"sv };
    arguments.strings = filtered.span();
    auto application = TRY(Firefly::ResourceRendererApplication::create(arguments));
    return application->run_probe();
}
