/** Headless integration gate: transfer an owned endpoint to the actual sandboxed renderer. */
#include "native/ipc/bytes/Channel.h"
#include <AK/ByteBuffer.h>
#include <AK/LexicalPath.h>
#include <AK/Random.h>
#include <AK/ScopeGuard.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Promise.h>
#include <LibCore/Timer.h>
#include <LibGfx/SystemTheme.h>
#include <LibMain/Main.h>
#include <LibWebCommon/WebView/Utilities.h>
#include <LibWebView/Application.h>
#include <LibWebView/HeadlessWebView.h>

namespace Firefly {
namespace {
    bool upstream_control = false;
    bool invalid_cookie = false;

    // Promise/channel callbacks retain this state, never stack references. The raw view
    // and loop are usable only while running; cleanup invalidates them before destruction.
    struct ProbeState : RefCounted<ProbeState> {
        WebView::HeadlessWebView* view { nullptr };
        Core::EventLoop* loop { nullptr };
        RefPtr<Bytes::Channel> channel;
        RefPtr<Bytes::Channel> duplicate;
        ByteBuffer payload;
        u64 high { 0 };
        u64 low { 0 };
        u64 pid { 0 };
        bool running { true };
        bool hello { false };
        bool finished { false };

        void fail(StringView message)
        {
            if (!running || finished)
                return;
            finished = true;
            warnln("Byte renderer probe failed: {}", message);
            loop->quit(1);
        }

        void check_document()
        {
            NonnullRefPtr self = *this;
            view->request_internal_page_info(WebView::PageInfoType::Text)
                ->when_resolved([self](auto const& text) {
                    if (!self->running || self->finished)
                        return;
                    if (!text.contains("BYTE_DOCUMENT_OK CAPABILITIES_ABSENT=true"sv)) {
                        self->fail("Renderer document/control connection did not survive"sv);
                        return;
                    }

                    outln("WebContent PID: {}", self->pid);
                    outln("{}",
                        upstream_control || invalid_cookie ? "BYTE_HANDOFF_DENIED_DOCUMENT_ALIVE"sv
                                                           : "BYTE_HANDOFF_OK bytes=65536"sv);
                    self->finished = true;
                    self->loop->quit(0);
                })
                .when_rejected([self](auto const&) {
                    self->fail("Document reply failed after byte handoff"sv);
                });
        }

        ErrorOr<void> reject_duplicate()
        {
            auto pair = TRY(IPC::Transport::create_paired());
            NonnullRefPtr self = *this;
            duplicate = TRY(Bytes::Channel::create(move(pair.local),
                {
                    .on_data = [self](auto) { self->fail("Duplicate endpoint admitted"sv); },
                    .on_writable = {},
                    .on_closed =
                        [self](auto reason) {
                            if (!self->running || self->finished)
                                return;
                            if (reason != Bytes::Channel::CloseReason::Peer) {
                                self->fail("Duplicate endpoint did not close normally"sv);
                                return;
                            }

                            outln("BYTE_DUPLICATE_DENIED");
                            if (self->channel->send(self->payload).is_error())
                                self->fail("Original endpoint lost after duplicate"sv);
                        },
                }));
            view->page().client().async_firefly_connect_byte_channel(
                move(pair.remote_handle), high, low);
            return {};
        }

