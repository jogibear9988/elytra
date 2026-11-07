/** Versioned exact schema, byte-preserving HTTP values and bounded metadata validation. */
#include "ResponseHead.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <AK/Utf8View.h>
#include <LibCore/MimeData.h>

namespace Firefly::Protocols {
namespace {
    bool forbidden_file_header(StringView name)
    {
        // These describe network framing rather than this exact-length file body.
        for (auto forbidden : { "connection"sv, "transfer-encoding"sv, "trailer"sv, "upgrade"sv,
                 "keep-alive"sv, "proxy-connection"sv, "te"sv })
            if (name.equals_ignoring_ascii_case(forbidden))
                return true;
        return false;
    }

    ErrorOr<void> validate(ResponseHead const& head)
    {
        if (!head.request || head.content_length > file_size_limit
            || head.headers.size() > response_header_limit)
            return Error::from_string_literal("Invalid file response identity/length/header count");
        size_t budget = 0, lengths = 0, types = 0;
        for (auto const& header : head.headers) {
            if (header.name.length() > response_head_limit
                || header.value.length() > response_head_limit)
                return Error::from_string_literal("File response header exceeds budget");
            budget += header.name.length() + header.value.length();
            if (budget > response_head_limit || !HTTP::is_header_name(header.name)
                || !HTTP::is_header_value(header.value) || forbidden_file_header(header.name))
                return Error::from_string_literal("Invalid file response header or budget");
            if (header.name.equals_ignoring_ascii_case("content-length"sv)) {
                ++lengths;
                if (header.value != ByteString::number(head.content_length))
                    return Error::from_string_literal("File response Content-Length mismatch");
            }

            if (header.name.equals_ignoring_ascii_case("content-type"sv))
                ++types;
        }

        if (lengths != 1 || types != 1)
            return Error::from_string_literal(
                "File response requires one Content-Length and Content-Type");
        return {};
    }

    // HTTP fields are byte sequences. Encode each byte as the same Unicode code
    // point, then recover bytes explicitly; UTF-8 conversion must not alter obs-text.
    ErrorOr<String> encode_header_bytes(StringView bytes)
    {
        StringBuilder builder;
        for (auto byte : bytes)
            TRY(builder.try_append_code_point(static_cast<u8>(byte)));
        return builder.to_string();
    }

    ErrorOr<ByteString> decode_header_bytes(String const& value)
    {
        StringBuilder builder;
        for (auto code_point : Utf8View(value)) {
            if (code_point > 255)
                return Error::from_string_literal(
                    "File response header contains non-byte character");
            TRY(builder.try_append(static_cast<char>(code_point)));
        }

        return builder.to_byte_string();
    }
}

ErrorOr<ResponseHead> make_file_response_head(
    u32 request, u64 length, bool head, StringView path, ReadonlySpan<HTTP::Header> supplied)
{
    if (supplied.size() > response_header_limit - 1)
        return Error::from_string_literal("Too many supplied file response headers");
    ResponseHead result { request, length, head, {} };
    bool type = false;
    for (auto const& header : supplied) {
        if (header.name.equals_ignoring_ascii_case("content-length"sv))
            return Error::from_string_literal("File response Content-Length is generated");
        type |= header.name.equals_ignoring_ascii_case("content-type"sv);
        TRY(result.headers.try_append(header));
    }

    if (!type)
        TRY(result.headers.try_append(
            { "Content-Type", Core::guess_mime_type_based_on_filename(path) }));
    TRY(result.headers.try_append({ "Content-Length", ByteString::number(length) }));
    TRY(validate(result));
    return result;
}

ErrorOr<ByteString> encode_file_response_head(ResponseHead const& head)
{
    TRY(validate(head));
    JsonArray headers;
    for (auto const& header : head.headers) {
        JsonArray pair;
        TRY(pair.append(TRY(encode_header_bytes(header.name))));
        TRY(pair.append(TRY(encode_header_bytes(header.value))));
        TRY(headers.append(move(pair)));
    }

    JsonObject root;
    root.set("schema"sv, 1);
    root.set("kind"sv, "file-response"sv);
    root.set("request"sv, head.request);
    root.set("status"sv, 200);
    root.set("length"sv, head.content_length);
    root.set("head"sv, head.head);
    root.set("headers"sv, move(headers));
    auto serialized = JsonValue(move(root)).serialized();
    if (serialized.bytes_as_string_view().length() > response_head_limit)
        return Error::from_string_literal("Encoded file response metadata exceeds budget");
    return ByteString(serialized.bytes_as_string_view());
}

ErrorOr<ResponseHead> decode_file_response_head(StringView text)
{
    if (text.length() > response_head_limit || text.contains('\0'))
        return Error::from_string_literal("Invalid file response metadata size/NUL");
    auto json = TRY(JsonValue::from_string(text));
    if (!json.is_object())
        return Error::from_string_literal("File response metadata must be an object");
    auto const& object = json.as_object();
    auto schema = object.get_u32("schema"sv);
    auto kind = object.get_string("kind"sv);
    auto request = object.get_u32("request"sv);
    auto status = object.get_u16("status"sv);
    auto length = object.get_u64("length"sv);
    auto head = object.get_bool("head"sv);
    auto headers = object.get_array("headers"sv);
    if (object.size() != 7 || (!schema.has_value() || *schema != 1) || !kind.has_value()
        || *kind != "file-response"sv || !request.has_value()
        || (!status.has_value() || *status != 200) || !length.has_value() || !head.has_value()
        || !headers.has_value() || headers->size() > response_header_limit)
        return Error::from_string_literal("Unknown/invalid file response metadata schema");
    ResponseHead result { *request, *length, *head, {} };
    for (auto const& entry : headers->values()) {
        if (!entry.is_array() || entry.as_array().size() != 2 || !entry.as_array().at(0).is_string()
            || !entry.as_array().at(1).is_string())
            return Error::from_string_literal("File response header must be a string pair");
        TRY(result.headers.try_append(
            { TRY(decode_header_bytes(entry.as_array().at(0).as_string())),
                TRY(decode_header_bytes(entry.as_array().at(1).as_string())) }));
    }

    TRY(validate(result));
    return result;
}
}
