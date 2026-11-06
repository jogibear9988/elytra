/**
 * Embeds the shared Mite runtime in the native GUI event loop. Trusted host modules are
 * registered before application entry and disconnected before VM teardown.
 */

#include "ApiSource.h"
#include "Bridge.h"
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "platform/process/SignalPolicy.h"
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibMain/Main.h>
#include <LibWebView/Application.h>
#include <QApplication>
#include <QColorSpace>
#include <QSurfaceFormat>
#include <QTimer>
#include <UI/Qt/EventLoopImplementationQt.h>
#include <filesystem>
#include <memory>

template <> constexpr bool AllocatedWithSystemAllocator<QApplication> = true;

namespace Firefly {
// Prefer physically adjacent helpers/resources in a packaged application. The
// development build keeps its existing upstream bundle; no cwd/env override.
static ByteString runtime_directory()
{
    auto executable = Core::System::current_executable_path();
    if (!executable.is_error()) {
        auto directory = std::filesystem::path(executable.value().characters()).parent_path();
        std::error_code error;
        if (std::filesystem::is_regular_file(directory / "WebContent", error)
            && std::filesystem::is_directory(directory.parent_path() / "Resources", error))
            return ByteString(directory.string().c_str());
    }

    return ByteString(FIREFLY_LADYBIRD_DIRECTORY);
}

class Application final : public WebView::Application {
    WEB_VIEW_APPLICATION(Application)
public:
    Application()
        : WebView::Application(runtime_directory())
    {
    }

    int run_script(int argc, char** argv)
    {
        std::unique_ptr<Bridge> bridge;
        Mite::HostHooks hooks;
        hooks.terminate_pending_work_on_stop = true;
        hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                               Mite::EventLoop& jobs) -> JS::ThrowCompletionOr<void> {
            bridge = std::make_unique<Bridge>(vm, realm, jobs, *m_loop, hooks.exit_code);
            GC::Root native { bridge->exports() };
            std::error_code error;
            auto entry = std::filesystem::absolute(argv[1], error).parent_path().string();
            if (error)
                return vm.throw_completion<JS::Error>("Cannot resolve application directory"_utf16);
            native->define_direct_property("appPath"_utf16,
                JS::PrimitiveString::create(vm,
                    Utf16String::from_utf8_with_replacement_character(
                        StringView(entry.data(), entry.size()))),
                JS::default_attributes);
            auto factory = TRY(modules.run_eval(FIREFLY_API_SOURCE));
            GC::Root factory_root { factory.as_object() };
            GC::Root events { TRY(modules.builtin_exports("events")) };
            GC::Root paths { TRY(modules.builtin_exports("path")) };
            GC::Root urls { TRY(modules.builtin_exports("url")) };
            auto api = TRY(JS::call(vm, factory, JS::js_undefined(), JS::Value(native.ptr()),
                TRY(events->get("EventEmitter"_utf16)), JS::Value(paths.ptr()),
                JS::Value(urls.ptr())));
            if (!api.is_object())
                return vm.throw_completion<JS::Error>("Firefly API factory failed"_utf16);
            TRY(modules.register_host_module("firefly", api.as_object(),
                { "app", "BrowserWindow", "ipcMain", "screen", "clipboard", "ClipboardItem",
                    "nativeTheme" }));
            return {};
        };

        hooks.drive = [&](Mite::EventLoop& jobs) {
            QTimer poll;
            poll.setInterval(1);
            poll.setTimerType(Qt::PreciseTimer);
            QObject::connect(&poll, &QTimer::timeout, [&] {
                if (jobs.pump_once())
                    m_loop->quit(jobs.status());
            });
            if (jobs.pump_once())
                return jobs.status();
            bridge->ready();
            poll.start();
            auto status = m_loop->exec();
            poll.stop();
            return status ? status : jobs.status();
        };

        hooks.shutdown = [&] { bridge.reset(); };
        return Mite::run(argc, argv, hooks);
    }

private:
    void create_platform_options(WebView::BrowserOptions& browser, WebView::RequestServerOptions&,
        WebView::WebContentOptions&) override
    {
        browser.disable_sql_database = WebView::DisableSQLDatabase::Yes;
        browser.enable_content_blocker = WebView::EnableContentBlocker::No;
    }

    bool should_coordinate_browser_process() const override
    {
        return false;
    }

    Core::EventLoop& create_platform_event_loop() override
    {
        m_qt = make<QApplication>(arguments().argc, arguments().argv);
        m_qt->setQuitOnLastWindowClosed(false);
        Core::EventLoopManager::install(*new Ladybird::EventLoopManagerQt);
        m_loop = &Core::EventLoop::initialize_for_current_thread();
        static_cast<Ladybird::EventLoopImplementationQt&>(m_loop->impl()).set_main_loop();
        return *m_loop;
    }

    OwnPtr<QApplication> m_qt;
    Core::EventLoop* m_loop { nullptr };
};
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    if (!Mite::ignore_broken_pipe_signal())
        return Error::from_string_literal("Cannot configure Firefly broken-pipe signal policy");
    if (arguments.argc < 2 || arguments.strings[1] == "--help"sv) {
        outln("Usage: firefly <main.js> [arguments...]");
        return arguments.argc < 2 ? 1 : 0;
    }

    if (arguments.strings[1].starts_with('-'))
        return Error::from_string_literal("Unsupported Firefly CLI option");
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    auto format = QSurfaceFormat::defaultFormat();
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);
    char program[] = "firefly";
    char temporary[] = "--temporary-profile";
    char* browser_argv[] { program, temporary, nullptr };
    StringView browser_strings[] { "firefly"sv, "--temporary-profile"sv };
    Main::Arguments browser_args { 2, browser_argv, browser_strings };
    auto app = TRY(Firefly::Application::create(browser_args));
    return app->run_script(arguments.argc, arguments.argv);
}
