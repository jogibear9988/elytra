/**
 * Native global-console diagnostics retain LibJS's no-side-effect class labels
 * and real Error stack text. This formatter performs no I/O: public Console
 * methods use the ordinary shared stdout/stderr queues after bootstrap.
 */
#include "core/console/Formatting.h"
#include <AK/Utf16StringBuilder.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>

namespace Mite {
JS::ThrowCompletionOr<Utf16String> format_console_arguments(JS::VM& vm)
{
    Utf16StringBuilder output;
    size_t size = 0;
    auto append = [&](Utf16View value) -> JS::ThrowCompletionOr<void> {
        // Check before growing the output. Iterating native UTF-16 storage never
        // invokes getters, Proxy traps or application inspection callbacks.
        for (auto point : value) {
            size += point < 0x80 ? 1 : point < 0x800 ? 2 : point < 0x10000 ? 3 : 4;
            if (size > 1024 * 1024) {
                auto error = JS::RangeError::create(
                    vm.realm(), "Console argument text exceeds 1 MiB of UTF-8"_utf16);
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, "ERR_MITE_CONSOLE_LIMIT"_utf16),
                    JS::default_attributes);
                return JS::throw_completion(error);
            }
        }

        output.append(value);
        return {};
    };

    for (size_t index = 0; index < vm.argument_count(); ++index) {
        auto value = vm.argument(index);
        if (index)
            TRY(append(" "_utf16));
        if (value.is_object() && JS::Error::is_engine_class_of(value.as_object())) {
            auto& error = static_cast<JS::Error&>(value.as_object());
            // Data-property inspection preserves the original failure even when
            // applications replace name/message/stack with throwing accessors.
            auto name = error.get_without_side_effects("name"_utf16);
            auto message = error.get_without_side_effects("message"_utf16);
            TRY(append(name.is_string() ? name.as_string().utf16_string_view() : "Error"_utf16));
            if (message.is_string() && message.as_string().length_in_utf16_code_units()) {
                TRY(append(": "_utf16));
                TRY(append(message.as_string().utf16_string_view()));
            }

            auto stack = error.stack_string();
            if (!stack.is_empty()) {
                TRY(append("\n"_utf16));
                TRY(append(stack));
            }

            continue;
        }
        // Primitive formatting stays unchanged; objects/functions use only the
        // engine's class label, including null-prototype objects/revoked Proxies.
        auto rendered = value.is_object() ? value.to_utf16_string_without_side_effects()
                                          : TRY(value.to_utf16_string(vm));
        TRY(append(rendered));
    }

    return output.to_string();
}
}
