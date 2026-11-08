/** Strict additive metadata codec. The legacy file-response wire is unchanged. */
#include "StreamResponseHead.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <AK/Utf8View.h>
#include <cmath>

namespace Firefly::Protocols {
namespace {
    template <typename T> Optional<T> integer(JsonValue const& value)
    {
        // AK numeric accessors permit fractional conversion. Wire identities and
        // lengths must be exact integers before any narrowing conversion.
        auto number = value.get_number_with_precision_loss<double>();
        if (!number.has_value() || !std::isfinite(*number) || std::trunc(*number) != *number)
            return {};
        return value.get_integer<T>();
    }

    template <typename T> Optional<T> integer(JsonObject const& object, StringView name)
    {
        auto value = object.get(name);
        return value.has_value() ? integer<T>(*value) : Optional<T> {};
    }

    bool forbidden(StringView name)
    {
        for (auto value : { "connection"sv, "transfer-encoding"sv, "trailer"sv, "upgrade"sv,
                 "keep-alive"sv, "proxy-connection"sv, "te"sv })
            if (name.equals_ignoring_ascii_case(value))
                return true;
        return false;
    }

    ErrorOr<void> validate(StreamResponseHead const& head)
    {
        if (!head.request || head.status < 200 || head.status > 599
            || (head.content_length.has_value() && *head.content_length > file_size_limit)
            || head.headers.size() > response_header_limit)
            return Error::from_string_literal("Invalid stream response identity/status/budget");
        if ((head.head || head.status == 204 || head.status == 205 || head.status == 304)
            && !head.body_null)
            return Error::from_string_literal("HEAD/null-body status cannot carry a stream");
        if (head.status == 205 && head.content_length.has_value() && *head.content_length != 0)
            return Error::from_string_literal("205 cannot advertise nonzero Content-Length");
        if (head.status == 204 && head.content_length.has_value())
            return Error::from_string_literal("204 cannot advertise Content-Length");
        if (head.body_null && !head.head && head.status != 304 && head.content_length.has_value()
            && *head.content_length != 0)
            return Error::from_string_literal("Null stream body has nonzero content length");
        size_t budget = 0, lengths = 0, types = 0;
        for (auto const& header : head.headers) {
            if (header.name.length() > response_head_limit
                || header.value.length() > response_head_limit)
                return Error::from_string_literal("Stream response header exceeds budget");
            budget += header.name.length() + header.value.length();
            if (budget > response_head_limit || !HTTP::is_header_name(header.name)
                || !HTTP::is_header_value(header.value) || forbidden(header.name))
                return Error::from_string_literal("Invalid stream response header or budget");
            if (header.name.equals_ignoring_ascii_case("content-length"sv)) {
                if (++lengths > 1 || !head.content_length.has_value()
                    || header.value != ByteString::number(*head.content_length))
                    return Error::from_string_literal("Stream response Content-Length mismatch");
            }

            if (header.name.equals_ignoring_ascii_case("content-type"sv) && ++types > 1)
                return Error::from_string_literal("Duplicate stream response Content-Type");
        }

        return {};
    }

    // Encode byte values as equal Unicode code points. A UTF-8 roundtrip would
    // otherwise transform valid HTTP obs-text bytes such as 0x80 and 0xff.
    ErrorOr<String> encode_bytes(StringView bytes)
    {
        StringBuilder builder;
        for (auto byte : bytes)
            TRY(builder.try_append_code_point(static_cast<u8>(byte)));
        return builder.to_string();
    }

    ErrorOr<ByteString> decode_bytes(String const& value)
    {
        StringBuilder builder;
        for (auto point : Utf8View(value)) {
            if (point > 255)
                return Error::from_string_literal("Stream response header contains non-byte text");
            TRY(builder.try_append(static_cast<char>(point)));
        }

        return builder.to_byte_string();
    }
}

ErrorOr<ByteString> encode_stream_response_head(StreamResponseHead const& head)
{
    TRY(validate(head));
    JsonArray headers;
    for (auto const& header : head.headers) {
        JsonArray pair;
        TRY(pair.append(TRY(encode_bytes(header.name))));
        TRY(pair.append(TRY(encode_bytes(header.value))));
        TRY(headers.append(move(pair)));
    }

    JsonObject root;
    root.set("schema"sv, 1);
    root.set("kind"sv, "stream-response"sv);
    root.set("request"sv, head.request);
    root.set("status"sv, head.status);
    root.set("length"sv,
        head.content_length.has_value() ? JsonValue(*head.content_length) : JsonValue {});
    root.set("head"sv, head.head);
    root.set("bodyNull"sv, head.body_null);
    root.set("headers"sv, move(headers));
    auto text = JsonValue(move(root)).serialized();
    if (text.bytes_as_string_view().length() > response_head_limit)
        return Error::from_string_literal("Encoded stream response metadata exceeds budget");
    return ByteString(text.bytes_as_string_view());
}

ErrorOr<StreamResponseHead> decode_stream_response_head(StringView text)
{
    if (text.length() > response_head_limit || text.contains('\0'))
        return Error::from_string_literal("Invalid stream response metadata size/NUL");
    auto json = TRY(JsonValue::from_string(text));
    if (!json.is_object())
        return Error::from_string_literal("Stream response metadata must be an object");
    auto const& object = json.as_object();
    auto schema = integer<u32>(object, "schema"sv);
    auto kind = object.get_string("kind"sv);
    auto request = integer<u32>(object, "request"sv);
    auto status = integer<u16>(object, "status"sv);
    auto length = object.get("length"sv);
    auto head = object.get_bool("head"sv);
    auto body_null = object.get_bool("bodyNull"sv);
    auto headers = object.get_array("headers"sv);
    if (object.size() != 8 || !schema.has_value() || *schema != 1 || !kind.has_value()
        || *kind != "stream-response"sv || !request.has_value() || !status.has_value()
        || !length.has_value() || (!length->is_null() && !integer<u64>(*length).has_value())
        || !head.has_value() || !body_null.has_value() || !headers.has_value()
        || headers->size() > response_header_limit)
        return Error::from_string_literal("Unknown/invalid stream response metadata schema");
    StreamResponseHead result { *request, *status, integer<u64>(*length), *head, *body_null, {} };
    for (auto const& entry : headers->values()) {
        if (!entry.is_array() || entry.as_array().size() != 2 || !entry.as_array().at(0).is_string()
            || !entry.as_array().at(1).is_string())
            return Error::from_string_literal("Stream response header must be a string pair");
        TRY(result.headers.try_append({ TRY(decode_bytes(entry.as_array().at(0).as_string())),
            TRY(decode_bytes(entry.as_array().at(1).as_string())) }));
    }

    TRY(validate(result));
    return result;
}
}
