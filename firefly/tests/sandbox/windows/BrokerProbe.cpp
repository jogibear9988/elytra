/** Real Core/AppContainer child and SDK broker traffic; not LibWeb/render acceptance. */
#include "native/sandbox/windows/Sandbox.h"
#include "native/sandbox/windows/helper/BrokerPolicy.h"
#include "native/sandbox/windows/helper/SDK.h"
#include <AK/Array.h>
#include <AK/ByteBuffer.h>
#include <AK/MemoryStream.h>
#include <AK/ScopeGuard.h>
#include <AK/Time.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <LibGfx/Bitmap.h>
#include <LibIPC/Attachment.h>
#include <LibIPC/Decoder.h>
#include <LibIPC/Encoder.h>
#include <LibIPC/HandleBroker.h>
#include <LibIPC/HandleBrokerMessages.h>
#include <LibIPC/HandleBrokerServer.h>
#include <LibIPC/Limits.h>
#include <LibIPC/Message.h>
#include <LibIPC/Transport.h>
#include <LibImageDecoderClient/Client.h>
#include <LibMain/Main.h>
#include <LibPlatformFeatures/SystemFacts.h>
#include <LibWebView/CompositorClient.h>
#include <LibWebView/ImageDecoderControlClient.h>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace B = Firefly::WindowsBroker;
namespace S = Firefly::WindowsSandbox;
namespace H = Firefly::WindowsHelpers;

namespace {
ErrorOr<void> require(bool condition, char const* label)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL Windows broker: %s\n", label);
        return Error::from_string_literal("Windows broker component failed");
    }

    return {};
}

ErrorOr<void> control_io(int fd, u64& value, bool writing)
{
    // The SDK's real duplex pipes use FILE_FLAG_OVERLAPPED. Keep the event and
    // request storage live until completion, including explicit timeout cancel.
    S::Handle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    TRY(require(static_cast<bool>(event), "control I/O event"));
    size_t offset = 0;
    while (offset < sizeof(value)) {
        ResetEvent(event.get());
        OVERLAPPED request {};
        request.hEvent = event.get();
        auto* bytes = reinterpret_cast<u8*>(&value) + offset;
        DWORD count = 0;
        auto size = static_cast<DWORD>(sizeof(value) - offset);
        auto completed = writing ? WriteFile(to_handle(fd), bytes, size, &count, &request)
                                 : ReadFile(to_handle(fd), bytes, size, &count, &request);
        if (!completed) {
            if (GetLastError() != ERROR_IO_PENDING)
                return Error::from_windows_error();
            if (WaitForSingleObject(event.get(), 15000) != WAIT_OBJECT_0) {
                CancelIoEx(to_handle(fd), &request);
                (void)GetOverlappedResult(to_handle(fd), &request, &count, TRUE);
                return Error::from_errno(ETIMEDOUT);
            }

            if (!GetOverlappedResult(to_handle(fd), &request, &count, FALSE))
                return Error::from_windows_error();
        }

        if (!count)
            return Error::from_errno(EPIPE);
        offset += count;
    }

    return {};
}

ErrorOr<void> send(int fd, u64 value)
{
    return control_io(fd, value, true);
}

ErrorOr<u64> receive(int fd)
{
    u64 value {};
    TRY(control_io(fd, value, false));
    return value;
}

ErrorOr<IPC::HandleBrokerResponse> exchange(
    IPC::Transport& transport, IPC::HandleBrokerRequest const& request)
{
    IPC::MessageBuffer buffer;
    IPC::Encoder encoder(buffer);
    TRY(encoder.encode(request));
    TRY(buffer.transfer_message(transport));
    Optional<ErrorOr<IPC::HandleBrokerResponse>> response;
    auto deadline = MonotonicTime::now() + Duration::from_seconds(10);
    while (!response.has_value()) {
        if (!transport.is_open() || MonotonicTime::now() >= deadline)
            return Error::from_errno(ETIMEDOUT);
        transport.wait_until_readable(deadline);
        auto shutdown
            = transport.read_as_many_messages_as_possible_without_blocking([&](auto&& message) {
                  if (response.has_value()) {
                      response = ErrorOr<IPC::HandleBrokerResponse> { Error::from_string_literal(
                          "Multiple broker responses") };
                      return;
                  }

                  FixedMemoryStream stream { message.bytes.bytes() };
                  IPC::Decoder decoder { stream, message.attachments };
                  response = decoder.decode<IPC::HandleBrokerResponse>();
              });
        if (shutdown == IPC::Transport::ShouldShutdown::Yes)
            transport.close();
    }

    return response.release_value();
}

