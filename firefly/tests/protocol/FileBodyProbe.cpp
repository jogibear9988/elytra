/** Real checked file IO and streamed bytes to a separate native consumer. No window or JS. */
#include "../ipc/SpawnBytePeer.h"
#include "Arguments.h"
#include "FileRequestsProbe.h"
#include "native/ipc/bytes/Channel.h"
#include "native/protocol/files/FileBody.h"
#include "native/protocol/files/FilePacket.h"
#include "native/protocol/files/FileRequests.h"
#include "native/protocol/files/ResponseHead.h"
#include <AK/Array.h>
#include <AK/ScopeGuard.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Process.h>
#include <LibCore/System.h>
#include <LibIPC/StartupTransport.h>
#ifdef AK_OS_WINDOWS
#include <LibIPC/HandleBroker.h>
#include <fcntl.h>
#include <io.h>
#endif
#include <LibFileSystem/FileSystem.h>
#include <LibMain/Main.h>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

using Firefly::Bytes::Channel;
using Firefly::Protocols::FileBody;
using Firefly::Protocols::ResponseHead;

// Channel close callbacks are deferred. Keep only this token alive after the
// fixture's stack leaves; every borrowing callback must test it before access.
struct CallbackScope final : RefCounted<CallbackScope> {
    bool active { true };
};

static auto supplied_headers()
{
    return Array<HTTP::Header, 7> { {
        { "Cross-Origin-Opener-Policy", "same-origin" },
        { "Cross-Origin-Embedder-Policy", "require-corp" },
        { "Cache-Control", "no-cache, no-store" },
        { "Document-Policy", "js-profiling" },
        { "Set-Cookie", "first=1" },
        { "Set-Cookie", "second=2" },
        { "X-Bytes", "\x80\xff" },
    } };
}