        ErrorOr<void> connect()
        {
            pid = static_cast<u64>(view->web_content_pid());
            if (!pid)
                return Error::from_string_literal("Missing native renderer PID");
            payload = TRY(ByteBuffer::create_uninitialized(65536));
            for (size_t i = 0; i < payload.size(); ++i)
                payload[i] = static_cast<u8>((i * 37 + 11) & 255);
            do {
                high = get_random<u64>();
                low = get_random<u64>();
            } while (!(high || low));
            if (invalid_cookie)
                high = low = 0;
            auto pair = TRY(IPC::Transport::create_paired());
            NonnullRefPtr self = *this;
            channel = TRY(Bytes::Channel::create(move(pair.local),
                {
                    .on_data =
                        [self](ReadonlyBytes data) {
                            if (!self->running || self->finished)
                                return;
                            if (upstream_control || invalid_cookie) {
                                self->fail("Denied helper unexpectedly accepted bytes"sv);
                                return;
                            }

                            if (!self->hello) {
                                if (data.size() != 24) {
                                    self->fail("Incorrect handshake length"sv);
                                    return;
                                }

                                u64 words[3] {};
                                for (size_t word = 0; word < 3; ++word)
                                    for (size_t byte = 0; byte < 8; ++byte)
                                        words[word] |= static_cast<u64>(data[word * 8 + byte])
                                            << (byte * 8);
                                if (words[0] != self->high || words[1] != self->low
                                    || words[2] != self->pid) {
                                    self->fail("Renderer PID/cookie mismatch"sv);
                                    return;
                                }

                                self->hello = true;
                                if (self->reject_duplicate().is_error())
                                    self->fail("Duplicate admission setup failed"sv);
                                return;
                            }

                            if (data != self->payload.bytes()) {
                                self->fail("Native arbitrary bytes changed"sv);
                                return;
                            }

                            self->check_document();
                        },
                    .on_writable = {},
                    .on_closed =
                        [self](auto reason) {
                            if (!self->running || self->finished)
                                return;
                            if ((upstream_control || invalid_cookie)
                                && reason == Bytes::Channel::CloseReason::Peer)
                                self->check_document();
                            else
                                self->fail("Unexpected native endpoint close"sv);
                        },
                }));
            view->page().client().async_firefly_connect_byte_channel(
                move(pair.remote_handle), high, low);
            return {};
        }
    };
}

class ByteRendererApplication final : public WebView::Application {
    WEB_VIEW_APPLICATION(ByteRendererApplication)
public:
    ByteRendererApplication()
        : WebView::Application(ByteString(
              upstream_control ? FIREFLY_UPSTREAM_DIRECTORY : FIREFLY_LADYBIRD_DIRECTORY))
    {
    }

    ErrorOr<int> run_probe()
    {
        auto const& options = browser_options();
        if (options.urls.size() != 1 || options.headless_mode != WebView::HeadlessMode::Text)
            return Error::from_string_literal("Byte probe requires one URL and headless text mode");
        auto theme_path
            = LexicalPath::join(WebView::s_ladybird_resource_root, "themes"sv, "Default.ini"sv);
        auto view
            = WebView::HeadlessWebView::create(TRY(Gfx::load_system_theme(theme_path.string())),
                { options.window_width, options.window_height });
        auto state = adopt_ref(*new ProbeState);
        state->view = view.ptr();
        state->loop = m_loop;
        ScopeGuard cleanup = [&] {
            state->running = false;
            view->on_load_finish = {};
            view->on_close = {};
            view->on_web_content_crashed = {};
            if (state->channel)
                state->channel->close();
            if (state->duplicate)
                state->duplicate->close();
            state->channel.clear();
            state->duplicate.clear();
            state->view = nullptr;
            state->loop = nullptr;
        };

        auto deadline = Core::Timer::create_single_shot(
            AK::Duration::from_seconds(30), [state] { state->fail("Timeout"sv); });
        view->on_close = [state] { state->fail("Unexpected view close"sv); };
        view->on_web_content_crashed = [state](auto) { state->fail("Renderer crashed"sv); };
        bool loaded = false;
        view->on_load_finish = [&, state](auto const& url) {
            if (loaded || !options.urls.first().equals(url, URL::ExcludeFragment::Yes))
                return;
            loaded = true;
            if (auto result = state->connect(); result.is_error()) {
                warnln("Byte handoff setup: {}", result.error());
                state->fail("Handoff setup failed"sv);
            }
        };

        view->load(options.urls.first());
        deadline->start();
        return m_loop->exec();
    }

private:
    void create_platform_options(WebView::BrowserOptions& browser, WebView::RequestServerOptions&,
        WebView::WebContentOptions&) override
    {
        browser.headless_mode = WebView::HeadlessMode::Text;
        browser.disable_sql_database = WebView::DisableSQLDatabase::Yes;
        browser.enable_content_blocker = WebView::EnableContentBlocker::No;
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
    Vector<StringView> isolated_arguments;
    isolated_arguments.append(arguments.strings.first());
    isolated_arguments.append("--temporary-profile"sv);
    for (auto argument : arguments.strings.slice(1)) {
        if (argument == "--upstream-control"sv)
            Firefly::upstream_control = true;
        else if (argument == "--invalid-cookie"sv)
            Firefly::invalid_cookie = true;
        else
            isolated_arguments.append(argument);
    }

    arguments.strings = isolated_arguments.span();
    auto application = TRY(Firefly::ByteRendererApplication::create(arguments));
    return application->run_probe();
}
