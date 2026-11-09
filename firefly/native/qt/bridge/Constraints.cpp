/** Public dimensions stay outside the native boundary; this host independently validates every
 * limit. */
#include "Impl.h"
#include "Value.h"
#include <AK/JsonArray.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>
#include <UI/Qt/WebContentView.h>
#include <cmath>

namespace Firefly {
namespace {
    Optional<int> dimension(JsonObject const& object, StringView name)
    {
        auto value = object.get(name);
        if (!value.has_value())
            return {};
        auto number = value->get_number_with_precision_loss<double>();
        if (!number.has_value() || !std::isfinite(*number) || std::floor(*number) != *number
            || *number < 0 || *number > 16384)
            return {};
        return static_cast<int>(*number);
    }

    Optional<int> dimension(JS::Value value)
    {
        if (!value.is_number())
            return {};
        auto number = value.as_double();
        if (!std::isfinite(number) || std::floor(number) != number || number < 0 || number > 16384)
            return {};
        return static_cast<int>(number);
    }
}

JS::ThrowCompletionOr<WindowLimits> Bridge::Impl::read_limits(JS::VM& vm, size_t argument)
{
    auto value = vm.argument(argument);
    if (value.is_undefined())
        return WindowLimits {};
    if (!value.is_string())
        return preload_error(vm, "ERR_INVALID_ARG_TYPE", "Window constraints must be JSON text"sv);
    auto encoded = value.as_string().utf16_string_view().to_utf8(AllowLonelySurrogates::No);
    if (encoded.is_error() || encoded.value().byte_count() > 1024)
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid window constraints metadata"sv);
    auto parsed = JsonValue::from_string(encoded.value());
    if (parsed.is_error() || !parsed.value().is_object())
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid window constraints object"sv);
    auto const& object = parsed.value().as_object();
    auto min_width = dimension(object, "minWidth"sv), min_height = dimension(object, "minHeight"sv);
    auto max_width = dimension(object, "maxWidth"sv), max_height = dimension(object, "maxHeight"sv);
    if (object.size() != 4 || !min_width.has_value() || !min_height.has_value()
        || !max_width.has_value() || !max_height.has_value())
        return preload_error(
            vm, "ERR_OUT_OF_RANGE", "Window constraints require four integers in 0..16384"sv);
    return WindowLimits { QSize(*min_width, *min_height), QSize(*max_width, *max_height) };
}

void Bridge::Impl::install_constraints(JS::Object& host)
{
    auto getter = [&](Utf16View name, bool maximum) {
        host.define_native_function(
            realm, Utf16String::from_utf16(name),
            [this, maximum](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* current = TRY(window_state(vm));
                auto size = maximum ? current->size_constraints->maximum()
                                    : current->size_constraints->minimum();
                JsonArray pair;
                MUST(pair.append(size.width()));
                MUST(pair.append(size.height()));
                return BridgeDetail::text(vm, JsonValue(move(pair)).serialized());
            },
            1, JS::default_attributes);
    };

    auto setter = [&](Utf16View name, bool maximum) {
        host.define_native_function(
            realm, Utf16String::from_utf16(name),
            [this, maximum](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* current = TRY(window_state(vm));
                auto width = dimension(vm.argument(1)), height = dimension(vm.argument(2));
                if (!width.has_value() || !height.has_value())
                    return preload_error(vm, "ERR_OUT_OF_RANGE",
                        "Window constraint must be an integer in 0..16384"sv);
                bool accepted = maximum
                    ? current->size_constraints->set_maximum({ *width, *height })
                    : current->size_constraints->set_minimum({ *width, *height });
                if (!accepted)
                    return preload_error(
                        vm, "ERR_FIREFLY_WINDOW_CONSTRAINT", "Native window is unavailable"sv);
                return JS::js_undefined();
            },
            3, JS::default_attributes);
    };

    getter("minimumSize"_utf16, false);
    getter("maximumSize"_utf16, true);
    setter("setMinimumSize"_utf16, false);
    setter("setMaximumSize"_utf16, true);
}
}