ErrorOr<IPC::HandleBrokerResponse> park(
    IPC::Transport& transport, size_t count, u64 handle, u64 channel = 42)
{
    IPC::HandleBrokerRequest request;
    request.operation = IPC::HandleBrokerOperation::Park;
    request.channel = channel;
    for (size_t index = 0; index < count; ++index)
        request.handle_values.append(handle);
    return exchange(transport, request);
}

ErrorOr<void> forget(IPC::Transport& transport, u64 channel = 42)
{
    IPC::HandleBrokerRequest request;
    request.operation = IPC::HandleBrokerOperation::ForgetChannel;
    request.channel = channel;
    return require(TRY(exchange(transport, request)).error == 0, "forget succeeds");
}

ErrorOr<IPC::HandleBrokerResponse> redeem(IPC::Transport& transport, u64 token)
{
    IPC::HandleBrokerRequest request;
    request.operation = IPC::HandleBrokerOperation::Redeem;
    request.token = token;
    return exchange(transport, request);
}

DWORD handle_count()
{
    DWORD result = 0;
    VERIFY(GetProcessHandleCount(GetCurrentProcess(), &result));
    return result;
}

ErrorOr<void> await_handles(DWORD expected)
{
    auto deadline = MonotonicTime::now() + Duration::from_seconds(5);
    while (handle_count() != expected && MonotonicTime::now() < deadline)
        Sleep(1);
    return require(handle_count() == expected, "no leaked process handles");
}

