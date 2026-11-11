/** Real Hub endpoints surviving their owner and queue; no VM, windows or desktop platform. */
#include "native/qt/hub/ConnectionHooks.h"
#include "native/qt/hub/Frontend.h"
#include "native/qt/hub/Host.h"
#include <AK/Format.h>
#include <LibCore/EventLoop.h>
#include <LibCore/ThreadEventQueue.h>
#include <LibIPC/Transport.h>
#include <LibIPC/TransportHandle.h>
#include <LibMain/Main.h>
#include <LibWebView/BrowserProcess.h>
#include <LibWebView/Settings.h>
#include <QApplication>
#include <QTemporaryDir>
#include <UI/Qt/EventLoopImplementationQt.h>

namespace {
struct Arguments {
    Vector<ByteString> owned;
    Vector<char*> pointers;
    Vector<StringView> strings;

    explicit Arguments(QString const& profile)
    {
        auto bytes = profile.toUtf8();
        owned.append("firefly-hub-lifetime-probe"sv);
        owned.append("--profile-path"sv);
        owned.append(ByteString(bytes.constData(), bytes.size()));
        owned.append("--disable-sql-database"sv);
        owned.append("--disable-content-blocker"sv);
        for (auto& value : owned) {
            pointers.append(const_cast<char*>(value.characters()));
            strings.append(value.view());
        }

        pointers.append(nullptr);
    }

    Main::Arguments get()
    {
        return { static_cast<int>(strings.size()), pointers.data(), strings.span() };
    }
};
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    bool replacement = false;
    for (auto argument : arguments.strings)
        if (argument == "--replacement"sv)
            replacement = true;
    int argc = 3;
    char executable[] = "firefly-hub-lifetime-probe";
    char platform[] = "-platform";
    char offscreen[] = "offscreen";
    char* argv[] = { executable, platform, offscreen, nullptr };
    QApplication application(argc, argv);
    if (QGuiApplication::platformName() != QStringLiteral("offscreen"))
        return Error::from_string_literal("Hub lifetime probe requires the offscreen platform");
    application.setQuitOnLastWindowClosed(false);
    Core::EventLoopManager::install(*new Ladybird::EventLoopManagerQt);
    auto& loop = Core::EventLoop::initialize_for_current_thread();
    static_cast<Ladybird::EventLoopImplementationQt&>(loop.impl()).set_main_loop();
    QTemporaryDir profiles;
    if (!profiles.isValid())
        return Error::from_string_literal("Cannot create private Hub lifetime profiles");
    Arguments first_arguments(profiles.path() + QStringLiteral("/first"));
    unsigned started = 0, stopped = 0, invocations = 0;
    auto create = [&](Arguments& arguments) -> ErrorOr<OwnPtr<Firefly::HubHost>> {
        auto host = make<Firefly::HubHost>(
            loop, ByteString(FIREFLY_LADYBIRD_DIRECTORY),
            [&] {
                Firefly::HubFrontend::initialize();
                ++started;
            },
            [&] {
                Firefly::HubFrontend::shutdown();
                ++stopped;
            });
        TRY(host->initialize(arguments.get()));
        return host;
    };

    auto first = TRY(create(first_arguments));
    RefPtr server = Hub::ConnectionFromUI::the();
    RefPtr client = Ladybird::HubConnection::the_if_connected();
    if (!server || !client || !server->is_open() || !client->is_open())
        return Error::from_string_literal("Real Hub endpoints were not created");
    if (replacement) {
        // Replacement endpoints exercise identity safety on this SAME Application.
        // The SDK's process-wide stores do not support starting a second Application.
        auto retire = [](auto& server, auto& client) {
            Firefly::HubLifecycle::retire_ui_connection(*server);
            Firefly::HubLifecycle::retire_ui_client(*client);
            client->shutdown();
            server->shutdown();
        };

        retire(server, client);
        auto first_pair = TRY(IPC::TransportHandle::create_pair());
        RefPtr replaced_server
            = Hub::ConnectionFromUI::construct(TRY(first_pair.first.create_transport()));
        RefPtr replaced_client
            = adopt_ref(*new Ladybird::HubConnection(TRY(first_pair.second.create_transport())));
        auto replaced_server_weak = replaced_server->make_weak_ptr();
        auto replaced_client_weak = replaced_client->make_weak_ptr();
        retire(replaced_server, replaced_client);
        auto next_pair = TRY(IPC::TransportHandle::create_pair());
        RefPtr current_server
            = Hub::ConnectionFromUI::construct(TRY(next_pair.first.create_transport()));
        RefPtr current_client
            = adopt_ref(*new Ladybird::HubConnection(TRY(next_pair.second.create_transport())));
        replaced_server->die();
        static_cast<IPC::ConnectionBase&>(*replaced_client).die();
        if (loop.was_exit_requested())
            return Error::from_string_literal(
                "Retired endpoint changed the later connection exit state");
        replaced_server = nullptr;
        replaced_client = nullptr;
        // These are ordinary retained native refs, not deferred self-references.
        // Their destructors must preserve the current pair's real callback stores.
        if (replaced_server_weak || replaced_client_weak || invocations
            || Hub::ConnectionFromUI::the() != current_server.ptr()
            || Ladybird::HubConnection::the_if_connected() != current_client.ptr()
            || !current_server->is_open() || !current_client->is_open()
            || !WebView::Application::settings().on_persist
            || !WebView::Application::the().browser_process().on_new_tab
            || !WebView::Application::the().browser_process().on_new_window
            || loop.was_exit_requested())
            return Error::from_string_literal(
                "Late endpoint destruction altered the current connection");
        retire(current_server, current_client);
        retire(current_server, current_client);
        current_server = nullptr;
        current_client = nullptr;
    }

    auto server_weak = server->make_weak_ptr();
    auto client_weak = client->make_weak_ptr();
    // The original Host's actual endpoints now survive its Application through
    // the same EventReceiver strong self-reference that crashed at process exit.
    server->deferred_invoke([&] { ++invocations; });
    client->deferred_invoke([&] { ++invocations; });
    first->shutdown();
    first->shutdown();
    if (server->is_open() || client->is_open() || Hub::ConnectionFromUI::the()
        || Ladybird::HubConnection::the_if_connected() || WebView::Application::is_initialized()
        || started != 1 || stopped != 1)
        return Error::from_string_literal("Hub shutdown did not retire endpoints exactly once");
    first = nullptr;
    if (Firefly::HubLifecycle::embedded_event_loop())
        return Error::from_string_literal("Retired Hub kept its borrowed configuration");
    server->die();
    static_cast<IPC::ConnectionBase&>(*client).die();
    server = nullptr;
    client = nullptr;
    if (!server_weak || !client_weak)
        return Error::from_string_literal("Post-Hub endpoints were not retained by the real queue");
    if (loop.was_exit_requested())
        return Error::from_string_literal("Retired endpoint changed the final exit state");
    // This is the only queue destruction, at the end of the process fixture.
    // No event-loop operations follow: its implementation still borrows the queue.
    Core::ThreadEventQueue::destroy_current();
    if (server_weak || client_weak || invocations || WebView::Application::is_initialized()
        || Hub::ConnectionFromUI::the() || Ladybird::HubConnection::the_if_connected())
        return Error::from_string_literal(
            "Late IPC destruction retained Hub ownership or ran callbacks");
    outln("PASS actual Hub retained IPC endpoints: {} mode, repeat shutdown and final queue "
          "destruction",
        replacement ? "replacement" : "natural Host shutdown");
    return 0;
}
