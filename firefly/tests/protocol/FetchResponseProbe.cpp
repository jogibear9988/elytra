/*
 * Copyright (c) 2026-present, the Ladybird developers.
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Read-request recording follows Tests/LibWeb/TestFetchBodyStream.cpp. Exercise
 * actual LibWeb bodies/controllers with our real file jobs, without a desktop.
 */
#include "Arguments.h"
#include "FetchDispatchProbe.h"
#include "loading/protocol/Response.h"
#include "loading/web-request/Gate.h"
#include "native/protocol/files/FilePacket.h"
#include "native/protocol/files/FileRequests.h"
#include <AK/ScopeGuard.h>
#include <AK/WeakPtr.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Timer.h>
#include <LibGC/Function.h>
#include <LibHTTP/HeaderList.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/VM.h>
#include <LibMain/Main.h>
#include <LibURL/Parser.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/Fetch/Fetching/BodyStreamPullSource.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/Loader/LoaderConfig.h>
#include <LibWeb/Platform/FontPlugin.h>
#include <LibWeb/Streams/ReadableStreamDefaultReader.h>

namespace Infra = Web::Fetch::Infrastructure;
namespace Engine = Web::Fetch::Engine;
namespace Protocols = Firefly::Protocols;
using Receiver = Firefly::ProtocolFetch::Response;
using Kind = Protocols::FilePacketKind;

namespace {
class Read final : public Web::Streams::ReadRequest {
    GC_CELL(Read, Web::Streams::ReadRequest);
    GC_DECLARE_ALLOCATOR(Read);

public:
    virtual void on_chunk(JS::Value value) override
    {
        auto& array = as<JS::Uint8Array>(value.as_object());
        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);
        bytes = MUST(array.viewed_array_buffer()->copy_to_byte_buffer(
            array.byte_offset(), JS::typed_array_byte_length(witness)));
        settled = true;
    }

    virtual void on_close() override
    {
        settled = closed = true;
    }

    virtual void on_error(JS::Value) override
    {
        settled = errored = true;
    }

    ByteBuffer bytes;
    bool settled { false }, closed { false }, errored { false };
};

GC_DEFINE_ALLOCATOR(Read);

struct State : RefCounted<State> {
    // Response/byte channels are native transport data in the current SDK.
    GC_HOLDS_NO_CELL_EDGES;
    u64 credit { 0 };
    size_t cancellations { 0 };
    size_t completions { 0 };
    RefPtr<Infra::Response> response;
    WeakPtr<Receiver> receiver;
    bool reenter { false };
    size_t credit_returns { 0 };
};

struct Fetch {
    Infra::FetchControllerRegistry registry;
    GC::Root<Infra::FetchController> controller { Infra::FetchController::create(registry) };
    NonnullRefPtr<Infra::FetchParams> params;
    NonnullRefPtr<State> state { adopt_ref(*new State) };
    RefPtr<Receiver> receiver;
    GC::Root<Web::Fetch::Fetching::PendingResponse> pending;

    explicit Fetch(bool head = false, u64 maximum_length = Protocols::file_size_limit,
        Web::DOM::Document* document = nullptr)
        : params(make_params(*controller, head, document))
    {
        auto shared = state;
        receiver = MUST(Receiver::create(*params, 91, 7,
            {
                .credit =
                    [shared](u64 count) {
                        shared->credit += count;
                        if (shared->reenter) {
                            if (auto owner = shared->receiver.strong_ref())
                                owner->stop();
                        }

                        ++shared->credit_returns;
                    },
                .cancel = [shared] { ++shared->cancellations; },
                .completed = [shared] { ++shared->completions; },
            },
            maximum_length));
        pending = receiver->pending();
        state->receiver = receiver->make_weak_ptr();
        receiver->when_loaded(GC::create_function(GC::Heap::the(), GC::captures(shared),
            [](auto& shared, NonnullRefPtr<Infra::Response> response) {
                shared->response = move(response);
            }));
    }

