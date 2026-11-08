/** Execute real global fetch in a native child; only its parent opens the authorized file. */
#include "../ipc/SpawnBytePeer.h"
#include "Arguments.h"
#include "loading/protocol/Client.h"
#include "loading/protocol/Dispatch.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/files/Server.h"
#include <AK/Array.h>
#include <AK/ScopeGuard.h>
#include <LibCore/ElapsedTimer.h>
#include <LibCore/EventLoop.h>
#include <LibCore/System.h>
#include <LibIPC/StartupTransport.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibURL/Parser.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/Platform/FontPlugin.h>
#ifdef AK_OS_WINDOWS
#include <LibIPC/HandleBroker.h>
#endif
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace PF = Firefly::ProtocolFetch;
namespace P = Firefly::Protocols;

namespace {
GC::Ref<Web::DOM::Document> create_document()
{
    GC::Root initial { Web::Bindings::create_a_principal_javascript_realm() };
    auto& page = Web::Bindings::principal_host_defined_page(*initial);
    auto traversable = Web::HTML::LocalTraversableNavigable::create_a_new_top_level_traversable(
        page, nullptr, {});
    page.set_top_level_traversable(traversable);
    auto document = traversable->active_document();
    auto parsed_url = URL::Parser::basic_parse("app://bundle/index.html"sv);
    VERIFY(parsed_url.has_value());
    auto url = parsed_url.release_value();
    document->set_url(url);
    document->set_origin(url.origin());
    auto& settings = document->relevant_settings_object();
    settings.creation_url = url;
    settings.top_level_creation_url = url;
    settings.top_level_origin = url.origin();
    return *document;
}

ErrorOr<JS::Value> run(JS::Realm& realm, Utf16View source)
{
    Web::HTML::TemporaryExecutionContext context { realm,
        Web::HTML::TemporaryExecutionContext::CallbacksEnabled::Yes };
    auto parsed = JS::Script::parse(Utf16String::from_utf16(source), realm);
    if (parsed.is_error())
        return Error::from_string_literal("Invalid fetch channel test script");
    GC::Root script { parsed.release_value() };
    auto result = realm.vm().run(*script);
    if (result.is_error())
        return Error::from_string_literal("Fetch channel test script threw synchronously");
    return result.release_value();
}

ErrorOr<void> wait(JS::Realm& realm, Function<bool()> predicate, int seconds = 10)
{
    auto timer = Core::Timer::create_single_shot(AK::Duration::from_seconds(seconds), [] { });
    timer->start();
    Web::HTML::main_thread_event_loop().spin_until(
        GC::create_function(realm.heap(), [&] { return predicate() || !timer->is_active(); }));
    if (!predicate()) {
        auto detail = run(realm, u"globalThis.channelResult"sv);
        if (!detail.is_error())
            warnln("JavaScript channel result: {}",
                detail.value().to_utf16_string_without_side_effects());
        return Error::from_string_literal("JavaScript fetch channel condition did not settle");
    }

    realm.vm().heap().collect_garbage();
    return {};
}

ErrorOr<void> wait_script(JS::Realm& realm, Utf16View expression, int seconds = 10)
{
    auto text = Utf16String::from_utf16(expression);
    return wait(
        realm,
        [&] {
            auto result = run(realm, text);
            return !result.is_error() && result.value().is_boolean() && result.value().as_bool();
        },
        seconds);
}

ErrorOr<int> child(StringView bootstrap, StringView mode)
{
    // Bootstrap is synchronous; use a native watchdog as well as JS wait timers.
    std::mutex lock;
    std::condition_variable wake;
    bool finished = false;
    std::thread watchdog([&] {
        std::unique_lock guard(lock);
        if (!wake.wait_for(guard, std::chrono::seconds(40), [&] { return finished; }))
            Core::Process::terminate_immediately(1);
    });
    ScopeGuard joined = [&] {
        {
            std::lock_guard guard(lock);
            finished = true;
        }

        wake.notify_one();
        watchdog.join();
    };

    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    Array schemes { P::OriginScheme { "app"_string, true, true } };
    TRY(P::install_origin_schemes(schemes));
    GC::Root document { create_document() };
    auto& realm = document->relevant_settings_object().realm();
    RefPtr<PF::Client> client
        = TRY(PF::Client::create(TRY(IPC::take_over_startup_transport(bootstrap)), 101, *document));
    auto weak = client->make_weak_ptr();
    auto registration = TRY(PF::Registration::install(
        [weak](auto const& params) -> ErrorOr<NonnullRefPtr<PF::Response>> {
            if (auto owner = weak.strong_ref())
                return owner->fetch(params);
            return Error::from_string_literal("Native resource client has ended");
        }));
    ScopeGuard revoke = [&] {
        registration->revoke();
        if (client)
            client->revoke();
    };

    if (mode == "body"sv) {
        TRY(run(realm, uR"JS(
            globalThis.channelResult='pending';
            Promise.all([
                fetch('app://bundle/body').then(async r=>{
                    if(r.status!==200 || r.headers.get('cache-control')!=='no-store' || r.headers.has('set-cookie')) throw Error('headers');
                    const bytes=new Uint8Array(await r.arrayBuffer());
                    if(bytes.length!==4194323 || !bytes.every((v,i)=>v===((i*37+11)&255))) throw Error('bytes');
                }),
                fetch('app://bundle/body',{method:'HEAD'}).then(async r=>{
                    if(r.headers.get('content-length')!=='4194323' || (await r.arrayBuffer()).byteLength!==0) throw Error('HEAD');
                }),
                fetch('app://bundle/denied').then(()=>{throw Error('unauthorized')},()=>{})
            ]).then(()=>channelResult='ok',e=>channelResult='error:'+e.message);
        )JS"sv));
        TRY(wait_script(realm, u"channelResult==='ok'"sv));
    } else if (mode == "stream"sv) {
        TRY(run(realm, uR"JS(
            globalThis.channelResult='pending';
            Promise.all([
                fetch('app://bundle/body').then(async r=>{
                    if(r.status!==404 || r.headers.get('content-type')!=='application/octet-stream' || r.headers.has('content-length') || r.headers.has('set-cookie')) throw Error('stream headers');
                    const reader=r.body.getReader();
                    let offset=0;
                    for (;;) {
                        const item=await reader.read();
                        if(item.done) break;
                        if(item.value.length>65536 || !item.value.every((v,i)=>v===(((offset+i)*37+11)&255))) throw Error('stream bytes');
                        offset+=item.value.length;
                    }
                    if(offset!==4194323) throw Error('stream EOF');
                }),
                fetch('app://bundle/body',{method:'HEAD'}).then(async r=>{
                    if(r.status!==404 || r.headers.get('content-length')!=='4194323' || (await r.arrayBuffer()).byteLength!==0) throw Error('stream HEAD');
                }),
                fetch('app://bundle/denied').then(()=>{throw Error('unauthorized')},()=>{})
            ]).then(()=>channelResult='ok',e=>channelResult='error:'+e.message);
        )JS"sv));
        TRY(wait_script(realm, u"channelResult==='ok'"sv));
    } else if (mode == "quota"sv) {
        TRY(run(realm, uR"JS(
            globalThis.channelResult='pending';
            Promise.allSettled(Array.from({length:9},()=>fetch('app://bundle/body').then(r=>r.arrayBuffer())))
                .then(results=>{
                    const accepted=results.filter(r=>r.status==='fulfilled');
                    const denied=results.filter(r=>r.status==='rejected');
                    const valid=accepted.length===8 && denied.length===1 && accepted.every(r=>{
                        const bytes=new Uint8Array(r.value);
                        return bytes.length===4194323 && bytes[0]===11 && bytes[4194322]===165;
                    });
                    channelResult=valid?'quota':'bad';
                },()=>channelResult='bad');
        )JS"sv));
        TRY(wait_script(realm, u"channelResult==='quota'"sv));
    } else if (mode == "scope"sv) {
        GC::Root other { create_document() };
        auto& other_realm = other->relevant_settings_object().realm();
        TRY(run(other_realm,
            u"globalThis.channelResult='pending'; fetch('app://bundle/body').then(()=>channelResult='bad',()=>channelResult='denied');"sv));
        TRY(wait_script(other_realm, u"channelResult==='denied'"sv));
        if (client->pending())
            return Error::from_string_literal("Foreign same-origin document acquired channel jobs");
    } else if (mode == "abort-early"sv) {
        TRY(run(realm,
            u"globalThis.channelResult='pending';const controller=new AbortController();fetch('app://bundle/body',{signal:controller.signal}).then(()=>channelResult='bad',e=>channelResult=e.name==='AbortError'?'aborted':'bad');controller.abort();"sv));
        TRY(wait_script(realm, u"channelResult==='aborted'"sv));
    } else if (mode == "abort"sv) {
        TRY(run(realm, uR"JS(
            globalThis.channelResult='pending';
            const controller=new AbortController();
            fetch('app://bundle/body',{signal:controller.signal}).then(async r=>{
                controller.abort();
                await r.arrayBuffer();
            }).then(()=>channelResult='bad',e=>channelResult=e.name==='AbortError'?'aborted':'error:'+e.name);
        )JS"sv));
        TRY(wait_script(realm, u"channelResult==='aborted'"sv));
    } else if (mode == "reader-cancel"sv) {
        TRY(run(realm, uR"JS(
            globalThis.channelResult='pending';
            fetch('app://bundle/body').then(async r=>{
                const reader=r.body.getReader();
                const chunk=await reader.read();
                if(chunk.done || chunk.value.byteLength!==65536) throw Error('first chunk');
                await reader.cancel();
            }).then(()=>channelResult='cancelled',e=>channelResult='error:'+e.message);
        )JS"sv));
        TRY(wait_script(realm, u"channelResult==='cancelled'"sv));
    } else {
        TRY(run(realm,
            u"globalThis.channelResult='pending';globalThis.channelHeaders=false;fetch('app://bundle/body').then(r=>{channelHeaders=true;return r.arrayBuffer()}).then(()=>channelResult='bad',()=>channelResult='failed');"sv));
        if (mode == "revoke"sv || mode == "drop"sv) {
            TRY(wait_script(realm, u"channelHeaders===true"sv));
            if (client->pending() != 1)
                return Error::from_string_literal("Revocation test has no pending fetch");
            if (mode == "drop"sv) {
                client = nullptr;
                if (weak.strong_ref())
                    return Error::from_string_literal(
                        "Resource client retained by a native callback cycle");
            } else
                client->revoke();
        }

        Core::ElapsedTimer lifetime;
        lifetime.start();
        TRY(wait_script(realm, u"channelResult==='failed'"sv, mode == "silence"sv ? 35 : 10));
        if (mode == "silence"sv && lifetime.elapsed_milliseconds() < 29000)
            return Error::from_string_literal("Client deadline did not wait for a silent peer");
        if (client && !client->is_revoked())
            return Error::from_string_literal(
                "Transport failure did not revoke the native fetch client");
    }

    if (client)
        TRY(wait(realm, [&] { return !client->pending(); }));
    TRY(wait_script(realm,
        u"['process','require','firefly','electron'].every(k=>typeof globalThis[k]==='undefined')"sv));
    outln("PASS actual JavaScript fetch over native process channel {}: body/stream settlement and "
          "absent page capabilities",
        mode);
    return 0;
}

ErrorOr<void> until(Function<bool()> predicate, int seconds = 20)
{
    Core::ElapsedTimer elapsed;
    elapsed.start();
    while (!predicate()) {
        Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
        if (elapsed.elapsed_milliseconds() > seconds * 1000)
            return Error::from_string_literal("Native fetch channel parent timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return {};
}

struct RawState final : RefCounted<RawState> {
    bool active { true }, closed { false }, bad { false };
    size_t calls { 0 };
};

ErrorOr<void> parent(StringView path, StringView mode)
{
    auto executable = TRY(Core::System::current_executable_path());
    Array<ByteString, 1> extra { ByteString(mode) };
    auto peer = TRY(spawn_byte_peer(executable, extra));
    if (mode.is_one_of("malformed"sv, "epoch"sv, "future"sv, "silence"sv)) {
        auto state = adopt_ref(*new RawState);
        RefPtr<Firefly::Bytes::Channel> channel;
        channel = TRY(Firefly::Bytes::Channel::create(move(peer->transport),
            {
                .on_data =
                    [&, state](ReadonlyBytes bytes) {
                        if (!state->active)
                            return;
                        ++state->calls;
                        auto request = P::decode_file_request(bytes);
                        if (request.is_error() || request.value().kind != P::FileRequestKind::Get
                            || request.value().epoch != 101 || request.value().request != 1) {
                            state->bad = true;
                            return;
                        }

                        if (mode == "silence"sv)
                            return;
                        auto wire = mode == "malformed"sv
                            ? ByteBuffer::copy("bad"sv.bytes())
                            : P::encode_file_packet(
                                  { P::FilePacketKind::Complete, mode == "epoch"sv ? 102u : 101u,
                                      mode == "future"sv ? 2u : 1u, 0, {} });
                        if (wire.is_error() || channel->send(wire.value()).is_error())
                            state->bad = true;
                    },
                .on_writable = {},
                .on_closed =
                    [state](auto) {
                        if (state->active)
                            state->closed = true;
                    },
            }));
        ScopeGuard close = [&] {
            state->active = false;
            channel->close();
        };

        TRY(until([&] { return state->bad || state->closed; }, mode == "silence"sv ? 40 : 20));
        if (state->bad || state->calls != 1)
            return Error::from_string_literal("Malformed-response native control failed");
    } else if (mode == "stream"sv) {
        size_t calls = 0, pulls = 0, cancellations = 0;
        auto server = TRY(P::FileServer::create_responses(move(peer->transport), 101,
            [&](P::FileServer::AuthorizationRequest request,
                P::FileServer::ResponseCompletion complete) {
                ++calls;
                if (request.url != "app://bundle/body"sv) {
                    (void)complete(Error::from_errno(EACCES));
                    return;
                }

                P::StreamResponseHead head { request.id, 404, {}, request.head, request.head,
                    { { "Content-Type", "application/octet-stream" },
                        { "Set-Cookie", "fixture=private" } } };
                if (request.head) {
                    head.content_length = 4194323;
                    head.headers.append({ "Content-Length", "4194323" });
                }

                P::AuthorizedStream stream { move(head),
                    [&, offset = u64 { 0 }](size_t maximum, P::StreamCompletion complete) mutable {
                        ++pulls;
                        auto count = min<u64>(maximum, 4194323 - offset);
                        auto bytes = MUST(ByteBuffer::create_uninitialized(count));
                        for (size_t i = 0; i < count; ++i)
                            bytes[i] = static_cast<u8>((offset + i) * 37 + 11);
                        offset += count;
                        Core::deferred_invoke([complete = move(complete), bytes = move(bytes),
                                                  done = offset == 4194323]() mutable {
                            (void)complete(P::StreamChunk { move(bytes), done });
                        });
                    },
                    [&] { ++cancellations; } };
                (void)complete(P::FileServer::AuthorizedResponse { move(stream) });
            }));
        ScopeGuard close = [&] { server->revoke(); };
        TRY(until([&] { return server->is_revoked(); }));
        if (server->pending() || server->files() || calls != 3 || pulls < 65 || cancellations != 1)
            return Error::from_string_literal(
                "Modern channel lost body credit/HEAD cancellation/quota cleanup");
    } else {
        size_t calls = 0;
        auto server = TRY(P::FileServer::create(
            move(peer->transport), 101, [&](StringView url, bool) -> ErrorOr<P::AuthorizedFile> {
                ++calls;
                if (url != "app://bundle/body"sv)
                    return Error::from_string_literal(
                        "URL not authorized for native test document");
                return P::AuthorizedFile { ByteString(path),
                    { { "Cache-Control", "no-store" }, { "Set-Cookie", "fixture=private" } } };
            }));
        ScopeGuard close = [&] { server->revoke(); };
        if (mode == "peerloss"sv) {
            TRY(until([&] { return server->files() == 1 || server->is_revoked(); }));
            if (server->files() != 1)
                return Error::from_string_literal("Peer-loss test has no pending native file");
            server->revoke();
        }

        TRY(until([&] { return server->is_revoked(); }));
        if (server->pending() || server->files()
            || calls
                != static_cast<size_t>(mode == "body"sv              ? 3
                        : mode == "quota"sv                          ? 8
                        : mode.is_one_of("scope"sv, "abort-early"sv) ? 0
                                                                     : 1))
            return Error::from_string_literal("Native fetch admission/file cleanup mismatch");
    }

    if (TRY(peer->wait()) != 0)
        return Error::from_string_literal("JavaScript fetch consumer process failed");
    outln("PASS main-side file ownership and reaped JavaScript child {}", mode);
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments native_arguments;
    TRY(native_arguments.initialize(arguments));
    arguments = native_arguments.arguments();
    Core::EventLoop::initialize_for_current_thread();
    if (arguments.argc == 4 && arguments.strings[1] == "child"sv)
        return child(arguments.strings[2], arguments.strings[3]);
#ifdef AK_OS_WINDOWS
    IPC::HandleBroker::install(make<IPC::LocalHandleBroker>());
#endif
    if (arguments.argc != 3
        || !arguments.strings[1].is_one_of("body"sv, "stream"sv, "quota"sv, "scope"sv, "abort"sv,
            "abort-early"sv, "reader-cancel"sv, "revoke"sv, "drop"sv, "peerloss"sv, "malformed"sv,
            "epoch"sv, "future"sv, "silence"sv))
        return Error::from_string_literal(
            "Expected fetch-channel mode and native-authorized file path");
    TRY(parent(arguments.strings[2], arguments.strings[1]));
    return 0;
}
