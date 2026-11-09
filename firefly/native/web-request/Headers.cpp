/** Application policy may replace response headers, but never transport framing. */
#include "Headers.h"
#include <AK/HashMap.h>
#include <AK/JsonArray.h>
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <AK/Utf8View.h>
#include <LibHTTP/Header.h>

namespace Firefly::WebRequest {
namespace {
    using Map = HashMap<ByteString, Vector<ByteString>>;

    ErrorOr<Map> decode(JsonObject const& object)
    {
        if (object.size() > 128)
            return Error::from_string_literal("Response header count exceeded");
        Map output;
        size_t budget = 0;
        TRY(object.try_for_each_member([&](StringView key,
                                           JsonValue const& value) -> ErrorOr<void> {
            if (!HTTP::is_header_name(key) || !value.is_array() || value.as_array().size() > 128)
                return Error::from_string_literal("Invalid response header name/value array");
            auto normalized = ByteString(key).to_lowercase();
            if (output.contains(normalized))
                return Error::from_string_literal("Duplicate case-insensitive response header");
            Vector<ByteString> values;
            budget += key.length();
            for (auto const& entry : value.as_array().values()) {
                if (!entry.is_string())
                    return Error::from_string_literal("Response header value is not text");
                StringBuilder bytes;
                for (auto point : Utf8View(entry.as_string())) {
                    if (point > 255)
                        return Error::from_string_literal("Response header contains non-byte text");
                    TRY(bytes.try_append(static_cast<char>(point)));
                }

                auto decoded = bytes.to_byte_string();
                if (!HTTP::is_header_value(decoded))
                    return Error::from_string_literal("Invalid response header bytes");
                budget += decoded.length();
                TRY(values.try_append(move(decoded)));
            }

            if (budget > 32768)
                return Error::from_string_literal("Response header byte limit exceeded");
            TRY(output.try_set(move(normalized), move(values)));
            return {};
        }));
        return output;
    }
}

ErrorOr<void> validate_headers(JsonObject const& headers)
{
    (void)TRY(decode(headers));
    return {};
}

ErrorOr<void> validate_decision(JsonObject const& decision, JsonObject const* original)
{
    auto cancel = decision.get_bool("cancel"sv);
    if (!cancel.has_value() || decision.size() > 2)
        return Error::from_string_literal("Invalid web request decision");
    auto replacement = decision.get_object("responseHeaders"sv);
    if (decision.has("responseHeaders"sv) && (!replacement.has_value() || !original))
        return Error::from_string_literal(
            "Header replacement is unavailable for this request stage");
    if (decision.size() != (replacement.has_value() ? 2u : 1u))
        return Error::from_string_literal("Unknown web request decision field");
    if (!replacement.has_value())
        return {};
    auto before = TRY(decode(*original));
    auto after = TRY(decode(*replacement));
    for (auto key : { "content-length"sv, "transfer-encoding"sv, "connection"sv, "trailer"sv,
             "upgrade"sv, "keep-alive"sv, "proxy-connection"sv, "te"sv }) {
        auto old_value = before.get(key), new_value = after.get(key);
        if (old_value.has_value() != new_value.has_value()
            || (old_value.has_value() && *old_value != *new_value))
            return Error::from_string_literal("Response framing changes are unsupported");
    }

    return {};
}
}