    static NonnullRefPtr<Infra::FetchParams> make_params(
        Infra::FetchController& controller, bool head, Web::DOM::Document* document)
    {
        controller.set_task_destination(GC::Ref<JS::Object> { Web::HTML::current_global_object() });
        auto request = Infra::Request::create();
        auto url = URL::Parser::basic_parse("https://example.test/resource"sv);
        VERIFY(url.has_value());
        request->set_url_list({ url.release_value() });
        request->set_method(head ? "HEAD" : "GET");
        if (document) {
            request->set_client(
                Infra::Request::ClientType { document->relevant_settings_object().address() });
            request->set_origin(document->origin());
        }
        auto algorithms = Infra::FetchAlgorithms::create(
            Infra::FetchAlgorithms::Input { Engine::BodyIntent::DeliverStreamToConsumerAgent });
        return Infra::FetchParams::create(controller, request, algorithms,
            Infra::FetchTimingInfo::create(), Web::current_loader_config());
    }

    ErrorOr<void> send(Kind kind, u64 offset, ReadonlyBytes bytes = {})
    {
        auto frame = TRY(Protocols::encode_file_packet({ kind, 91, 7, offset, bytes }));
        return receiver->receive(frame);
    }

    ErrorOr<void> metadata(u64 length, bool head = false)
    {
        auto info = TRY(Protocols::make_file_response_head(7, length, head, "/fixture.mjs"sv));
        auto encoded = TRY(Protocols::encode_file_response_head(info));
        return send(Kind::Metadata, 0, encoded.bytes());
    }

    ~Fetch()
    {
        if (receiver)
            receiver->stop();
    }
};

void pump()
{
    for (size_t i = 0; i < 4; ++i)
        Core::EventLoop::current().pump(Core::EventLoop::WaitMode::PollForEvents);
}

ErrorOr<void> check(bool condition, StringView message)
{
    if (!condition)
        return Error::from_string_view(message);
    return {};
}

GC::Ref<Read> read(JS::Realm& realm, Web::Streams::ReadableStreamDefaultReader& reader)
{
    auto request = realm.heap().allocate<Read>();
    reader.read_a_chunk(*request);
    if (!request->settled) {
        auto timer = Core::Timer::create_single_shot(AK::Duration::from_seconds(3), [] { });
        timer->start();
        Web::HTML::main_thread_event_loop().spin_until(GC::create_function(realm.heap(),
            GC::captures(request, timer->make_weak_ptr<Core::Timer>()),
            [](auto& request, auto& timer) {
                return request->settled || !timer || !timer->is_active();
            }));
    }

    return request;
}

GC::Ref<Infra::Body> body(JS::Realm& realm, Fetch& fetch)
{
    pump();
    VERIFY(fetch.state->response && fetch.state->response->body().has_value());
    auto channel
        = fetch.state->response->body()->bytes().get<NonnullRefPtr<Engine::FetchByteChannel>>();
    return Web::Fetch::Fetching::create_body_from_channel(realm, *fetch.controller, channel);
}