static ErrorOr<void> until(Core::EventLoop& loop, Function<bool()> ready)
{
    Core::ElapsedTimer elapsed;
    elapsed.start();
    while (!ready()) {
        loop.pump(Core::EventLoop::WaitMode::PollForEvents);
        if (elapsed.elapsed_milliseconds() > 10000)
            return Error::from_string_literal("File stream transport timeout");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return {};
}

static ErrorOr<int> child(Core::EventLoop& loop, StringView server)
{
    // Bootstrap itself is synchronous, so the fixture needs a native watchdog.
    std::mutex lock;
    std::condition_variable wake;
    bool finished = false;
    std::thread watchdog([&] {
        std::unique_lock guard(lock);
        if (!wake.wait_for(guard, std::chrono::seconds(15), [&] { return finished; }))
            Core::Process::terminate_immediately(1);
    });
    ScopeGuard cleanup = [&] {
        {
            std::lock_guard guard(lock);
            finished = true;
        }

        wake.notify_one();
        watchdog.join();
    };

    bool done = false, bad = false;
    u64 received = 0;
    Optional<ResponseHead> metadata;
    auto callbacks = adopt_ref(*new CallbackScope);
    RefPtr<Channel> channel;
    channel = TRY(Channel::create(TRY(IPC::take_over_startup_transport(server)),
        {
            .on_data =
                [&, callbacks](ReadonlyBytes wire) {
                    if (!callbacks->active)
                        return;
                    auto decoded_packet = Firefly::Protocols::decode_file_packet(wire);
                    if (decoded_packet.is_error()) {
                        bad = true;
                        return;
                    }

                    auto const& packet = decoded_packet.value();
                    using Kind = Firefly::Protocols::FilePacketKind;
                    auto bytes = packet.payload;
                    if (packet.epoch != 101 || packet.request != 7 || done) {
                        bad = true;
                        return;
                    }

                    if (!metadata.has_value()) {
                        if (packet.kind != Kind::Metadata) {
                            bad = true;
                            return;
                        }

                        auto decoded = Firefly::Protocols::decode_file_response_head(
                            { reinterpret_cast<char const*>(bytes.data()), bytes.size() });
                        if (decoded.is_error()) {
                            bad = true;
                            return;
                        }

                        metadata = decoded.release_value();
                        if (metadata->request != 7 || metadata->headers.size() != 9) {
                            bad = true;
                            return;
                        }
                        // Compare preserved bytes/order to fixed independently supplied fields.
                        auto expected = supplied_headers();
                        for (size_t i = 0; i < expected.size(); ++i)
                            if (metadata->headers[i].name != expected[i].name
                                || metadata->headers[i].value != expected[i].value)
                                bad = true;
                        if (metadata->headers[7].name != "Content-Type"sv
                            || metadata->headers[7].value != "application/octet-stream"sv)
                            bad = true;
                    } else {
                        if (packet.kind == Kind::Cancel) {
                            done = true;
                        } else if (packet.kind == Kind::Complete) {
                            if (packet.offset != received)
                                bad = true;
                            done = true;
                            if (received != (metadata->head ? 0 : metadata->content_length))
                                bad = true;
                        } else {
                            if (packet.kind != Kind::Chunk || packet.offset != received
                                || metadata->head
                                || bytes.size() > metadata->content_length - received) {
                                bad = true;
                                return;
                            }

                            for (size_t i = 0; i < bytes.size(); ++i)
                                if (bytes[i] != static_cast<u8>(((received + i) * 37 + 11) & 255))
                                    bad = true;
                            received += bytes.size();
                        }
                    }

                    Array<u8, 16> acknowledgement {};
                    Array<u64, 2> values { static_cast<u64>(Core::System::getpid()), received };
                    for (size_t word = 0; word < 2; ++word)
                        for (size_t i = 0; i < 8; ++i)
                            acknowledgement[word * 8 + i]
                                = static_cast<u8>(values[word] >> (i * 8));
                    if (channel->send(acknowledgement).is_error())
                        bad = true;
                },
            .on_writable = {},
            .on_closed =
                [&, callbacks](auto) {
                    if (!callbacks->active)
                        return;
                    if (!done)
                        bad = true;
                },
        }));
    ScopeGuard close = [&] {
        callbacks->active = false;
        channel->close();
    };

    TRY(until(loop, [&] { return bad || (done && !channel->pending_send_count()); }));
    channel->close();
    return bad ? 1 : 0;
}

enum class TransferMode { Body, Head, Cancel, PeerLoss };

static ErrorOr<void> transfer(Core::EventLoop& loop, StringView path, TransferMode mode)
{
    using namespace Firefly::Protocols;
    bool head_only = mode == TransferMode::Head;
    auto owner = TRY(FileRequests::create(101));
    auto fields = supplied_headers();
    auto metadata = TRY(owner->begin(101, 7, path, head_only, fields, MonotonicTime::now()));
    auto executable = TRY(Core::System::current_executable_path());
    auto peer = TRY(spawn_byte_peer(executable));
    u64 observed = 0;
    size_t replies = 0;
    bool bad = false, peer_closed = false;
    auto callbacks = adopt_ref(*new CallbackScope);
    auto channel = TRY(Channel::create(move(peer->transport),
        {
            .on_data =
                [&, callbacks](ReadonlyBytes bytes) {
                    if (!callbacks->active)
                        return;
                    if (bytes.size() != 16) {
                        bad = true;
                        return;
                    }

                    Array<u64, 2> words {};
                    for (size_t word = 0; word < 2; ++word)
                        for (size_t i = 0; i < 8; ++i)
                            words[word] |= static_cast<u64>(bytes[word * 8 + i]) << (i * 8);
                    if (words[0] != static_cast<u64>(peer->process.pid()))
                        bad = true;
                    observed = words[1];
                    ++replies;
                },
            .on_writable = {},
            .on_closed =
                [&, callbacks](auto) {
                    if (!callbacks->active)
                        return;
                    peer_closed = true;
                    owner->revoke();
                },
        }));
    ScopeGuard close = [&] {
        callbacks->active = false;
        owner->revoke();
        channel->close();
    };

    auto send = [&](FilePacketKind kind, u64 offset, ReadonlyBytes bytes) -> ErrorOr<void> {
        auto packet = TRY(encode_file_packet({ kind, 101, 7, offset, bytes }));
        auto expected = replies + 1;
        TRY(channel->send(packet));
        TRY(until(loop, [&] { return bad || peer_closed || replies == expected; }));
        if (bad || replies != expected)
            return Error::from_string_literal("File response peer/sequence failed");
        return {};
    };

    auto encoded = TRY(encode_file_response_head(metadata));
    TRY(send(FilePacketKind::Metadata, 0, encoded.bytes()));
    if (observed)
        return Error::from_string_literal("File metadata arrived after body bytes");
    size_t chunks = 0;
    bool cancelled = false;
    bool killed = false;
    for (;;) {
        auto chunk = TRY(owner->pull(101, 7, file_chunk_limit, MonotonicTime::now()));
        if (!chunk.bytes.is_empty()) {
            TRY(send(FilePacketKind::Chunk, chunk.offset, chunk.bytes));
            ++chunks;
            if (observed != chunk.offset + chunk.bytes.size())
                return Error::from_string_literal("File bytes/PID/offset mismatch");
        }

        if (mode == TransferMode::PeerLoss && chunks == 1) {
            if (owner->pending() != 1)
                return Error::from_string_literal("Peer-loss fixture has no active file job");
            TRY(Core::Process::terminate_process(
                peer->process.pid(), Core::Process::TerminationMode::Forceful));
            TRY(until(loop, [&] { return peer_closed; }));
            if (!owner->is_revoked() || owner->pending()
                || !owner->pull(101, 7, 1, MonotonicTime::now()).is_error())
                return Error::from_string_literal("Peer loss did not revoke pending file job");
            killed = true;
            break;
        }

        if (mode == TransferMode::Cancel && chunks == 1) {
            if (!owner->cancel(101, 7))
                return Error::from_string_literal("File request cancel failed");
            TRY(send(FilePacketKind::Cancel, 0, {}));
            cancelled = true;
            break;
        }

        if (chunk.complete) {
            TRY(send(FilePacketKind::Complete, chunk.offset + chunk.bytes.size(), {}));
            break;
        }
    }

    auto status = TRY(peer->wait());
    if ((killed ? status == 0 : status != 0) || owner->pending()
        || (!cancelled && !killed && observed != (head_only ? 0 : metadata.content_length)))
        return Error::from_string_literal("File consumer did not finish/release its request");
    outln("PASS actual checked file {} to separate native child: bytes={} chunks={} parent={} "
          "child={}",
        killed          ? "peer loss"sv
            : cancelled ? "cancellation"sv
                        : "body"sv,
        observed, chunks, Core::System::getpid(), peer->process.pid());
    outln("PASS native child admitted versioned length/MIME/COOP/COEP/cache/document-policy and "
          "duplicate/obs-text header bytes before file chunks");
    outln("PASS request-managed frames preserve owner/request identity and exact chunk/completion "
          "offsets");
    return {};
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments native_arguments;
    TRY(native_arguments.initialize(arguments));
    arguments = native_arguments.arguments();
#ifdef AK_OS_WINDOWS
    // Decoder fixtures must see all original bytes, including a DOS EOF byte.
    if (_setmode(_fileno(stdin), _O_BINARY) == -1 || _setmode(_fileno(stdout), _O_BINARY) == -1)
        return Error::from_string_literal("Cannot establish binary native fixture streams");
#endif

    auto& loop = Core::EventLoop::initialize_for_current_thread();
    if (arguments.argc == 3 && arguments.strings[1] == "child"sv)
        return child(loop, arguments.strings[2]);
#ifdef AK_OS_WINDOWS
    IPC::HandleBroker::install(make<IPC::LocalHandleBroker>());
#endif
    if (arguments.argc != 3)
        return Error::from_string_literal("Expected a file body operation and absolute path");
    auto operation = arguments.strings[1];
    auto path = arguments.strings[2];
    if (operation == "arguments"sv) {
        outln("{}", path);
        return 0;
    }

    if (operation == "packet-encode"sv) {
        Array<u8, 3> bytes { 0, 128, 255 };
        auto packet = TRY(
            Firefly::Protocols::encode_file_packet({ Firefly::Protocols::FilePacketKind::Chunk,
                0x0102030405060708ull, 0x10203040u, 123, bytes }));
        if (fwrite(packet.data(), 1, packet.size(), stdout) != packet.size())
            return 1;
    } else if (operation == "packet-decode"sv) {
        std::string input(
            Firefly::Protocols::file_packet_header_size + Firefly::Protocols::file_chunk_limit + 1,
            '\0');
        std::cin.read(input.data(), static_cast<std::streamsize>(input.size()));
        input.resize(static_cast<size_t>(std::cin.gcount()));
        auto packet = TRY(Firefly::Protocols::decode_file_packet(
            { reinterpret_cast<u8 const*>(input.data()), input.size() }));
        auto encoded = TRY(Firefly::Protocols::encode_file_packet(packet));
        if (fwrite(encoded.data(), 1, encoded.size(), stdout) != encoded.size())
            return 1;
    } else if (operation == "decode"sv) {
        std::string input(Firefly::Protocols::response_head_limit + 1, '\0');
        std::cin.read(input.data(), static_cast<std::streamsize>(input.size()));
        input.resize(static_cast<size_t>(std::cin.gcount()));
        auto metadata
            = TRY(Firefly::Protocols::decode_file_response_head({ input.data(), input.size() }));
        auto encoded = TRY(Firefly::Protocols::encode_file_response_head(metadata));
        outln("{}", encoded);
    } else if (operation == "metadata"sv || operation == "metadata-head"sv) {
        bool head = operation == "metadata-head"sv;
        auto body = TRY(FileBody::open(path, head));
        auto fields = supplied_headers();
        auto metadata = TRY(Firefly::Protocols::make_file_response_head(
            7, body->content_length(), head, path, fields));
        outln("{}", TRY(Firefly::Protocols::encode_file_response_head(metadata)));
    } else if (operation == "transfer"sv || operation == "transfer-head"sv
        || operation == "transfer-cancel"sv || operation == "transfer-peer-loss"sv) {
        auto mode = operation == "transfer-head"sv ? TransferMode::Head
            : operation == "transfer-cancel"sv     ? TransferMode::Cancel
            : operation == "transfer-peer-loss"sv  ? TransferMode::PeerLoss
                                                   : TransferMode::Body;
        TRY(transfer(loop, path, mode));
        // Exercise deferred close delivery after transfer's borrowed stack left.
        loop.pump(Core::EventLoop::WaitMode::PollForEvents);
    } else if (operation == "metadata-policy"sv) {
        using namespace Firefly::Protocols;
        for (auto const& header : Array<HTTP::Header, 4> {
                 { { "Content-Length", "17" }, { "Transfer-Encoding", "chunked" },
                     { "Invalid Name", "v" }, { "X-Header", "a\r\nb" } } }) {
            if (!make_file_response_head(7, 17, false, path, { &header, 1 }).is_error())
                return 1;
        }

        if (!make_file_response_head(0, 17, false, path).is_error()
            || !make_file_response_head(7, file_size_limit + 1, false, path).is_error())
            return 1;
        Vector<HTTP::Header> supplied;
        for (size_t i = 0; i < 62; ++i)
            supplied.append({ "X-Test", "value" });
        auto full = TRY(make_file_response_head(7, 17, false, path, supplied));
        if (full.headers.size() != 64)
            return 1;
        supplied.append({ "Content-Type", "text/plain" });
        full = TRY(make_file_response_head(7, 17, false, path, supplied));
        if (full.headers.size() != 64)
            return 1;
        supplied.append({ "X-Extra", "v" });
        if (!make_file_response_head(7, 17, false, path, supplied).is_error())
            return 1;
        Array<HTTP::Header, 2> duplicate_type { { { "Content-Type", "text/html" },
            { "content-type", "text/plain" } } };
        if (!make_file_response_head(7, 17, false, path, duplicate_type).is_error())
            return 1;
        auto raw = TRY(ByteBuffer::create_uninitialized(response_head_limit + 1));
        __builtin_memset(raw.data(), 'v', raw.size());
        HTTP::Header raw_overflow { "X-Large",
            ByteString(StringView { reinterpret_cast<char const*>(raw.data()), raw.size() }) };
        if (!make_file_response_head(7, 17, false, path, { &raw_overflow, 1 }).is_error())
            return 1;
        auto escaped = TRY(ByteBuffer::create_uninitialized(6000));
        __builtin_memset(escaped.data(), 1, escaped.size());
        HTTP::Header expansion { "X-Escaped",
            ByteString(
                StringView { reinterpret_cast<char const*>(escaped.data()), escaped.size() }) };
        auto huge_wire = TRY(make_file_response_head(7, 17, false, path, { &expansion, 1 }));
        if (!encode_file_response_head(huge_wire).is_error())
            return 1;
        outln("PASS native metadata builder rejects framing overrides, invalid identity/fields, "
              "duplicate type and raw/encoded budgets");
    } else if (operation == "requests"sv) {
        TRY(check_file_requests(path));
    } else if (operation == "head"sv) {
        auto body = TRY(FileBody::open(path, true));
        if (body->state() != FileBody::State::Complete || !TRY(body->read()).is_empty()
            || body->transferred())
            return 1;
        outln("HEAD length={}", body->content_length());
    } else if (operation == "cancel"sv) {
        if (!FileBody::open(""sv).is_error() || !FileBody::open("/nul\0path"sv).is_error())
            return 1;
        auto body = TRY(FileBody::open(path));
        if (!body->read(0).is_error()
            || !body->read(Firefly::Protocols::file_chunk_limit + 1).is_error())
            return 1;
        auto prefix = TRY(body->read(17));
        if (prefix.size() != 17)
            return 1;
        body->cancel();
        body->cancel();
        if (body->state() != FileBody::State::Cancelled || !body->read().is_error())
            return 1;
        outln("PASS cancellation and invalid chunk budgets");
    } else if (operation == "grow"sv) {
        auto body = TRY(FileBody::open(path));
        auto original = body->content_length();
        auto writer = TRY(
            Core::File::open(path, Core::File::OpenMode::Write | Core::File::OpenMode::Append));
        Array<u8, 9> tail {};
        TRY(writer->write_until_depleted(tail));
        writer->close();
        while (!TRY(body->read()).is_empty()) { }
        if (body->transferred() != original || body->content_length() != original)
            return 1;
        outln("PASS growing file stays bounded to advertised length");
    } else if (operation == "replace"sv) {
        auto body = TRY(FileBody::open(path));
        auto saved = ByteString::formatted("{}.old", path);
        TRY(FileSystem::rename_without_replacing(saved, path));
        auto writer = TRY(Core::File::open(path, Core::File::OpenMode::Write));
        Array<u8, 17> changed {};
        changed.fill(255);
        TRY(writer->write_until_depleted(changed));
        writer->close();
        auto original = TRY(body->read(17));
        if (original.size() != 17)
            return 1;
        for (size_t i = 0; i < original.size(); ++i)
            if (original[i] != static_cast<u8>((i * 37 + 11) & 255))
                return 1;
        auto replacement = TRY(FileBody::open(path));
        auto replaced = TRY(replacement->read());
        if (replaced.bytes() != ReadonlyBytes { changed.data(), changed.size() })
            return 1;
        body->cancel();
        outln("PASS descriptor remains bound to original file after path replacement");
    } else if (operation == "truncate"sv) {
        auto body = TRY(FileBody::open(path));
        auto writer = TRY(Core::File::open(path, Core::File::OpenMode::Write));
        writer->close();
        if (!body->read().is_error() || body->state() != FileBody::State::Failed)
            return 1;
        outln("PASS truncated file is an error, not successful advertised-length EOF");
    } else if (operation == "refuse"sv) {
        if (!FileBody::open(path).is_error())
            return 1;
        outln("PASS file path/type/size refused");
    } else {
        return 1;
    }

    return 0;
}
