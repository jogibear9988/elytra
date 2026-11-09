/** Pure native control-schema checks. No browser, network request or policy callback is simulated.
 */
#include "native/web-request/Headers.h"
#include "native/web-request/Packet.h"
#include <AK/JsonArray.h>
#include <AK/JsonValue.h>
#include <cstdio>

using namespace Firefly::WebRequest;
static int failures;

static void check(bool condition, char const* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static JsonObject headers(StringView key, StringView value)
{
    JsonArray array;
    MUST(array.append(MUST(String::from_utf8(value))));
    JsonObject result;
    result.set(key, move(array));
    return result;
}

int main()
{
    Packet original { Kind::Before, 0, 1, 3, 0, 11, 12, 13, 14, {} };
    original.payload.set("url"sv, "https://example.invalid/path"sv);
    auto encoded = encode_packet(original);
    check(!encoded.is_error(), "encode bounded native identity");
    if (!encoded.is_error()) {
        auto roundtrip = decode_packet(encoded.value());
        check(!roundtrip.is_error() && roundtrip.value().request == 3
                && roundtrip.value().document_local == 14
                && roundtrip.value().payload.get_string("url"sv)
                    == original.payload.get_string("url"sv),
            "roundtrip preserves IDs and payload");
        auto invalid = MUST(ByteBuffer::copy(encoded.value()));
        invalid[6] = 1;
        check(decode_packet(invalid).is_error(), "reserved field rejected");
        invalid[6] = 0;
        invalid[5] = 1;
        check(decode_packet(invalid).is_error(), "stage flag forbidden on request");
        invalid[5] = 0;
        invalid[40] = 0;
        check(decode_packet(invalid).is_error(), "missing document namespace rejected");
        check(decode_packet(encoded.value().span().trim(55)).is_error(), "short packet rejected");
    }

    Packet head = original;
    head.kind = Kind::Headers;
    head.flags = 1;
    auto head_wire = encode_packet(head);
    check(!head_wire.is_error() && !decode_packet(head_wire.value()).is_error(),
        "headers stage flag roundtrip");
    head.kind = Kind::Decision;
    check(!encode_packet(head).is_error(), "headers decision retains stage flag");
    head.kind = Kind::Cancel;
    check(!encode_packet(head).is_error(), "headers cancellation retains stage flag");
    head.kind = Kind::Finish;
    check(!encode_packet(head).is_error(), "headers finish retains stage flag");
    head.kind = Kind::Headers;
    head.flags = 0;
    check(encode_packet(head).is_error(), "headers requires exact stage flag");
    for (auto kind : { Kind::Hello, Kind::Policy, Kind::Before, Kind::Headers, Kind::Decision,
             Kind::Cancel, Kind::Finish }) {
        for (u8 flag : { 0, 1, 2 }) {
            Packet candidate = original;
            candidate.kind = kind;
            candidate.flags = flag;
            if (kind == Kind::Hello || kind == Kind::Policy) {
                candidate.epoch = kind == Kind::Hello ? 0 : 1;
                candidate.request = 0;
                candidate.page = candidate.navigable_namespace = candidate.navigable_local
                    = candidate.document_namespace = candidate.document_local = 0;
            }

            bool expected = flag < 2;
            if (kind == Kind::Hello || kind == Kind::Policy || kind == Kind::Before)
                expected = flag == 0;
            if (kind == Kind::Headers)
                expected = flag == 1;
            auto wire = encode_packet(candidate);
            check(!wire.is_error() == expected, "complete kind/stage admission matrix");
            if (!wire.is_error())
                check(
                    !decode_packet(wire.value()).is_error(), "complete kind/stage wire roundtrip");
        }
    }

    original.payload.set("oversized"sv, MUST(String::repeated('x', packet_payload_limit)));
    check(encode_packet(original).is_error(), "metadata budget enforced");
    Packet policy { Kind::Policy, 0, 2, 0, 0, 0, 0, 0, 0, {} };
    check(!encode_packet(policy).is_error(), "policy has no document capability");
    policy.document_local = 4;
    check(encode_packet(policy).is_error(), "policy cannot smuggle identity");

    auto original_headers = headers("Content-Length"sv, "12"sv);
    JsonObject copied;
    copied.set("cancel"sv, false);
    auto modified = original_headers;
    modified.set("Access-Control-Allow-Origin"sv, MUST(JsonValue::from_string("[\"*\"]"sv)));
    copied.set("responseHeaders"sv, modified);
    check(!validate_decision(copied, &original_headers).is_error(),
        "unchanged framing plus CORS header accepted");
    copied.set("responseHeaders"sv, headers("content-length"sv, "13"sv));
    check(validate_decision(copied, &original_headers).is_error(), "framing mutation rejected");
    copied.set("responseHeaders"sv, JsonObject {});
    check(validate_decision(copied, &original_headers).is_error(), "framing removal rejected");
    copied.set("responseHeaders"sv, headers("content-length"sv, "12"sv));
    check(!validate_decision(copied, &original_headers).is_error(),
        "framing name casing preserved semantically");
    check(validate_decision(copied, nullptr).is_error(),
        "before stage cannot replace response headers");
    check(validate_headers(headers("Invalid Name"sv, "ok"sv)).is_error(),
        "invalid header name rejected");
    check(validate_headers(headers("X-Value"sv, "bad\r\nvalue"sv)).is_error(),
        "header CRLF rejected");
    check(validate_headers(headers("X-Value"sv, "\xc4\x80"sv)).is_error(),
        "non-byte header text rejected");
    check(
        !validate_headers(headers("X-Value"sv, "\xc3\xbf"sv)).is_error(), "obs-text byte retained");
    auto duplicate = headers("x-value"sv, "one"sv);
    duplicate.set("X-Value"sv, MUST(JsonValue::from_string("[\"two\"]"sv)));
    check(validate_headers(duplicate).is_error(), "duplicate case-insensitive headers rejected");
    if (!failures)
        std::puts("PASS native webRequest packet/header schema (no transport acceptance)");
    return failures ? 1 : 0;
}
