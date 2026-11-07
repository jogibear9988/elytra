/** Independently validate the bounded wire record before publishing any process origin state. */
#include "RegistrationHost.h"
#include "OriginConfig.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly::Protocols {
namespace {
    JS::Completion fail(JS::VM& vm, StringView code, StringView message)
    {
        auto error = JS::Error::create(*vm.current_realm(), Utf16String::from_utf8(message));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(code)), JS::default_attributes);
        return JS::throw_completion(error);
    }
}

void install_registration_host(JS::Realm& realm, JS::Object& host)
{
    host.define_native_function(
        realm, "registerSchemes"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            if (!value.is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Invalid origin registration record"sv);
            auto view = value.as_string().utf16_string_view();
            if (view.length_in_code_units() > origin_config_limit)
                return fail(vm, "ERR_OUT_OF_RANGE"sv, "Origin registration exceeds byte budget"sv);
            auto bytes = view.to_utf8();
            if (bytes.is_error())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid origin registration text"sv);
            if (bytes.value().byte_count() > origin_config_limit)
                return fail(vm, "ERR_OUT_OF_RANGE"sv, "Origin registration exceeds byte budget"sv);
            auto schemes = parse_origin_configuration(bytes.value());
            if (schemes.is_error())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid origin registration schema"sv);
            auto installed = install_origin_schemes(schemes.value());
            if (installed.is_error())
                return fail(vm, "ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED"sv,
                    "Origin policy already registered or helper launch has started"sv);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
}
}
