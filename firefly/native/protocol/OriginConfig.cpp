/** Exact native schema and argument ownership, with no environment or filesystem configuration. */
#include "OriginConfig.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>

namespace Firefly::Protocols {
ErrorOr<Vector<OriginScheme>> parse_origin_configuration(StringView text)
{
    if (text.length() > origin_config_limit || text.contains('\0'))
        return Error::from_string_literal(
            "Origin configuration exceeds its byte budget or contains NUL");
    auto value = TRY(JsonValue::from_string(text));
    if (!value.is_object())
        return Error::from_string_literal("Origin configuration must be an object");
    auto const& object = value.as_object();
    auto schema = object.get_u32("schema"sv);
    auto entries = object.get_array("schemes"sv);
    if (object.size() != 2 || !schema.has_value() || *schema != 1 || !entries.has_value())
        return Error::from_string_literal("Unknown origin configuration schema");
    if (entries->size() > 32)
        return Error::from_string_literal("Too many configured origin schemes");
    Vector<OriginScheme> schemes;
    for (auto const& item : entries->values()) {
        if (!item.is_object())
            return Error::from_string_literal("Origin scheme entry must be an object");
        auto const& entry = item.as_object();
        auto name = entry.get_string("scheme"sv);
        auto standard = entry.get_bool("standard"sv);
        auto secure = entry.get_bool("secure"sv);
        auto cors = entry.get_bool("cors"sv);
        bool declares_cors = entry.has("cors"sv);
        if (entry.size() != (declares_cors ? 4u : 3u) || !name.has_value() || !standard.has_value()
            || !secure.has_value() || (declares_cors && !cors.has_value()))
            return Error::from_string_literal(
                "Origin entry requires scheme/standard/secure and optional boolean cors");
        TRY(schemes.try_append({ *name, *standard, *secure, cors.value_or(false) }));
    }

    TRY(validate_origin_schemes(schemes));
    return schemes;
}

ErrorOr<ByteString> serialize_origin_configuration(ReadonlySpan<OriginScheme> schemes)
{
    TRY(validate_origin_schemes(schemes));
    JsonArray entries;
    for (auto const& scheme : schemes) {
        JsonObject entry;
        entry.set("scheme"sv, scheme.name);
        entry.set("standard"sv, scheme.standard);
        entry.set("secure"sv, scheme.secure);
        if (scheme.cors)
            entry.set("cors"sv, true);
        TRY(entries.append(move(entry)));
    }

    JsonObject root;
    root.set("schema"sv, 1);
    root.set("schemes"sv, move(entries));
    auto serialized = JsonValue(move(root)).serialized();
    auto result = ByteString(serialized.bytes_as_string_view());
    if (result.length() > origin_config_limit)
        return Error::from_string_literal("Serialized origin configuration exceeds budget");
    return result;
}

ErrorOr<OriginArguments> consume_origin_configuration(ReadonlySpan<StringView> input)
{
    if (input.is_empty())
        return Error::from_string_literal("Helper entry requires argv[0]");
    constexpr auto option = "--firefly-origin-policy"sv;
    constexpr auto prefix = "--firefly-origin-policy="sv;
    Optional<StringView> configuration;
    OriginArguments result;
    bool delimited = false;
    size_t index = 0;
    for (auto argument : input) {
        bool option_position = index++ > 0 && !delimited;
        if (argument.contains('\0'))
            return Error::from_string_literal("Helper argument contains NUL");
        if (option_position && argument == "--"sv)
            delimited = true;
        if (option_position && argument == option)
            return Error::from_string_literal("Origin configuration option requires =JSON");
        if (option_position && argument.starts_with(prefix)) {
            if (configuration.has_value())
                return Error::from_string_literal("Duplicate origin configuration option");
            configuration = argument.substring_view(prefix.length());
        } else {
            TRY(result.owned.try_append(ByteString(argument)));
        }
    }
    // Prepare borrowed vectors before publication; their buffers live in result.owned.
    for (auto& argument : result.owned) {
        TRY(result.argv.try_append(const_cast<char*>(argument.characters())));
        TRY(result.strings.try_append(argument.view()));
    }

    TRY(result.argv.try_append(nullptr));
    if (configuration.has_value()) {
        auto schemes = TRY(parse_origin_configuration(*configuration));
        TRY(install_origin_schemes(schemes));
    }

    return result;
}
}
