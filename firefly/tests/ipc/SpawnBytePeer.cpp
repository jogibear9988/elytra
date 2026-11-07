/** Pass only native transport endpoints to one owned test child; preserve kernel PID binding. */
#include "SpawnBytePeer.h"
#include <AK/Array.h>
#include <AK/Mutex.h>
#include <AK/Random.h>
#include <AK/ScopeGuard.h>
#include <LibCore/Environment.h>
#include <LibCore/System.h>
#if defined(AK_OS_MACOS)
#include <LibIPC/MachBootstrapListener.h>
#include <LibIPC/TransportBootstrapMach.h>
#elif defined(AK_OS_WINDOWS)
#include <LibIPC/HandleBrokerServer.h>
#else
#include <fcntl.h>
#include <sys/socket.h>
#endif

SpawnedBytePeer::~SpawnedBytePeer()
{
    if (!reaped) {
        (void)Core::Process::terminate_process(
            process.pid(), Core::Process::TerminationMode::Forceful);
        (void)process.wait_for_termination();
    }
}

ErrorOr<int> SpawnedBytePeer::wait()
{
    auto status = TRY(process.wait_for_termination());
    reaped = true;
    return status;
}

namespace {
#if defined(AK_OS_MACOS)
struct MacByteResources final : BytePeerResources {
    IPC::TransportBootstrapMachServer server;
    Mutex admission;
    pid_t expected { -1 };
    OwnPtr<IPC::MachBootstrapListener> listener;
};
#elif defined(AK_OS_WINDOWS)
struct WindowsByteResources final : BytePeerResources {
    NonnullOwnPtr<IPC::HandleBrokerServer> broker { IPC::HandleBrokerServer::create() };
};
#endif
}

ErrorOr<NonnullOwnPtr<SpawnedBytePeer>> spawn_byte_peer(
    ByteString const& executable, ReadonlySpan<ByteString> extra_arguments)
{
    Core::ProcessSpawnOptions options;
    options.executable = executable;
    options.die_with_parent = true;
#if defined(AK_OS_MACOS)
    auto resources = make<MacByteResources>();
    auto name = ByteString::formatted(
        "elytra.byte-peer.{}.{}", Core::System::getpid(), get_random<u64>());
    resources->listener = make<IPC::MachBootstrapListener>(name);
    if (!resources->listener->is_initialized())
        return Error::from_string_literal("Cannot initialize native test Mach bootstrap");
    resources->listener->on_bootstrap_request
        = [owner = resources.ptr()](IPC::MachBootstrapListener::BootstrapRequest request) {
              MutexLocker admission(owner->admission);
              if (request.pid != owner->expected)
                  return;
              owner->expected = -1; // One bootstrap; no late PID-reuse/on-demand admission.
              if (auto result
                  = owner->server.handle_bootstrap_request(request.pid, move(request.reply_port));
                  result.is_error())
                  warnln("Native byte-peer bootstrap failed: {}", result.error());
          };

    auto receive = TRY(Core::MachPort::create_with_right(Core::MachPort::PortRight::Receive));
    auto send_to_parent = TRY(receive.insert_right(Core::MachPort::MessageRight::MakeSend));
    auto child_receive = TRY(Core::MachPort::create_with_right(Core::MachPort::PortRight::Receive));
    auto send_to_child = TRY(child_receive.insert_right(Core::MachPort::MessageRight::MakeSend));
    IPC::TransportMachPort::raise_receive_queue_limit(child_receive);
    Vector<ByteString> arguments { "child", name };
    for (auto const& argument : extra_arguments)
        TRY(arguments.try_append(argument));
    options.arguments = arguments;
    MutexLocker admission(resources->admission);
    MutexLocker registration(resources->server.child_registration_lock());
    auto process = TRY(Core::Process::spawn(options));
    resources->expected = process.pid();
    resources->server.register_child_transport(
        process.pid(), { move(child_receive), move(send_to_parent) });
    auto transport = make<IPC::Transport>(move(receive), move(send_to_child));
    return make<SpawnedBytePeer>(move(process), move(transport), move(resources));
#else
    Array<int, 2> fds { -1, -1 };
    ScopeGuard close_fds([&] {
        for (auto fd : fds)
            if (fd >= 0)
                MUST(Core::System::close(fd));
    });
#if defined(AK_OS_WINDOWS)
    auto resources = make<WindowsByteResources>();
    fds = TRY(Core::System::duplex_pipe());
    auto broker = TRY(Core::System::duplex_pipe());
    ScopeGuard close_broker([&] {
        for (auto fd : broker)
            if (fd >= 0)
                MUST(Core::System::close(fd));
    });
#else
    OwnPtr<BytePeerResources> resources;
    int pair[2];
    TRY(Core::System::socketpair(AF_LOCAL, SOCK_STREAM | SOCK_CLOEXEC, 0, pair));
    fds = { pair[0], pair[1] };
#endif
    auto previous = Core::Environment::get("SOCKET_TAKEOVER"sv);
    Optional<ByteString> restore;
    if (previous.has_value())
        restore = ByteString(*previous);
    ScopeGuard restore_environment([&] {
        if (restore.has_value())
            MUST(Core::Environment::set(
                "SOCKET_TAKEOVER"sv, *restore, Core::Environment::Overwrite::Yes));
        else
            MUST(Core::Environment::unset("SOCKET_TAKEOVER"sv));
    });
    Vector<ByteString> arguments { "child", "native" };
    for (auto const& argument : extra_arguments)
        TRY(arguments.try_append(argument));
    options.arguments = arguments;
    options.file_actions.append(Core::FileAction::DupFd { .write_fd = fds[1], .fd = fds[1] });
#if defined(AK_OS_WINDOWS)
    options.file_actions.append(Core::FileAction::DupFd { .write_fd = broker[1], .fd = broker[1] });
    auto takeover = ByteString::formatted("ipc:{};handle-broker:{}", fds[1], broker[1]);
#else
    auto takeover = ByteString::formatted("ipc:{}", fds[1]);
#endif
    TRY(Core::Environment::set("SOCKET_TAKEOVER"sv, takeover, Core::Environment::Overwrite::Yes));
    auto process = TRY(Core::Process::spawn(options));
    ArmedScopeGuard stop_failed_child([&] {
        (void)Core::Process::terminate_process(
            process.pid(), Core::Process::TerminationMode::Forceful);
        (void)process.wait_for_termination();
    });
    auto transport = TRY(IPC::Transport::adopt_fd(fds[0]));
    fds[0] = -1;
#if defined(AK_OS_WINDOWS)
    resources->broker->add_child(process.pid(), broker[0]);
    broker[0] = -1;
#endif
    auto owner = make<SpawnedBytePeer>(move(process), move(transport), move(resources));
    stop_failed_child.disarm();
    return owner;
#endif
}
