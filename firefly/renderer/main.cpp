/**
 * Native renderer probe for process/load/script acceptance. This diagnostic executable does
 * not define application-facing Firefly APIs.
 */

#include "../tests/protocol/Arguments.h"
#include "HelperDirectory.h"
#include <AK/LexicalPath.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Promise.h>
#include <LibCore/Timer.h>
#include <LibGfx/SystemTheme.h>
#include <LibMain/Main.h>
#include <LibWebCommon/WebView/Utilities.h>
#include <LibWebView/Application.h>
#include <LibWebView/HeadlessWebView.h>
#include <cstdio>

namespace Firefly {

// Own the embedding callbacks, while reusing upstream's process launch, IPC,
// view and compositor. No Mite host capabilities are installed in WebContent.
class RendererApplication final : public WebView::Application {
    WEB_VIEW_APPLICATION(RendererApplication)
public:
    explicit RendererApplication(
        ByteString helper_directory = ByteString(FIREFLY_LADYBIRD_DIRECTORY))
        : WebView::Application(move(helper_directory))
    {
    }

    static ErrorOr<NonnullOwnPtr<RendererApplication>> create_checked(
        Main::Arguments const& arguments, Optional<ByteString> const& selected)
    {
        auto app = adopt_own(
            *new RendererApplication(selected.value_or(ByteString(FIREFLY_LADYBIRD_DIRECTORY))));
        if (selected.has_value())
            TRY(Diagnostics::verify_selected_paths(*selected));
        TRY(app->initialize(arguments));
        if (selected.has_value()) {
            outln("PROBE_HELPER_DIRECTORY={}", *selected);
            outln("PROBE_RESOURCE_ROOT={}", WebView::s_ladybird_resource_root);
        }

        return app;
    }

    ErrorOr<int> run_probe()
    {
        auto const& options = browser_options();
        if (options.urls.size() != 1)
            return Error::from_string_literal("The renderer probe requires exactly one URL");
        auto mode = options.headless_mode.value();
        if (mode == WebView::HeadlessMode::Test)
            return Error::from_string_literal(
                "The renderer probe does not expose internal test mode");
        auto theme_path
            = LexicalPath::join(WebView::s_ladybird_resource_root, "themes"sv, "Default.ini"sv);
        auto theme = TRY(Gfx::load_system_theme(theme_path.string()));
        auto view = WebView::HeadlessWebView::create(
            move(theme), { options.window_width, options.window_height });
        auto& loop = *m_loop;
        auto deadline = Core::Timer::create_single_shot(AK::Duration::from_seconds(30), [&] {
            warnln("Firefly renderer probe timed out");
            loop.quit(1);
        });
        RefPtr<Core::Timer> screenshot_timer;
        bool loaded = false;
        view->on_close = [&] { loop.quit(0); };
        view->on_web_content_crashed = [&](auto) {
            warnln("Firefly WebContent crashed");
            loop.quit(1);
        };

        view->on_load_finish = [&](auto const& url) {
            if (loaded || !options.urls.first().equals(url, URL::ExcludeFragment::Yes))
                return;
            loaded = true;
            outln("WebContent PID: {}", view->web_content_pid());
            fflush(stdout); // The parent may inspect this owned live helper in manual mode.
            if (mode == WebView::HeadlessMode::Manual)
                return;
            if (mode == WebView::HeadlessMode::Screenshot) {
                screenshot_timer = Core::Timer::create_single_shot(
                    AK::Duration::from_seconds(options.screenshot_delay), [&] {
                        view->take_screenshot(WebView::ViewImplementation::ScreenshotType::Full)
                            ->when_resolved([&](auto const& path) {
                                outln("Screenshot: {}", path);
                                loop.quit(0);
                            })
                            .when_rejected([&](auto const& error) {
                                warnln("Screenshot failed: {}", error);
                                loop.quit(1);
                            });
                    });
                screenshot_timer->start();
                return;
            }

            auto type = mode == WebView::HeadlessMode::LayoutTree
                ? WebView::PageInfoType::LayoutTree
                : WebView::PageInfoType::Text;
            view->request_internal_page_info(type)
                ->when_resolved([&](auto const& text) {
                    outln("{}", text);
                    loop.quit(0);
                })
                .when_rejected([&](auto const& error) {
                    warnln("Page information failed: {}", error);
                    loop.quit(1);
                });
        };

        view->load(options.urls.first());
        deadline->start();
        return loop.exec();
    }

private:
    void create_platform_options(WebView::BrowserOptions& browser, WebView::RequestServerOptions&,
        WebView::WebContentOptions&) override
    {
        if (!browser.headless_mode.has_value())
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
#ifndef FIREFLY_DIAGNOSTIC_ARGUMENTS_OWNED
    Firefly::Tests::ProtocolArguments native_arguments;
    TRY(native_arguments.initialize(arguments));
    arguments = native_arguments.arguments();
#endif
    Firefly::Diagnostics::HelperDirectoryOption helper;
    TRY(helper.parse(arguments));
    arguments.strings = helper.arguments.span();
    // A development probe must not attach to or alter a user's browser profile.
    Vector<StringView> isolated_arguments;
    isolated_arguments.append(arguments.strings.first());
    isolated_arguments.append("--temporary-profile"sv);
    for (auto argument : arguments.strings.slice(1))
        isolated_arguments.append(argument);
    arguments.strings = isolated_arguments.span();
    auto application
        = TRY(Firefly::RendererApplication::create_checked(arguments, helper.selected));
    return application->run_probe();
}