ErrorOr<void> retained_headers(JS::Realm& realm, Web::DOM::Document& document)
{
    namespace Gate = Firefly::WebRequestGate;
    Gate::Completion held;
    size_t decisions = 0, native_cancels = 0, logical_finishes = 0;
    auto epoch = Gate::policy().epoch + 1;
    Gate::install(
        [&](Gate::Stage stage, auto& source, JsonObject,
            Gate::Completion complete) -> ErrorOr<u32> {
            if (stage != Gate::Stage::Headers || &source != &document || held)
                return Error::from_string_literal("Unexpected retained header admission");
            held = move(complete);
            return static_cast<u32>(++decisions);
        },
        [&](u32) { ++native_cancels; }, [&](u32) { ++logical_finishes; });
    ScopeGuard cleanup([&] {
        Gate::shutdown();
        (void)Gate::configure({ epoch + 1, false, false });
    });
    TRY(check(Gate::configure({ epoch, false, true }), "Header policy setup failed"sv));
    for (bool head : { false, true }) {
        for (bool deny : { false, true }) {
            Fetch fetch(head, Protocols::file_size_limit, &document);
            TRY(fetch.metadata(0, head));
            TRY(fetch.send(Kind::Complete, 0));
            TRY(check(!fetch.receiver->finished() && !fetch.pending->is_resolved()
                    && !fetch.state->response && !fetch.state->completions
                    && !fetch.state->cancellations,
                "Early EOF bypassed pending header authorization"sv));
            pump(); // Promote the real gate job, retaining its security decision.
            TRY(check(!!held && !fetch.receiver->finished() && !fetch.pending->is_resolved(),
                "Header gate did not retain the empty response"sv));
            realm.vm().heap().collect_garbage();
            JsonObject decision;
            decision.set("cancel"sv, deny);
            auto complete = move(held);
            complete(move(decision));
            pump();
            TRY(check(fetch.receiver->finished() && fetch.pending->is_resolved()
                    && fetch.state->response && fetch.state->response->is_network_error() == deny
                    && fetch.state->completions == 1 && fetch.state->cancellations == 0,
                "Retained header decision lost terminal completion accounting"sv));
            if (!deny) {
                if (head) {
                    TRY(check(!fetch.state->response->body().has_value(),
                        "Authorized HEAD acquired a body"sv));
                } else {
                    GC::Root stream_body { body(realm, fetch) };
                    GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
                    GC::Root result { read(realm, *reader) };
                    TRY(check(result->closed && result->bytes.is_empty() && !result->errored,
                        "Authorized empty GET did not deliver clean EOF"sv));
                }
            }
            TRY(check(fetch.send(Kind::Complete, 0).is_error() && fetch.state->completions == 1
                    && !fetch.state->cancellations,
                "Duplicate EOF changed terminal callback accounting"sv));
            // This direct adapter fixture does not run Fetch's full end-of-body
            // handover; explicitly retire its logical identity after validation.
            Gate::finish(*fetch.params);
        }
    }
    TRY(check(decisions == 4 && native_cancels == 0 && logical_finishes == 4,
        "Retained header fixture leaked decision or logical ownership"sv));
    outln("PASS retained header authorization after early EOF: empty GET/HEAD allow/deny, "
          "GC and exact terminal callbacks");
    return {};
}

ErrorOr<void> transfer(JS::Realm& realm, StringView path)
{
    Fetch fetch;
    auto source = TRY(Protocols::FileRequests::create(91));
    Array<HTTP::Header, 1> headers { { { "Cache-Control", "no-store" } } };
    auto info = TRY(source->begin(91, 7, path, false, headers, MonotonicTime::now()));
    auto encoded = TRY(Protocols::encode_file_response_head(info));
    TRY(fetch.send(Kind::Metadata, 0, encoded.bytes()));
    GC::Root stream_body { body(realm, fetch) };
    TRY(check(fetch.state->response->status() == 200
            && fetch.state->response->header_list()->get("Content-Type"sv)
                == Optional<ByteString> { "application/octet-stream" }
            && fetch.state->response->header_list()->get("Content-Length"sv)
                == Optional<ByteString> { ByteString::number(info.content_length) }
            && fetch.state->response->header_list()->get("Cache-Control"sv)
                == Optional<ByteString> { "no-store" },
        "Actual LibWeb response lost status/headers"sv));
    GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
    u64 received = 0;
    for (;;) {
        TRY(check(fetch.state->credit > 0 && fetch.state->credit <= Protocols::file_chunk_limit,
            "Invalid upstream body credit"sv));
        auto maximum = min<u64>(fetch.state->credit, Protocols::file_chunk_limit);
        auto chunk = TRY(source->pull(91, 7, maximum, MonotonicTime::now()));
        fetch.state->credit -= chunk.bytes.size();
        if (!chunk.bytes.is_empty())
            TRY(fetch.send(Kind::Chunk, chunk.offset, chunk.bytes));
        if (chunk.complete)
            TRY(fetch.send(Kind::Complete, chunk.offset + chunk.bytes.size()));
        while (received < chunk.offset + chunk.bytes.size()) {
            GC::Root result { read(realm, *reader) };
            TRY(check(result->settled && !result->errored && !result->closed
                    && !result->bytes.is_empty()
                    && result->bytes.size() <= chunk.offset + chunk.bytes.size() - received,
                "Actual ReadableStream did not deliver chunk"sv));
            for (size_t i = 0; i < result->bytes.size(); ++i)
                TRY(check(result->bytes[i] == static_cast<u8>(((received + i) * 37 + 11) & 255),
                    "ReadableStream file byte mismatch"sv));
            received += result->bytes.size();
        }

        if (chunk.complete)
            break;
        realm.vm().heap().collect_garbage();
        pump();
    }

    GC::Root end { read(realm, *reader) };
    TRY(check(end->closed && received == info.content_length && fetch.receiver->finished()
            && !fetch.state->cancellations && !source->pending(),
        "ReadableStream completion/lifetime failed"sv));
    outln("PASS actual file jobs through LibWeb PendingResponse/ReadableStream: {} bytes, bounded "
          "credit and GC",
        received);
    return {};
}