ErrorOr<int> child(bool fill_only, bool blocked = false)
{
    TRY(H::verify_current());
    for (auto* name : { "FIREFLY_BROKER_SECRET", "LIBWEB_BROKER_SECRET", "LIBJS_BROKER_SECRET",
             "LIBGC_BROKER_SECRET", "LC_BROKER_SECRET" })
        TRY(require(
            std::getenv(name) == nullptr, "private ambient configuration was not forwarded"));
    int control = -1, broker_fd = -1;
    auto* environment = std::getenv("SOCKET_TAKEOVER");
    TRY(require(environment
            && std::sscanf(environment, "ipc:%d;handle-broker:%d", &control, &broker_fd) == 2,
        "actual private inherited transports"));
    auto transport = TRY(IPC::Transport::adopt_fd(broker_fd));
    S::Handle event(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    TRY(require(static_cast<bool>(event), "create actual native event"));
    u64 event_value = static_cast<u64>(to_fd(event.get()));
    TRY(require(B::valid_handle_value(event_value), "native source handle fits SDK descriptor"));
    if (fill_only) {
        if (blocked) {
            TRY(require(TRY(park(*transport, 0, 0)).error == ENOBUFS,
                "remote aggregate rejects a new actual sibling"));
            TRY(send(control, 1));
            TRY(receive(control));
        }

        for (size_t index = 0; index < B::owner_tokens; ++index)
            TRY(require(TRY(park(*transport, 0, 0)).error == 0, "remote token admission"));
        TRY(send(control, blocked ? 2 : 1));
        TRY(receive(control));
        return 0; // Deliberately leave Park records for genuine broker EOF cleanup.
    }

    auto warm = TRY(park(*transport, 1, event_value));
    auto copy = TRY(redeem(*transport, warm.token));
    TRY(require(!warm.error && !copy.error && copy.handle_values.size() == 1,
        "real native event roundtrip"));
    for (auto value : copy.handle_values)
        CloseHandle(to_handle(static_cast<int>(value)));
    TRY(send(control, 1));
    TRY(receive(control));
    for (auto invalid : { 0ULL, ~0ULL, ~0ULL - 1, 0x100000004ULL, 0x80000000ULL }) {
        IPC::HandleBrokerRequest request;
        request.operation = IPC::HandleBrokerOperation::Park;
        request.channel = 42;
        request.handle_values = { event_value, invalid };
        auto rejected = TRY(exchange(*transport, request));
        TRY(require(rejected.error == EBADF && rejected.token == 0,
            "pseudo/truncation rejected before duplication"));
    }
    // A valid first source and an invalid *representable* second source also
    // exercise real DuplicateHandle partial-failure cleanup in the Browser.
    auto invalid = 0x7ffffffc;
    DWORD flags = 0;
    TRY(require(
        !GetHandleInformation(to_handle(invalid), &flags), "independent invalid native source"));
    IPC::HandleBrokerRequest mixed;
    mixed.operation = IPC::HandleBrokerOperation::Park;
    mixed.channel = 42;
    mixed.handle_values = { event_value, static_cast<u64>(invalid) };
    TRY(require(TRY(exchange(*transport, mixed)).error != 0, "partial source duplication refused"));
    TRY(send(control, 2));
    auto bad_parent_token = TRY(receive(control));
    auto count = handle_count();
    TRY(require(TRY(redeem(*transport, bad_parent_token)).error != 0,
        "partial remote duplication refused"));
    TRY(await_handles(count));
    TRY(send(control, 3));

    for (size_t index = 0; index < B::owner_tokens; ++index)
        TRY(require(TRY(park(*transport, 0, 0)).error == 0, "finite token admission"));
    TRY(require(TRY(park(*transport, 0, 0)).error == ENOBUFS, "token quota refusal"));
    TRY(send(control, 4));
    TRY(receive(control));
    TRY(forget(*transport));
    auto recovered = TRY(park(*transport, 1, event_value));
    TRY(require(recovered.error == 0, "forget refunds real map admission"));
    TRY(forget(*transport));
    constexpr size_t packet_handles = IPC::MAX_MESSAGE_FD_COUNT_WITH_SHARED_MEMORY;
    static_assert(packet_handles * 2 <= B::owner_handles);
    auto first = TRY(park(*transport, packet_handles, event_value));
    TRY(require(first.error == 0 && TRY(park(*transport, packet_handles, event_value)).error == 0,
        "two full SDK attachment packets admitted"));
    TRY(require(
        TRY(park(*transport, B::owner_handles - packet_handles * 2, event_value)).error == 0,
        "exact handle quota admitted"));
    TRY(require(TRY(park(*transport, 1, event_value)).error == ENOBUFS,
        "handle quota refusal before duplication"));
    auto released = TRY(redeem(*transport, first.token));
    TRY(require(released.error == 0 && released.handle_values.size() == packet_handles,
        "actual redeem refunds handles"));
    for (auto value : released.handle_values)
        CloseHandle(to_handle(static_cast<int>(value)));
    TRY(require(TRY(park(*transport, 1, event_value)).error == 0, "capacity returns after redeem"));
    TRY(forget(*transport));
    TRY(send(control, 5));
    TRY(receive(control));
    auto orphan = TRY(park(*transport, 1, event_value));
    TRY(require(!orphan.error && orphan.token, "leave one owned pending attachment"));
    TRY(send(control, orphan.token));
    return 0;
}

struct Session {
    Core::Process process;
    int control;

    Session(Core::Process process, int control)
        : process(move(process))
        , control(control)
    {
    }

    ~Session()
    {
        if (control != -1)
            (void)Core::System::close(control);
    }
};

ErrorOr<std::unique_ptr<Session>> spawn(std::filesystem::path const& executable,
    IPC::HandleBrokerServer& server, Vector<ByteString> arguments = { "--child"sv },
    H::Role role = H::Role::WebContent)
{
    auto channel = TRY(Core::System::duplex_pipe());
    auto broker = TRY(Core::System::duplex_pipe());
    ScopeGuard cleanup([&] {
        for (auto fd : { channel[0], channel[1], broker[0], broker[1] }) {
            if (fd != -1)
                (void)Core::System::close(fd);
        }
    });
    auto text = executable.u8string();
    Core::ProcessSpawnOptions options;
    options.executable
        = ByteString(StringView(reinterpret_cast<char const*>(text.data()), text.size()));
    options.die_with_parent = true;
    options.arguments = arguments;
    options.environment = Vector<ByteString> { ByteString::formatted(
        "SOCKET_TAKEOVER=ipc:{};handle-broker:{}", channel[1], broker[1]) };
    for (auto* name : { "FIREFLY_BROKER_SECRET", "LIBWEB_BROKER_SECRET", "LIBJS_BROKER_SECRET",
             "LIBGC_BROKER_SECRET", "LC_BROKER_SECRET" })
        options.environment->append(ByteString::formatted("{}=must-not-enter-child", name));
    options.file_actions.append(
        Core::FileAction::DupFd { .write_fd = channel[1], .fd = channel[1] });
    options.file_actions.append(Core::FileAction::DupFd { .write_fd = broker[1], .fd = broker[1] });
    Optional<Core::Process> process;
    {
        H::Scope scope(role, options);
        process = TRY(Core::Process::spawn(options));
    }

    server.add_child(process->pid(), broker[0]);
    broker[0] = -1;
    auto result = std::make_unique<Session>(process.release_value(), channel[0]);
    channel[0] = -1;
    return result;
}

ErrorOr<void> exited(Session& session, IPC::LocalHandleBroker& broker)
{
    S::Handle process(OpenProcess(SYNCHRONIZE, FALSE, session.process.pid()));
    TRY(require(process && WaitForSingleObject(process.get(), 15000) == WAIT_OBJECT_0,
        "owned child exits under exact Job"));
    TRY(require(TRY(session.process.wait_for_termination()) == 0, "native child exit status"));
    auto deadline = MonotonicTime::now() + Duration::from_seconds(5);
    while (!broker.can_park_on_behalf_of(session.process.pid(), B::owner_handles)
        && MonotonicTime::now() < deadline)
        Sleep(1);
    TRY(require(broker.can_park_on_behalf_of(session.process.pid(), B::owner_handles),
        "EOF forgets actual owner records"));
    return {};
}

ErrorOr<void> decode_with_confined_helper(std::filesystem::path const& renderer,
    IPC::HandleBrokerServer& server, IPC::LocalHandleBroker& broker)
{
    auto decoder = renderer.parent_path() / L"ImageDecoder.exe";
    TRY(require(
        spawn(decoder, server).is_error(), "renderer role cannot launch the decoder image"));
    TRY(require(spawn(renderer, server, {}, H::Role::ImageDecoder).is_error(),
        "decoder role cannot launch the renderer image"));
    TRY(require(spawn(decoder, server, { "--disable-sandbox"sv }, H::Role::ImageDecoder).is_error(),
        "decoder cannot opt out of its child-side sandbox check"));

    auto session = TRY(spawn(decoder, server, {}, H::Role::ImageDecoder));
    auto control_transport = TRY(IPC::Transport::adopt_fd(session->control));
    session->control = -1;
    auto control
        = TRY(try_make_ref_counted<WebView::ImageDecoderControlClient>(move(control_transport)));
    auto channel = TRY(IPC::TransportHandle::create_pair());
    control->async_connect_new_client(channel.first);
    auto client = TRY(
        try_make_ref_counted<ImageDecoderClient::Client>(TRY(channel.second.create_transport())));
    // A 1 x 1 PNG with one opaque red pixel, encoded in RGB.
    static constexpr auto png = "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
                                "\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90\x77\x53"
                                "\xde\x00\x00\x00\x0c\x49\x44\x41\x54\x78\x9c\x63\xf8\xcf\xc0\x00"
                                "\x00\x03\x01\x01\x00\xc9\xfe\x92\xef\x00\x00\x00\x00\x49\x45\x4e"
                                "\x44\xae\x42\x60\x82"sv;
    auto decoded = TRY(client->decode_first_frame_synchronously(png.bytes()));
    TRY(require(decoded.bitmap->width() == 1 && decoded.bitmap->height() == 1
            && decoded.bitmap->get_pixel(0, 0) == Gfx::Color::Red,
        "actual sandboxed decoder returns the expected pixels"));
    client->shutdown();
    control->shutdown();
    TRY(exited(*session, broker));
    outln("PASS real AppContainer ImageDecoder PNG, exact role and natural exit");
    return {};
}

ErrorOr<void> connect_to_confined_compositor(std::filesystem::path const& renderer,
    IPC::HandleBrokerServer& server, IPC::LocalHandleBroker& broker)
{
    auto binary_directory = renderer.parent_path();
    auto compositor = binary_directory / L"Compositor.exe";
    TRY(require(
        spawn(compositor, server).is_error(), "renderer role cannot launch the compositor"));
    TRY(require(
        spawn(compositor, server, { "--resource-root"sv, "."sv }, H::Role::Compositor).is_error(),
        "compositor must use the exact sealed resource directory"));

    auto resource = (binary_directory.parent_path() / L"share" / L"Lagom").u8string();
    auto resource_root
        = ByteString(StringView(reinterpret_cast<char const*>(resource.data()), resource.size()));
    auto session = TRY(spawn(compositor, server,
        { "--test-mode"sv, "--resource-root"sv, move(resource_root) }, H::Role::Compositor));
    auto transport = TRY(IPC::Transport::adopt_fd(session->control));
    session->control = -1;
    auto control = TRY(try_make_ref_counted<WebView::CompositorClient>(move(transport)));
    auto response = TRY(control->try_connect_web_content());
    TRY(require(response.web_content_connection_id() >= 0,
        "actual compositor registers a WebContent channel"));
    auto channel = TRY(response.take_handle().create_transport());
    channel->close();
    control->shutdown();
    TRY(exited(*session, broker));
    outln("PASS real AppContainer Compositor control IPC and natural exit");
    return {};
}

ErrorOr<void> read_confined_system_report(
    std::filesystem::path const& renderer, IPC::HandleBrokerServer& server)
{
    auto probe = renderer.parent_path() / L"SystemProbe.exe";
    TRY(require(spawn(probe, server, {}, H::Role::SystemProbe).is_error(),
        "system facts probe cannot accept transport or ambient options"));

    auto pipe = TRY(Core::System::pipe2(O_CLOEXEC));
    ScopeGuard cleanup([&] {
        for (auto fd : pipe) {
            if (fd != -1)
                (void)Core::System::close(fd);
        }
    });
    auto input = TRY(Core::File::adopt_fd(pipe[0], Core::File::OpenMode::Read));
    pipe[0] = -1;
    Core::ProcessSpawnOptions options;
    auto path = probe.u8string();
    options.executable
        = ByteString(StringView(reinterpret_cast<char const*>(path.data()), path.size()));
    options.die_with_parent = true;
    options.environment = Vector<ByteString> {};
    {
        H::Scope scope(H::Role::SystemProbe, options);
        TRY(require(
            Core::Process::spawn(options).is_error(), "missing system report pipe is refused"));
    }

    options.file_actions.append(
        Core::FileAction::DupFd { .write_fd = pipe[1], .fd = STDOUT_FILENO });
    auto launch = [&]() -> ErrorOr<Core::Process> {
        H::Scope scope(H::Role::SystemProbe, options);
        return Core::Process::spawn(options);
    };

    auto process = TRY(launch());
    TRY(Core::System::close(pipe[1]));
    pipe[1] = -1;

    ByteBuffer report;
    Array<u8, 4096> buffer;
    for (;;) {
        auto bytes = TRY(input->read_some(buffer));
        if (bytes.is_empty())
            break;
        TRY(require(report.size() + bytes.size() <= 64 * KiB, "bounded native system report"));
        TRY(report.try_append(bytes));
    }

    auto facts = TRY(PlatformFeatures::SystemFacts::from_json(StringView { report.bytes() }));
    TRY(require(!facts.probe_failure.has_value(), "actual probe writes a system facts report"));
    TRY(require(TRY(process.wait_for_termination()) == 0, "system probe exits naturally"));
    outln("PASS real AppContainer SystemProbe report pipe and natural exit");
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    auto& loop = Core::EventLoop::initialize_for_current_thread();
    (void)loop;
    if (arguments.argc == 2 && StringView(arguments.argv[1]) == "--child"sv)
        return child(false);
    if (arguments.argc == 2 && StringView(arguments.argv[1]) == "--fill"sv)
        return child(true);
    if (arguments.argc == 2 && StringView(arguments.argv[1]) == "--blocked"sv)
        return child(true, true);
    TRY(require(arguments.argc == 2, "pass exact staged WebContent.exe path"));
    auto local = make<IPC::LocalHandleBroker>();
    auto& broker = *local;
    IPC::HandleBroker::install(move(local));
    auto server = IPC::HandleBrokerServer::create();
    auto path = StringView(arguments.argv[1]);
    std::filesystem::path executable(
        std::u8string(reinterpret_cast<char8_t const*>(path.characters_without_null_termination()),
            path.length()));
    TRY(require(spawn(executable, *server, { "--child"sv }, H::Role::Unsupported).is_error(),
        "unsupported helper role never falls back to an ordinary child"));
    TRY(decode_with_confined_helper(executable, *server, broker));
    TRY(connect_to_confined_compositor(executable, *server, broker));
    TRY(read_confined_system_report(executable, *server));
    auto session = TRY(spawn(executable, *server));
    TRY(require(TRY(receive(session->control)) == 1, "actual sandbox child/broker initialized"));
    auto baseline = handle_count();
    TRY(send(session->control, 1));
    TRY(require(TRY(receive(session->control)) == 2, "malformed requests completed"));
    TRY(await_handles(baseline));
    S::Handle event(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    HANDLE duplicate = nullptr;
    TRY(require(event
            && DuplicateHandle(GetCurrentProcess(), event.get(), GetCurrentProcess(), &duplicate, 0,
                FALSE, DUPLICATE_SAME_ACCESS),
        "parent native source"));
    Vector<IPC::Attachment> broken;
    broken.append(IPC::Attachment::from_fd(to_fd(duplicate)));
    DWORD flags = 0;
    TRY(require(!GetHandleInformation(to_handle(0x7ffffffc), &flags),
        "invalid parent descriptor independently checked"));
    broken.append(IPC::Attachment::from_fd(0x7ffffffc));
    auto token = TRY(broker.park(87, move(broken)));
    TRY(send(session->control, token));
    TRY(require(TRY(receive(session->control)) == 3, "remote partial rollback completed"));
    TRY(require(TRY(receive(session->control)) == 4, "real token limit reached"));
    TRY(require(!broker.can_park_on_behalf_of(session->process.pid(), 0),
        "actual map token quota charged"));
    TRY(send(session->control, 1));
    TRY(require(TRY(receive(session->control)) == 5, "handle quota and refunds completed"));
    TRY(send(session->control, 1));
    auto orphan = TRY(receive(session->control));
    TRY(exited(*session, broker));
    TRY(require(broker.redeem(orphan).is_error(), "disconnect removed outstanding bearer token"));
    session.reset();
    outln("PASS real Core AppContainer identity, pseudo/truncation rejection, partial-copy "
          "rollback and token/handle refunds");
    std::vector<std::unique_ptr<Session>> owners;
    for (size_t index = 0; index < B::remote_tokens / B::owner_tokens; ++index) {
        owners.push_back(TRY(spawn(executable, *server, { "--fill"sv })));
        TRY(require(
            TRY(receive(owners.back()->control)) == 1, "independent owner fills its own share"));
    }

    TRY(require(!broker.can_park_on_behalf_of(owners.front()->process.pid(), 0),
        "remote aggregate charged"));
    TRY(require(broker.can_park_on_behalf_of(Core::System::getpid(), B::remote_handles),
        "remote pressure never refuses Browser-origin send"));
    auto replacement = TRY(spawn(executable, *server, { "--blocked"sv }));
    TRY(require(TRY(receive(replacement->control)) == 1,
        "new sibling RPC refuses saturated remote aggregate"));
    TRY(send(owners.front()->control, 1));
    TRY(exited(*owners.front(), broker));
    owners.erase(owners.begin());
    TRY(send(replacement->control, 1));
    TRY(require(TRY(receive(replacement->control)) == 2,
        "sibling admission recovers after owner disconnect"));
    for (auto& owner : owners) {
        TRY(send(owner->control, 1));
        TRY(exited(*owner, broker));
    }

    TRY(send(replacement->control, 1));
    TRY(exited(*replacement, broker));
    outln("PASS actual multi-child remote Park bound, Browser-origin independence and disconnect "
          "recovery");
    return 0;
}
