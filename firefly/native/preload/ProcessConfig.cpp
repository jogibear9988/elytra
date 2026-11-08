/** Validate both sides of the native handoff before constructing source-realm JavaScript data. */
#include "ProcessConfig.h"

namespace Firefly::PreloadProcess {
ErrorOr<Configuration> decode(JsonValue const& value)
{
    if (!value.is_object() || value.as_object().size() != 3
        || value.as_object().get_u32("version"sv) != 1u)
        return Error::from_string_literal("Invalid preload process metadata version/schema");
    auto args = value.as_object().get_array("arguments"sv);
    auto env = value.as_object().get_object("environment"sv);
    if (!args.has_value() || !env.has_value() || args->size() > max_arguments
        || env->size() > max_environment_entries)
        return Error::from_string_literal("Invalid preload argument/environment collection");
    size_t argument_bytes = 0, total = 0;
    for (auto const& item : args->values()) {
        if (!item.is_string() || item.as_string().contains('\0')
            || item.as_string().bytes().size() > 4096
            || item.as_string().bytes().size() > max_argument_bytes - argument_bytes)
            return Error::from_string_literal("Invalid preload argument string/budget");
        argument_bytes += item.as_string().bytes().size();
    }

    total = argument_bytes;
    bool valid = true;
    env->for_each_member([&](String const& key, JsonValue const& item) {
        if (!valid)
            return;
        if (key.is_empty() || key.contains('\0') || !item.is_string()
            || item.as_string().contains('\0')) {
            valid = false;
            return;
        }

        auto bytes = key.bytes().size() + item.as_string().bytes().size();
        if (bytes > max_metadata_bytes - total) {
            valid = false;
            return;
        }

        total += bytes;
    });
    if (!valid)
        return Error::from_string_literal("Invalid preload environment string/budget");
    return Configuration { *args, *env, total };
}

JsonObject encode(Configuration const& value)
{
    JsonObject output;
    output.set("version"sv, 1);
    output.set("arguments"sv, value.arguments);
    output.set("environment"sv, value.environment);
    return output;
}
}
