/**
 * Embeds the shared Mite runtime in the native GUI event loop. Trusted host modules are
 * registered before application entry and disconnected before VM teardown.
 */

#include "../app-paths/Host.h"
#include "../app-paths/Paths.h"
#include "../command-line/EnginePolicy.h"
#include "../command-line/Host.h"
#include "../command-line/State.h"
#include "../locale/Host.h"
#include "../locale/Locale.h"
#include "../platform/DesktopServices.h"
#include "../platform/RuntimePaths.h"
#include "../platform/StartupOptions.h"
#include "../safe-storage/Host.h"
#include "../theme/ThemeHost.h"
#include "../utility/Bootstrap.h"
#include "../utility/Host.h"
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

// Owned by ladybird_main: both Qt and borrowed native argv outlive the entire
// WebView base destructor, including helper shutdown and database flushes.
struct BootstrapStorage {
    OwnPtr<QApplication> qt;
    int qt_argc { 1 };
    char qt_program[8] { "firefly" };
    char* qt_argv[2] { qt_program, nullptr };
    Vector<ByteString> native_owned;
    std::vector<char*> native_argv;
    Vector<StringView> native_strings;
};

class Application final : public WebView::Application {
public:
    Application(BootstrapStorage& storage, QString directory, bool startup_cpu)
        : WebView::Application(runtime_directory())
        , m_storage(storage)
        , m_paths(std::move(directory))
        , m_engine(startup_cpu)
        , m_startup_cpu(startup_cpu)
    {
        defer_theme_settings();
        create_platform_event_loop();
    }

