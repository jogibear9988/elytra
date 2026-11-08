/** Snapshot validated application arguments and the main OS environment before native window
 * creation. */
#include "Impl.h"
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cstring>
#include <uv.h>

namespace Firefly {
JS::ThrowCompletionOr<JsonArray> Bridge::Impl::read_process_arguments(JS::VM& vm)
{
    JsonArray arguments;
    size_t argument_bytes = 0;
    // Inspect exact array storage/descriptors; never invoke renderer-facing hooks.
    auto input = vm.argument(9);
    if (!input.is_undefined()) {
        if (!input.is_object() || input.as_object().engine_class_id() != JS_LAYOUT_CLASS_ID_ARRAY
            || JS::host_class_of(input.as_object()) || &input.as_object().shape().realm() != &realm)
            return preload_error(
                vm, "ERR_INVALID_ARG_TYPE", "additionalArguments must be an array of strings"sv);
        auto& array = input.as_object();
        auto length = TRY(array.internal_get_own_property("length"_utf16));
        if (!length.has_value() || !length->value.has_value() || !length->value->is_number()
            || length->value->as_double() > PreloadProcess::max_arguments)
            return preload_error(
                vm, "ERR_INVALID_ARG_VALUE", "additionalArguments exceeds 64 entries"sv);
        auto count = static_cast<size_t>(length->value->as_double());
        for (size_t i = 0; i < count; ++i) {
            auto item = TRY(array.ordinary_get_own_property(JS::PropertyKey(i)));
            if (!item.has_value() || item->is_accessor_descriptor() || !item->value.has_value()
                || !item->value->is_string())
                return preload_error(vm, "ERR_INVALID_ARG_TYPE",
                    "additionalArguments requires own string entries"sv);
            auto encoded
                = item->value->as_string().utf16_string_view().to_utf8(AllowLonelySurrogates::No);
            if (encoded.is_error() || encoded.value().contains('\0')
                || encoded.value().bytes().size() > 4096
                || encoded.value().bytes().size()
                    > PreloadProcess::max_argument_bytes - argument_bytes)
                return preload_error(
                    vm, "ERR_INVALID_ARG_VALUE", "additionalArguments string/budget is invalid"sv);
            argument_bytes += encoded.value().bytes().size();
            arguments.must_append(encoded.release_value());
        }
    }

    return arguments;
}

JS::ThrowCompletionOr<PreloadProcess::Configuration> Bridge::Impl::read_process_config(
    JS::VM& vm, JsonArray arguments)
{
    size_t argument_bytes = 0;
    for (auto const& item : arguments.values())
        argument_bytes += item.as_string().bytes().size();
    // Read the main OS environment, including process.env updates, once per window.
    uv_env_item_t* entries = nullptr;
    int count = 0;
    if (uv_os_environ(&entries, &count))
        return preload_error(
            vm, "ERR_FIREFLY_PRELOAD_READ_FAILED", "Cannot snapshot preload environment"sv);
    ScopeGuard free_environment([&] { uv_os_free_environ(entries, count); });
    if (count < 0 || static_cast<size_t>(count) > PreloadProcess::max_environment_entries)
        return preload_error(
            vm, "ERR_FIREFLY_PRELOAD_TOO_LARGE", "Preload environment entry budget exceeded"sv);
    JsonObject environment;
    size_t bytes = argument_bytes;
    for (int i = 0; i < count; ++i) {
        auto key = String::from_utf8_with_replacement_character(
            StringView(entries[i].name, strlen(entries[i].name)));
        auto value = String::from_utf8_with_replacement_character(
            StringView(entries[i].value, strlen(entries[i].value)));
        auto charge = key.bytes().size() + value.bytes().size();
        if (charge > PreloadProcess::max_metadata_bytes - bytes)
            return preload_error(
                vm, "ERR_FIREFLY_PRELOAD_TOO_LARGE", "Preload process metadata exceeds 64 KiB"sv);
        bytes += charge;
        environment.set(move(key), move(value));
    }

    JsonObject data;
    data.set("version"sv, 1);
    data.set("arguments"sv, move(arguments));
    data.set("environment"sv, move(environment));
    auto checked = PreloadProcess::decode(data);
    if (checked.is_error())
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid preload process metadata"sv);
    return checked.release_value();
}
}
