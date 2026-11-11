/** Produce fresh VM-owned values; native guards also enforce readiness independently of JS. */
#include "Host.h"
#include "../platform/GCIncludes.h"
#include "Locale.h"
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly::Locale {
namespace {
    struct NativeCallbacks {
        using HoldsNoCellEdges = void;
        std::function<bool()> ready;
        std::function<QString()> application_locale;
    };
}

namespace {
    JS::Completion fail(JS::VM& vm, Utf16View code, Utf16View message)
    {
        auto error = JS::Error::create(*vm.current_realm(), Utf16String::from_utf16(message));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf16(code)), JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::Value text(JS::VM& vm, QString const& value)
    {
        auto bytes = value.toUtf8();
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
    }
}

void install_host(JS::Realm& realm, JS::Object& host, std::function<bool()> ready,
    std::function<QString()> application_locale)
{
    NativeCallbacks callbacks { move(ready), move(application_locale) };
    host.define_native_function(realm, "appLocaleRead"_utf16,
        GC::create_function(realm.heap(), GC::captures(move(callbacks)),
            [](auto& callbacks, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_int32())
                    return fail(
                        vm, u"ERR_INVALID_ARG_TYPE"sv, u"Locale selector must be an integer"sv);
                auto selector = vm.argument(0).as_i32();
                if (selector < 0 || selector > 3)
                    return fail(vm, u"ERR_INVALID_ARG_VALUE"sv, u"Unknown locale selector"sv);
                if ((selector == 1 || selector == 2) && !callbacks.ready())
                    return fail(vm, u"ERR_FIREFLY_NOT_READY"sv,
                        u"Application/system locale requires readiness"sv);
                if (selector == 0) {
                    auto result = MUST(JS::Array::create(*vm.current_realm(), 0));
                    size_t index = 0;
                    for (auto const& language : preferred_languages())
                        (void)TRY(
                            result->create_data_property_or_throw(index++, text(vm, language)));
                    return result;
                }

                if (selector == 3)
                    return text(vm, country_code());
                auto value = selector == 1 ? system_locale() : callbacks.application_locale();
                if (value.isEmpty())
                    return fail(
                        vm, u"ERR_FIREFLY_UNSUPPORTED_API"sv, u"Native locale is unavailable"sv);
                return text(vm, value);
            }),
        1, JS::default_attributes);
}
}