// Exercise real LibWeb responses with modern metadata, not a JavaScript mock.
ErrorOr<void> streamed_responses(JS::Realm& realm)
{
    for (auto status : { 200, 404, 500 }) {
        Fetch fetch;
        Protocols::StreamResponseHead head { 7, static_cast<u16>(status), {}, false, false,
            { { "Content-Type", "application/octet-stream" } } };
        auto encoded = TRY(Protocols::encode_stream_response_head(head));
        TRY(fetch.send(Kind::Metadata, 0, encoded.bytes()));
        GC::Root stream_body { body(realm, fetch) };
        TRY(check(fetch.state->response->status() == status
                && !fetch.state->response->body()->length().has_value(),
            "Unknown-length response lost status or gained a guessed length"sv));
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        Array<u8, 3> bytes { 0, 255, 17 };
        TRY(fetch.send(Kind::Chunk, 0, bytes));
        realm.vm().heap().collect_garbage();
        GC::Root chunk { read(realm, *reader) };
        TRY(check(chunk->settled && !chunk->errored && chunk->bytes.bytes() == bytes.span(),
            "Modern binary body was altered"sv));
        TRY(fetch.send(Kind::Complete, bytes.size()));
        GC::Root end { read(realm, *reader) };
        TRY(check(end->closed && fetch.receiver->finished() && !fetch.state->cancellations,
            "Unknown-length EOF did not close the actual stream"sv));
    }

    for (auto status : { 204, 205, 304 }) {
        Fetch fetch;
        Protocols::StreamResponseHead head { 7, static_cast<u16>(status), {}, false, true, {} };
        auto encoded = TRY(Protocols::encode_stream_response_head(head));
        TRY(fetch.send(Kind::Metadata, 0, encoded.bytes()));
        TRY(fetch.send(Kind::Complete, 0));
        pump();
        TRY(check(fetch.state->response && fetch.state->response->status() == status
                && !fetch.state->response->body().has_value() && !fetch.state->credit,
            "Bodyless modern status requested credit or created a body"sv));
    }

    {
        Fetch fetch;
        Protocols::StreamResponseHead head { 7, 200, 4, false, false, {} };
        auto encoded = TRY(Protocols::encode_stream_response_head(head));
        TRY(fetch.send(Kind::Metadata, 0, encoded.bytes()));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        TRY(check(fetch.send(Kind::Complete, 0).is_error(),
            "Premature modern declared-length EOF was accepted"sv));
        GC::Root result { read(realm, *reader) };
        TRY(check(result->errored && fetch.state->cancellations == 1,
            "Modern length mismatch left the body pending"sv));
    }

    {
        Fetch fetch(false, 2);
        Protocols::StreamResponseHead head { 7, 200, {}, false, false, {} };
        auto encoded = TRY(Protocols::encode_stream_response_head(head));
        TRY(fetch.send(Kind::Metadata, 0, encoded.bytes()));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        Array<u8, 3> bytes { 1, 2, 3 };
        TRY(check(fetch.send(Kind::Chunk, 0, bytes).is_error(),
            "Unknown-length response bypassed native navigation/body size budget"sv));
        GC::Root result { read(realm, *reader) };
        TRY(check(result->errored && fetch.state->cancellations == 1,
            "Unknown-length body budget refusal left the stream pending"sv));
    }

    outln("PASS modern LibWeb status/binary/unknown-length EOF/GC/bodyless/length-mismatch "
          "responses");
    return {};
}

