/** Validate path/header reply metadata independently before touching the native file service. */
#include "Host.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/StringBuilder.h>
#include <AK/Utf8View.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
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

    Optional<u64> token(String const& text)
    {
        auto value = text.bytes_as_string_view().to_number<u64>();
        if (!value.has_value() || !*value || String::number(*value) != text)
            return {};
        return value;
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
        realm, "protocolRegisterResponse"_utf16,
        [routes](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Scheme must be a string"sv);
            auto scheme = vm.argument(0).as_string().utf16_string_view().to_utf8();
            if (scheme.is_error())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid scheme text"sv);
            auto registered = routes->register_route(scheme.value(), true);
            if (registered.is_error())
                return fail(vm, "ERR_FIREFLY_UNSUPPORTED_API"sv,
                    "Response route requires a standard custom scheme or file and available budget"sv);
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
    host.define_native_function(
        realm, "protocolResponseHead"_utf16,
        [routes](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Response head must be JSON text"sv);
            auto view = vm.argument(0).as_string().utf16_string_view();
            // The envelope adds an opaque token to the separately bounded head.
            if (view.length_in_code_units() > response_head_limit + 128)
                return fail(vm, "ERR_OUT_OF_RANGE"sv, "Response head exceeds metadata budget"sv);
            auto bytes = view.to_utf8();
            if (bytes.is_error() || bytes.value().byte_count() > response_head_limit + 128)
                return fail(vm, "ERR_OUT_OF_RANGE"sv, "Invalid/oversized response head text"sv);
            auto parsed = JsonValue::from_string(bytes.value());
            if (parsed.is_error() || !parsed.value().is_object())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid response head envelope"sv);
            auto& object = parsed.value().as_object();
            auto text = object.get_string("token"sv);
            auto id = text.has_value() ? token(*text) : Optional<u64> {};
            if (!id.has_value())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid protocol request token"sv);
            if (object.size() == 2 && object.get_bool("error"sv).value_or(false))
                return JS::Value(routes->reply_response(*id, Error::from_errno(EACCES)));
            auto head = object.get_object("head"sv);
            if (object.size() != 2 || !head.has_value()) {
                (void)routes->reply_response(*id, Error::from_errno(EINVAL));
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid response head envelope"sv);
            }

            // The application token is a u64 identity, never a wire u32 request.
            // Ignore any supplied request field and assign the real ID in routes.
            head->set("request"sv, 1);
            auto wire = JsonValue(*head).serialized();
            auto decoded = decode_stream_response_head(wire.bytes_as_string_view());
            if (decoded.is_error()) {
                (void)routes->reply_response(*id, Error::from_errno(EINVAL));
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid stream response head"sv);
            }

            return JS::Value(routes->reply_response(*id, decoded.release_value()));
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "protocolResponseChunk"_utf16,
        [routes](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string() || !vm.argument(1).is_object()
                || !is<JS::Uint8Array>(vm.argument(1).as_object()) || !vm.argument(2).is_boolean())
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv,
                    "Response chunk requires token, Uint8Array and boolean completion"sv);
            auto token_view = vm.argument(0).as_string().utf16_string_view();
            if (token_view.length_in_code_units() > 20)
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid protocol request token"sv);
            auto text = token_view.to_utf8();
            auto id = text.is_error() ? Optional<u64> {} : token(text.value());
            if (!id.has_value())
                return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid protocol request token"sv);
            auto& array = static_cast<JS::Uint8Array&>(vm.argument(1).as_object());
            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness))
                return fail(vm, "ERR_INVALID_ARG_TYPE"sv, "Detached response chunk"sv);
            auto size = JS::typed_array_byte_length(witness);
            if (size > file_chunk_limit)
                return fail(
                    vm, "ERR_OUT_OF_RANGE"sv, "Response chunk exceeds native credit budget"sv);
            auto* data
                = static_cast<u8 const*>(array.viewed_array_buffer()->data_at(array.byte_offset()));
            return JS::Value(routes->reply_chunk(*id, { data, size }, vm.argument(2).as_bool()));
        },
        3, JS::default_attributes);
}
}
