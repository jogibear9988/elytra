/** Native metadata policy only: no server, Fetch handler or protocol.handle acceptance. */
#include "native/protocol/files/ResponseHead.h"
#include "native/protocol/files/StreamResponseHead.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <LibMain/Main.h>
#include <initializer_list>
#include <string>

using namespace Firefly::Protocols;

namespace {
ErrorOr<void> check(bool value, StringView message)
{
    if (!value)
        return Error::from_string_view(message);
    return {};
}

JsonArray array(std::initializer_list<JsonValue> values)
{
    JsonArray result;
    for (auto const& value : values)
        MUST(result.append(value));
    return result;
}

StreamResponseHead sample()
{
    return { 7, 201, 17, false, false,
        { { "Content-Type", "application/octet-stream" }, { "Content-Length", "17" },
            { "Set-Cookie", "first=1" }, { "set-cookie", "second=2" },
            { "X-Bytes", "\x80\xff\t end" } } };
}

ErrorOr<void> roundtrip(StreamResponseHead const& head)
{
    auto wire = TRY(encode_stream_response_head(head));
    auto copy = TRY(decode_stream_response_head(wire));
    TRY(check(copy.request == head.request && copy.status == head.status
            && copy.content_length == head.content_length && copy.head == head.head
            && copy.body_null == head.body_null && copy.headers.size() == head.headers.size(),
        "Stream response fields changed during roundtrip"sv));
    for (size_t i = 0; i < head.headers.size(); ++i)
        TRY(check(copy.headers[i].name == head.headers[i].name
                && copy.headers[i].value == head.headers[i].value,
            "Stream response lost header bytes/order/duplicates"sv));
    TRY(check(TRY(encode_stream_response_head(copy)) == wire,
        "Stream response encoding is not stable"sv));
    return {};
}

ErrorOr<void> policies()
{
    TRY(roundtrip(sample()));
    for (auto status : { 200, 299, 301, 404, 500, 599 }) {
        StreamResponseHead head { 1, static_cast<u16>(status), {}, false, false, {} };
        TRY(roundtrip(head));
    }

    // Null response and empty readable body are different states on the wire.
    TRY(roundtrip({ 1, 200, 0, false, true, {} }));
    TRY(roundtrip({ 1, 200, 0, false, false, {} }));
    TRY(roundtrip({ 1, 200, file_size_limit, false, false, {} }));
    TRY(roundtrip({ 1, 200, 17, true, true, { { "Content-Length", "17" } } }));
    TRY(roundtrip({ 1, 304, 17, false, true, { { "Content-Length", "17" } } }));
    TRY(roundtrip({ 1, 204, {}, false, true, {} }));
    TRY(roundtrip({ 1, 205, 0, false, true, { { "Content-Length", "0" } } }));
    for (size_t mode = 0; mode < 12; ++mode) {
        auto head = sample();
        switch (mode) {
        case 0:
            head.request = 0;
            break;
        case 1:
            head.status = 199;
            break;
        case 2:
            head.status = 600;
            break;
        case 3:
            head.content_length = file_size_limit + 1;
            break;
        case 4:
            head.head = true;
            break;
        case 5:
            head.status = 204;
            break;
        case 6:
            head.status = 205;
            break;
        case 7:
            head.status = 304;
            break;
        case 8:
            head.body_null = true;
            break;
        case 9:
            head.content_length = {};
            break;
        case 10:
            head.content_length = 18;
            break;
        case 11:
            head.headers.append({ "content-length", "17" });
            break;
        }

        TRY(check(encode_stream_response_head(head).is_error(),
            "Invalid stream identity/status/body/length admitted"sv));
    }

    for (auto status : { 204, 205 }) {
        StreamResponseHead head { 1, static_cast<u16>(status), 1, true, true, {} };
        TRY(check(encode_stream_response_head(head).is_error(),
            "Null-body status accepted prohibited length on HEAD"sv));
    }

    for (auto name : { "Connection"sv, "TRANSFER-ENCODING"sv, "Trailer"sv, "Upgrade"sv,
             "Keep-Alive"sv, "Proxy-Connection"sv, "TE"sv, "Bad Name"sv, "X:\t"sv }) {
        auto head = sample();
        head.headers.append({ name, "value" });
        TRY(check(encode_stream_response_head(head).is_error(),
            "Forbidden framing/header name admitted"sv));
    }

    for (auto value : { "bad\r\ninjected: yes"sv, "bad\0value"sv }) {
        auto head = sample();
        head.headers.append({ "X-Invalid", value });
        TRY(check(encode_stream_response_head(head).is_error(), "Invalid header value admitted"sv));
    }

    for (auto value : { "017"sv, "17 "sv, "+17"sv, "17,17"sv }) {
        auto head = sample();
        head.headers[1].value = value;
        TRY(check(
            encode_stream_response_head(head).is_error(), "Ambiguous Content-Length admitted"sv));
    }

    auto duplicate = sample();
    duplicate.headers.append({ "content-type", "text/plain" });
    TRY(check(encode_stream_response_head(duplicate).is_error(), "Duplicate MIME admitted"sv));
    StreamResponseHead count { 1, 200, {}, false, false, {} };
    for (size_t i = 0; i < response_header_limit; ++i)
        count.headers.append({ "X", "v" });
    TRY(roundtrip(count));
    count.headers.append({ "X", "v" });
    TRY(check(encode_stream_response_head(count).is_error(), "Header count overflow admitted"sv));
    std::string long_value(response_head_limit, 'a');
    auto bytes = sample();
    bytes.headers.append({ "X", StringView(long_value.data(), long_value.size()) });
    TRY(check(encode_stream_response_head(bytes).is_error(), "Header byte overflow admitted"sv));
    std::string expansion(response_head_limit / 2, '\xff');
    StreamResponseHead expanded { 1, 200, {}, false, false,
        { { "X-Bytes", StringView(expansion.data(), expansion.size()) } } };
    TRY(check(encode_stream_response_head(expanded).is_error(),
        "Serialized metadata expansion overflow admitted"sv));
    return {};
}

ErrorOr<void> malformed()
{
    auto wire = TRY(encode_stream_response_head(sample()));
    auto parsed = TRY(JsonValue::from_string(wire));
    for (size_t mode = 0; mode < 21; ++mode) {
        auto root = parsed.as_object();
        switch (mode) {
        case 0:
            root.set("schema"sv, 2);
            break;
        case 1:
            root.set("kind"sv, "file-response"sv);
            break;
        case 2:
            root.set("path"sv, "/private/secret"sv);
            break;
        case 3:
            root.set("request"sv, 0);
            break;
        case 4:
            root.set("request"sv, 1.5);
            break;
        case 5:
            root.set("status"sv, 200.5);
            break;
        case 6:
            root.set("length"sv, -1);
            break;
        case 7:
            root.set("length"sv, "17"sv);
            break;
        case 8:
            root.set("head"sv, 1);
            break;
        case 9:
            root.set("bodyNull"sv, 0);
            break;
        case 10:
            root.set("headers"sv, JsonArray {});
            root.set("status"sv, 199);
            break;
        case 11:
            root.set("headers"sv, "bad"sv);
            break;
        case 12:
            root.set("headers"sv, array({ JsonValue(1) }));
            break;
        case 13:
            root.set("headers"sv, array({ array({ "X"sv }) }));
            break;
        case 14:
            root.set("headers"sv, array({ array({ "X"sv, 1 }) }));
            break;
        case 15:
            root.set("headers"sv, array({ array({ "X"sv, "€"sv }) }));
            break;
        case 16:
            root.remove("bodyNull"sv);
            break;
        case 17:
            root.set("length"sv, true);
            break;
        case 18:
            root.set("schema"sv, 1.5);
            break;
        case 19:
            root.set("length"sv, 17.5);
            break;
        case 20:
            root.set("request"sv, 4294967296ull);
            break;
        }

        auto invalid = JsonValue(move(root)).serialized();
        TRY(check(decode_stream_response_head(invalid.bytes_as_string_view()).is_error(),
            "Malformed stream metadata admitted"sv));
    }

    for (auto text : { "null"sv, "[]"sv, "{}"sv, "bad"sv, "{}\0"sv })
        TRY(check(decode_stream_response_head(text).is_error(), "Invalid JSON/NUL admitted"sv));
    std::string oversized(response_head_limit + 1, ' ');
    TRY(check(decode_stream_response_head({ oversized.data(), oversized.size() }).is_error(),
        "Oversized metadata admitted"sv));
    auto legacy = TRY(make_file_response_head(1, 17, false, "/fixture.txt"sv));
    TRY(check(decode_stream_response_head(TRY(encode_file_response_head(legacy))).is_error()
            && decode_file_response_head(wire).is_error(),
        "Legacy file and stream schemas became interchangeable"sv));
    return {};
}
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    TRY(policies());
    TRY(malformed());
    outln("PASS strict stream-response metadata roundtrip, binary HTTP headers, status/null/HEAD, "
          "length/framing/byte/count budgets and malformed schema; no protocol.handle claim");
    return 0;
}
