/**
 * Embeds the shared Mite runtime in the native GUI event loop. Trusted host modules are
 * registered before application entry and disconnected before VM teardown.
 */

#include "../platform/DesktopServices.h"
#include "../platform/RuntimePaths.h"
#include "../platform/StartupOptions.h"
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
#include <vector>

template <> constexpr bool AllocatedWithSystemAllocator<QApplication> = true;

namespace Firefly {
// Resolve from the native executable, never argv[0] or the application's working directory.
static ByteString runtime_directory()
{
    auto executable = Core::System::current_executable_path();
#ifdef __APPLE__
    constexpr auto layout = RuntimeLayout::MacOS;
#else
    constexpr auto layout = RuntimeLayout::Linux;
#endif
    auto selected = Firefly::runtime_directory(
        executable.is_error() ? std::filesystem::path {} : executable.value().characters(),
        FIREFLY_LADYBIRD_DIRECTORY, layout);
    return ByteString(selected.string().c_str());
}

class Application final : public WebView::Application {
    WEB_VIEW_APPLICATION(Application)
public:
    Application()
        : WebView::Application(runtime_directory())
    {
    }

    int run_script(int argc, char** argv, bool electron_compat)
    {
        std::unique_ptr<Bridge> bridge;
        Mite::HostHooks hooks;
        hooks.allow_process_signals = true;
        hooks.terminate_pending_work_on_stop = true;
        hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                               Mite::EventLoop& jobs) -> JS::ThrowCompletionOr<void> {
            TRY(modules.register_host_filesystem("original-fs"));
            bridge = std::make_unique<Bridge>(
                vm, realm, jobs, *m_loop, hooks.exit_code, electron_compat);
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
            std::vector<std::string> names { "app", "BrowserWindow", "ipcMain", "screen",
                "clipboard", "ClipboardItem", "nativeTheme" };
            TRY(modules.register_host_module("firefly", api.as_object(), names));
            if (electron_compat)
                TRY(modules.register_host_module("electron", api.as_object(), names));
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
    auto options = Firefly::startup_options(arguments.argc, arguments.argv);
    if (options.help || !options.error.empty()) {
        outln("Usage: firefly [--electron-compat] <main.js> [arguments...]");
        if (!options.error.empty())
            warnln("{}", options.error.c_str());
        return options.help ? 0 : 1;
    }

    TRY(Firefly::verify_desktop_environment());
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
    return app->run_script(options.argc(), options.arguments.data(), options.electron_compat);
}