ErrorOr<void> controls(JS::Realm& realm)
{
    {
        Fetch fetch;
        auto weak = fetch.receiver->make_weak_ptr();
        fetch.receiver = nullptr;
        pump();
        TRY(check(!weak.strong_ref() && fetch.state->cancellations == 1 && fetch.state->response
                && fetch.state->response->is_network_error(),
            "Dropping native owner retained unresolved fetch"sv));
    }

    {
        Fetch fetch;
        fetch.controller->abort(realm, {});
        pump();
        TRY(check(fetch.receiver->finished() && fetch.state->cancellations == 1
                && fetch.state->response && fetch.state->response->aborted(),
            "Abort before metadata did not settle"sv));
    }

    {
        Fetch fetch;
        TRY(fetch.metadata(100));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        Array<u8, 17> queued {};
        TRY(fetch.send(Kind::Chunk, 0, queued));
        fetch.controller->abort(realm, {});
        GC::Root result { read(realm, *reader) };
        TRY(check(result->errored && fetch.receiver->finished() && fetch.state->cancellations == 1,
            "Abort after metadata left body pending"sv));
        fetch.receiver->stop();
        TRY(check(fetch.state->cancellations == 1, "Repeated stop sent duplicate cancellation"sv));
    }

    {
        Fetch fetch;
        TRY(fetch.metadata(100));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        (void)reader->cancel(JS::js_undefined());
        pump();
        TRY(check(fetch.receiver->finished() && fetch.state->cancellations == 1,
            "Actual reader cancellation did not reach native transport"sv));
    }

    {
        Fetch fetch(true);
        TRY(fetch.metadata(100, true));
        TRY(fetch.send(Kind::Complete, 0));
        pump();
        TRY(check(fetch.state->response && !fetch.state->response->body().has_value()
                && fetch.receiver->finished() && !fetch.state->credit
                && !fetch.state->cancellations,
            "HEAD supplied body or transport credit"sv));
    }

    {
        Fetch fetch;
        TRY(fetch.metadata(0));
        TRY(fetch.send(Kind::Complete, 0));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        GC::Root result { read(realm, *reader) };
        TRY(check(result->closed && !result->errored && !fetch.state->cancellations,
            "Empty body did not close normally"sv));
    }

    {
        Fetch fetch;
        fetch.state->reenter = true;
        TRY(fetch.metadata(100));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        GC::Root result { read(realm, *reader) };
        TRY(check(result->errored && fetch.receiver->finished() && fetch.state->cancellations == 1
                && fetch.state->credit_returns == 1,
            "Credit callback cancellation lost native lifetime"sv));
    }

    {
        Fetch fetch;
        TRY(fetch.metadata(Protocols::file_chunk_limit + 1));
        auto bytes = TRY(ByteBuffer::create_zeroed(Protocols::file_chunk_limit));
        TRY(fetch.send(Kind::Chunk, 0, bytes));
        u8 extra = 1;
        TRY(check(fetch.send(Kind::Chunk, bytes.size(), { &extra, 1 }).is_error(),
            "Producer exceeded upstream credit"sv));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        GC::Root result { read(realm, *reader) };
        TRY(check(result->errored && fetch.state->cancellations == 1,
            "Over-credit response did not error"sv));
    }

    {
        Fetch fetch;
        u8 byte = 1;
        TRY(check(fetch.send(Kind::Chunk, 0, { &byte, 1 }).is_error(), "Body preceded metadata"sv));
        pump();
        TRY(check(fetch.state->response && fetch.state->response->is_network_error(),
            "Missing-head failure did not settle pending response"sv));
    }

    for (auto foreign : { Protocols::FilePacket { Kind::Complete, 92, 7, 0, {} },
             Protocols::FilePacket { Kind::Complete, 91, 8, 0, {} } }) {
        Fetch fetch;
        auto wire = TRY(Protocols::encode_file_packet(foreign));
        TRY(check(
            fetch.receiver->receive(wire).is_error(), "Foreign owner/request frame admitted"sv));
        pump();
        TRY(check(fetch.state->response && fetch.state->response->is_network_error()
                && fetch.state->cancellations == 1,
            "Foreign frame did not settle pending response"sv));
    }

    for (size_t mode = 0; mode < 5; ++mode) {
        Fetch fetch;
        TRY(fetch.metadata(100));
        GC::Root stream_body { body(realm, fetch) };
        GC::Root reader { MUST(stream_body->stream()->get_a_reader()) };
        u8 byte = 1;
        auto invalid = mode == 0 ? fetch.send(Kind::Chunk, 1, { &byte, 1 })
            : mode == 1          ? fetch.send(Kind::Complete, 0)
            : mode == 2          ? fetch.metadata(100)
            : mode == 3          ? fetch.receiver->receive("bad"sv.bytes())
                                 : fetch.metadata(100, true);
        TRY(check(invalid.is_error(), "Malformed protocol response admitted"sv));
        GC::Root result { read(realm, *reader) };
        TRY(check(result->errored && fetch.state->cancellations == 1,
            "Invalid response left stream pending"sv));
    }

    outln("PASS actual FetchController abort before/after headers, reader cancel, HEAD and "
          "malformed-frame stream errors");
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments arguments)
{
    Firefly::Tests::ProtocolArguments native_arguments;
    TRY(native_arguments.initialize(arguments));
    arguments = native_arguments.arguments();
    if (arguments.argc != 2)
        return Error::from_string_literal("Expected authorized fixture path");
    Core::EventLoop::initialize_for_current_thread();
    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    GC::Root initial { Web::Bindings::create_a_principal_javascript_realm() };
    auto& page = Web::Bindings::principal_host_defined_page(*initial);
    auto traversable = Web::HTML::LocalTraversableNavigable::create_a_new_top_level_traversable(
        page, nullptr, {});
    page.set_top_level_traversable(traversable);
    GC::Root document { *traversable->active_document() };
    auto& realm = document->relevant_settings_object().realm();
    {
        Web::HTML::TemporaryExecutionContext context { realm,
            Web::HTML::TemporaryExecutionContext::CallbacksEnabled::Yes };
        TRY(transfer(realm, arguments.strings[1]));
        TRY(controls(realm));
        TRY(streamed_responses(realm));
        TRY(retained_headers(realm, *document));
    }

    TRY(check_fetch_dispatch(realm, *document, arguments.strings[1]));
    // The principal-realm helper leaves its testing context on the VM stack.
    // HTML tasks require an empty stack at their microtask checkpoint, as in
    // EventLoop::spin_until. Keep the saved context rooted until pumping ends.
    auto& vm = realm.vm();
    vm.save_execution_context_stack();
    vm.clear_execution_context_stack();
    ScopeGuard restore_context = [&] { vm.restore_execution_context_stack(); };
    pump();
    return 0;
}