    int run_script(int argc, char** argv, bool electron_compat)
    {
        CommandLine::State command_line(
            argc, argv, [&](auto name, auto const& value, bool initial) {
                return m_engine.apply(name, value, initial);
            });
        if (auto error = command_line.apply_initial(); error.has_value()) {
            warnln("{}: {}", error->code.c_str(), error->message.c_str());
            return 1;
        }

        bool locale_ready = false;
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
            native->define_direct_property("utility"_utf16,
                Utility::create_host(vm, realm, jobs, electron_compat), JS::default_attributes);
            AppPaths::install_host(realm, *native, m_paths);
            SafeStorage::install_host(
                realm, *native, [&] { return locale_ready; },
                [&] {
                    QString profile;
                    if (m_paths.get("userData", profile).has_value())
                        return std::unique_ptr<SafeStorage::Backend> {};
                    auto bytes = profile.toUtf8();
                    return std::make_unique<SafeStorage::Backend>(
                        std::string(bytes.data(), bytes.size()));
                });
            Locale::install_host(
                realm, *native, [&] { return locale_ready; },
                [&] {
                    auto language = m_engine.locale();
                    auto bytes = language.bytes_as_string_view();
                    return QString::fromUtf8(
                        bytes.characters_without_null_termination(), bytes.length());
                });
            CommandLine::install_host(realm, *native, command_line);
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
            GC::Root files { TRY(modules.builtin_exports("fs")) };
            GC::Root streams { TRY(modules.builtin_exports("stream")) };
            GC::Root transport { TRY(modules.web_transport()) };
            auto api = TRY(JS::call(vm, factory, JS::js_undefined(), JS::Value(native.ptr()),
                TRY(events->get("EventEmitter"_utf16)), JS::Value(paths.ptr()),
                JS::Value(urls.ptr()), JS::Value(files.ptr()), JS::Value(streams.ptr()),
                JS::Value(transport.ptr())));
            if (!api.is_object())
                return vm.throw_completion<JS::Error>("Firefly API factory failed"_utf16);
            std::vector<std::string> names { "app", "BrowserWindow", "ipcMain", "screen",
                "clipboard", "ClipboardItem", "nativeTheme", "protocol", "session", "crashReporter",
                "Menu", "MenuItem", "contentTracing", "MessageChannelMain", "utilityProcess",
                "safeStorage", "WebContentsView", "webContents", "desktopCapturer", "nativeImage",
                "Notification", "powerMonitor", "powerSaveBlocker", "shell", "systemPreferences",
                "net" };

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
                if (bridge->pump_jobs())
                    m_loop->quit(jobs.status());
            });
            if (bridge->pump_jobs())
                return jobs.status();
            if (hooks.exit_code.has_value())
                return *hooks.exit_code;
            if (auto initialized = initialize_profile(); initialized.is_error()) {
                warnln("Firefly native profile initialization failed: {}", initialized.error());
                return 1;
            }

            m_engine.activate();
            activate_theme_settings();
            command_line.seal();
            locale_ready = true;
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
        browser.disable_sql_database = WebView::DisableSQLDatabase::No;
        browser.enable_content_blocker = WebView::EnableContentBlocker::No;
    }

    Vector<String> platform_preferred_languages() const override
    {
        Vector<String> languages;
        for (auto const& language : Locale::preferred_languages()) {
            auto bytes = language.toUtf8();
            auto text = String::from_utf8(StringView(bytes.data(), bytes.size()));
            if (!text.is_error())
                languages.append(text.release_value());
        }

        return languages;
    }

    bool should_coordinate_browser_process() const override
    {
        return false;
    }

    Core::EventLoop& create_platform_event_loop() override
    {
        if (m_loop)
            return *m_loop;
        m_storage.qt = make<QApplication>(m_storage.qt_argc, m_storage.qt_argv);
        m_storage.qt->setQuitOnLastWindowClosed(false);
        Core::EventLoopManager::install(*new Ladybird::EventLoopManagerQt);
        m_loop = &Core::EventLoop::initialize_for_current_thread();
        static_cast<Ladybird::EventLoopImplementationQt&>(m_loop->impl()).set_main_loop();
        return *m_loop;
    }

    ErrorOr<void> initialize_profile()
    {
        if (auto failure = m_paths.seal(); failure.has_value())
            return Error::from_string_literal(
                "Cannot prepare selected application/profile directories");
        auto root = m_paths.session_root().toUtf8();
        m_storage.native_owned.append("firefly"sv);
        m_storage.native_owned.append("--profile-path"sv);
        m_storage.native_owned.append(ByteString(root.data(), root.size()));
        if (m_startup_cpu)
            m_storage.native_owned.append("--force-cpu-painting"sv);
        for (auto& argument : m_storage.native_owned) {
            m_storage.native_argv.push_back(const_cast<char*>(argument.characters()));
            m_storage.native_strings.append(argument.view());
        }

        auto argc = static_cast<int>(m_storage.native_argv.size());
        m_storage.native_argv.push_back(nullptr);
        Main::Arguments selected { argc, m_storage.native_argv.data(),
            m_storage.native_strings.span() };
        return initialize(selected);
    }

    BootstrapStorage& m_storage;
    AppPaths::Paths m_paths;
    CommandLine::EnginePolicy m_engine;
    bool m_startup_cpu;
    Core::EventLoop* m_loop { nullptr };
};
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    if (!Mite::ignore_broken_pipe_signal())
        return Error::from_string_literal("Cannot configure Firefly broken-pipe signal policy");
    // Node-style fork children run the shared standalone engine before any Qt,
    // compositor, window, Firefly API or utility parentPort initialization.
    if (arguments.argc > 1 && std::string_view(arguments.argv[1]) == "--mite-fork-child") {
        Mite::HostHooks hooks;
        hooks.allow_process_signals = true;
        return Mite::run(arguments.argc, arguments.argv, hooks);
    }

    if (arguments.argc > 1 && std::string_view(arguments.argv[1]) == "--firefly-utility")
        return Firefly::Utility::run(arguments.argc, arguments.argv);
    if (arguments.argc > 1 && std::string_view(arguments.argv[1]) == "--firefly-utility-electron")
        return Firefly::Utility::run(arguments.argc, arguments.argv, true);
    auto options = Firefly::startup_options(arguments.argc, arguments.argv);
    if (options.help || !options.error.empty()) {
        outln("Usage: firefly [--electron-compat] <main.js> [arguments...]");
        if (!options.error.empty())
            warnln("{}", options.error.c_str());
        return options.help ? 0 : 1;
    }

    // Parse supported app switches before Application::create starts the compositor.
    Firefly::CommandLine::State initial_switches(options.argc(), options.arguments.data(), {});
    if (initial_switches.error().has_value()) {
        warnln("{}: {}", initial_switches.error()->code.c_str(),
            initial_switches.error()->message.c_str());
        return 1;
    }

    bool cpu_painting = initial_switches.has("force-cpu-painting");
    if (cpu_painting && !initial_switches.value("force-cpu-painting").empty()) {
        warnln("ERR_INVALID_ARG_VALUE: force-cpu-painting does not take a value");
        return 1;
    }

    TRY(Firefly::verify_desktop_environment());
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    auto format = QSurfaceFormat::defaultFormat();
    format.setColorSpace(QColorSpace::SRgb);
    QSurfaceFormat::setDefaultFormat(format);
    std::error_code entry_error;
    auto entry_directory
        = std::filesystem::absolute(options.arguments[1], entry_error).parent_path().string();
    if (entry_error)
        return Error::from_string_literal("Cannot resolve application directory");
    Firefly::BootstrapStorage storage;
    int result;
    {
        auto app = adopt_own(*new Firefly::Application(storage,
            QString::fromUtf8(entry_directory.data(), entry_directory.size()), cpu_painting));
        result = app->run_script(options.argc(), options.arguments.data(), options.electron_compat);
    }

    return result;
}
