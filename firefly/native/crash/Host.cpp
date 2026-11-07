/** No VM object is retained by the signal handler or process-lifetime crash configuration. */
#include "Host.h"
#include "Reporter.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly::CrashReporting {
namespace {
    JS::Completion fail(JS::VM& vm, StringView code, StringView message)
    {
        auto error = JS::Error::create(*vm.current_realm(), Utf16String::from_utf8(message));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(code)), JS::default_attributes);
        return JS::throw_completion(error);
    }
}

void install_host(JS::Realm& realm, JS::Object& host)
{
    host.define_native_function(
        realm, "crashDirectory"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_undefined()) {
                if (!vm.argument(0).is_string())
                    return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Crash directory must be a string"sv);
                auto view = vm.argument(0).as_string().utf16_string_view();
                if (view.length_in_code_units() > 32768)
                    return fail(vm, "ERR_OUT_OF_RANGE"sv, "Crash directory exceeds path budget"sv);
                auto path = view.to_utf8();
                if (path.is_error() || set_directory(path.value()).is_error())
                    return fail(vm, "ERR_INVALID_ARG_VALUE"sv,
                        "Crash directory must exist and be absolute; configure it before start"sv);
            }

            return JS::PrimitiveString::create(vm, Utf16String::from_utf8(directory()));
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "crashStart"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return fail(
                    vm, "ERR_INVALID_ARG_TYPE"sv, "Crash configuration must be JSON text"sv);
            auto view = vm.argument(0).as_string().utf16_string_view();
            if (view.length_in_code_units() > 16 * 1024)
                return fail(
                    vm, "ERR_OUT_OF_RANGE"sv, "Crash configuration exceeds metadata budget"sv);
            auto text = view.to_utf8();
            if (text.is_error() || start(text.value()).is_error())
                return fail(vm, "ERR_FIREFLY_CRASH_REPORTER_START_FAILED"sv,
                    "Cannot initialize local native crash reporting; invalid metadata, unsupported backend or prior initialization"sv);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "crashParameters"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::PrimitiveString::create(vm, Utf16String::from_utf8(parameters()));
        },
        0, JS::default_attributes);
}
}
