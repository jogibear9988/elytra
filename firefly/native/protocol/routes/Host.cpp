/** Validate path/header reply metadata independently before touching the native file service. */
#include "Host.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/StringBuilder.h>
#include <AK/Utf8View.h>
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

    ErrorOr<ByteString> field_bytes(String const& value)
    {
        StringBuilder bytes;
        for (auto point : Utf8View(value)) {
            if (point > 255)
                return Error::from_errno(EINVAL);
            TRY(bytes.try_append(static_cast<char>(point)));
        }

        return bytes.to_byte_string();
    }

    ErrorOr<AuthorizedFile> response(JsonObject const& object)
    {
        auto path = object.get_string("path"sv);
        auto headers = object.get_array("headers"sv);
        if (object.size() != 3 || !path.has_value() || !headers.has_value()
            || path->byte_count() > 32768 || path->contains('\0')
            || headers->size() > response_header_limit)
            return Error::from_errno(EINVAL);
        AuthorizedFile file { path->to_byte_string(), {} };
        for (auto const& pair : headers->values()) {
            if (!pair.is_array() || pair.as_array().size() != 2
                || !pair.as_array().at(0).is_string() || !pair.as_array().at(1).is_string())
                return Error::from_errno(EINVAL);
            TRY(file.headers.try_append({ TRY(field_bytes(pair.as_array().at(0).as_string())),
                TRY(field_bytes(pair.as_array().at(1).as_string())) }));
        }
        // Pure metadata validation precedes file opening; the service repeats it with real length.
        TRY(make_file_response_head(1, 0, false, file.path, file.headers));
        return file;
    }
}

void install_file_route_host(JS::Realm& realm, JS::Object& host, NonnullRefPtr<FileRoutes> routes)
{
    host.define_native_function(
        realm, "protocolRegisterFile"_utf16,
        [routes](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Scheme must be a string"sv);
            auto scheme = vm.argument(0).as_string().utf16_string_view().to_utf8();
            if (scheme.is_error())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid scheme text"sv);
            auto registered = routes->register_route(scheme.value());
            if (registered.is_error())
                return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API"sv,
                    "File route requires a registered standard scheme and available budget"sv);
            if (!registered.value().has_value())
                return JS::Value(false);
            return JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(ByteString::number(*registered.value())));
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "protocolUnregisterFile"_utf16,
        [routes](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Scheme must be a string"sv);
            auto scheme = vm.argument(0).as_string().utf16_string_view().to_utf8();
            if (scheme.is_error())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid scheme text"sv);
            return JS::Value(routes->unregister_route(scheme.value()));
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "protocolFileReply"_utf16,
        [routes](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Protocol reply must be JSON text"sv);
            auto view = vm.argument(0).as_string().utf16_string_view();
            if (view.length_in_code_units() > 128 * 1024)
                return fail(vm, "ERR_OUT_OF_RANGE"sv, "Protocol reply exceeds metadata budget"sv);
            auto bytes = view.to_utf8();
            if (bytes.is_error() || bytes.value().byte_count() > 128 * 1024)
                return fail(vm, "ERR_OUT_OF_RANGE"sv, "Protocol reply exceeds byte budget"sv);
            auto parsed = JsonValue::from_string(bytes.value());
            if (parsed.is_error() || !parsed.value().is_object())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid protocol reply"sv);
            auto const& object = parsed.value().as_object();
            auto token_text = object.get_string("token"sv);
            auto token = token_text.has_value()
                ? token_text->bytes_as_string_view().to_number<u64>()
                : Optional<u64> {};
            if (!token.has_value() || !*token || String::number(*token) != *token_text)
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid protocol request token"sv);
            if (object.size() == 2 && object.get_bool("error"sv).value_or(false))
                return JS::Value(routes->reply(*token, Error::from_errno(EACCES)));
            auto file = response(object);
            if (file.is_error()) {
                (void)routes->reply(*token, Error::from_errno(EINVAL));
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid protocol file response"sv);
            }

            return JS::Value(routes->reply(*token, file.release_value()));
        },
        1, JS::default_attributes);
}
}
