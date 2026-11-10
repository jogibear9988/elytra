/**
 * Provides OS entropy and native crypto host factories. Worker requests own native bytes only;
 * completion objects and callbacks stay on the VM owner thread.
 */

#include "platform/crypto/Crypto.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <uv.h>
#include <vector>

namespace Mite {
namespace {
    JS::Value entropy_error(JS::VM& vm, int status)
    {
        auto error = JS::Error::create(vm.realm(),
            Utf16String::from_utf8(StringView(uv_strerror(status), strlen(uv_strerror(status)))));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm,
                Utf16String::from_utf8(
                    StringView(uv_err_name(status), strlen(uv_err_name(status))))),
            JS::default_attributes);
        error->define_direct_property("errno"_utf16, JS::Value(status), JS::default_attributes);
        error->define_direct_property("syscall"_utf16,
            JS::PrimitiveString::create(vm, u"uv_random"sv), JS::default_attributes);
        return error;
    }

    JS::ThrowCompletionOr<size_t> requested_size(JS::VM& vm)
    {
        auto value = vm.argument(0);
        if (!value.is_number())
            return vm.throw_completion<JS::TypeError>("Invalid internal random size"_utf16);
        auto size = value.as_double();
        if (!std::isfinite(size) || size < 0 || size > 2147483647.0 || std::trunc(size) != size)
            return vm.throw_completion<JS::RangeError>("Invalid internal random size"_utf16);
        return static_cast<size_t>(size);
    }

    JS::Completion web_crypto_error(JS::VM& vm, char const* code, char const* message)
    {
        auto error = JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message))));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    struct RandomRequest {
        GC_HOLDS_NO_CELL_EDGES;

        std::vector<unsigned char> bytes;
        int status { 0 };
    };
}

GC::Ref<JS::Object> create_crypto_host(JS::VM&, JS::Realm& realm, EventLoop& loop)
{
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_direct_property("hmac"_utf16, create_hmac_host(realm), JS::default_attributes);
    host->define_direct_property("hash"_utf16, create_hash_host(realm), JS::default_attributes);
    host->define_native_function(
        realm, "webRandomFill"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            if (!value.is_object() || !is<JS::TypedArrayBase>(value.as_object()))
                return web_crypto_error(
                    vm, "ERR_MITE_WEB_CRYPTO_TYPE", "Expected an integer TypedArray");
            auto& array = static_cast<JS::TypedArrayBase&>(value.as_object());
            switch (array.kind()) {
            case JS::TypedArrayBase::Kind::Int8Array:
            case JS::TypedArrayBase::Kind::Uint8Array:
            case JS::TypedArrayBase::Kind::Uint8ClampedArray:
            case JS::TypedArrayBase::Kind::Int16Array:
            case JS::TypedArrayBase::Kind::Uint16Array:
            case JS::TypedArrayBase::Kind::Int32Array:
            case JS::TypedArrayBase::Kind::Uint32Array:
            case JS::TypedArrayBase::Kind::BigInt64Array:
            case JS::TypedArrayBase::Kind::BigUint64Array:
                break;
            default:
                return web_crypto_error(
                    vm, "ERR_MITE_WEB_CRYPTO_TYPE", "Expected an integer TypedArray");
            }

            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            // Detached and resized-out-of-bounds integer views have zero exposed length in Node.
            if (JS::is_typed_array_out_of_bounds(witness))
                return value;
            auto size = JS::typed_array_byte_length(witness);
            if (size > 65536)
                return web_crypto_error(
                    vm, "ERR_MITE_WEB_CRYPTO_QUOTA", "Random values exceed 65536 bytes");
            if (size) {
                std::vector<unsigned char> bytes(size);
                if (auto status = uv_random(nullptr, nullptr, bytes.data(), size, 0, nullptr);
                    status)
                    return JS::throw_completion(entropy_error(vm, status));
                array.viewed_array_buffer()->overwrite(array.byte_offset(), bytes.data(), size);
            }

            return value;
        },
        1, JS::default_attributes);
    host->define_native_function(realm, "random"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(realm, native_reference(loop)),
            [](auto& realm, auto& captured_loop, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& loop = captured_loop.get();

                auto size = TRY(requested_size(vm));
                auto callback = vm.argument(1);
                if (callback.is_undefined()) {
                    GC::Root result { TRY(JS::Uint8Array::create(realm, size)) };
                    if (size) {
                        auto status = uv_random(nullptr, nullptr,
                            result->viewed_array_buffer()->data_at(0), size, 0, nullptr);
                        if (status)
                            return JS::throw_completion(entropy_error(vm, status));
                    }

                    return result.ptr();
                }

                if (!callback.is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid internal random callback"_utf16);
                auto request = std::make_shared<RandomRequest>();
                request->bytes.resize(size);
                auto complete = GC::create_function(vm.heap(),
                    GC::captures(realm, shared_capture(request), callback),
                    [](auto& realm, auto& request,
                        auto& callback) -> JS::ThrowCompletionOr<JS::Value> {
                        auto& vm = realm.vm();

                        // Do not leave external byte storage in an unreachable GC closure
                        // until a later collection. The completed worker no longer uses it.
                        auto bytes = std::move(request->bytes);
                        if (request->status) {
                            std::vector<unsigned char>().swap(bytes);
                            return JS::call(vm, callback, JS::js_undefined(),
                                entropy_error(vm, request->status));
                        }

                        GC::Root result { TRY(JS::Uint8Array::create(realm, bytes.size())) };
                        if (!bytes.empty())
                            result->viewed_array_buffer()->overwrite(0, bytes.data(), bytes.size());
                        std::vector<unsigned char>().swap(bytes);
                        return JS::call(vm, callback, JS::js_undefined(), JS::js_null(),
                            JS::Value(result.ptr()));
                    });
                TRY(loop.queue_work(
                    [request] {
                        // Worker owns native bytes only; it never touches a VM or JS backing store.
                        if (!request->bytes.empty())
                            request->status = uv_random(nullptr, nullptr, request->bytes.data(),
                                request->bytes.size(), 0, nullptr);
                    },
                    complete));
                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host->define_native_function(
        realm, "uuid"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            unsigned char bytes[16];
            if (auto status = uv_random(nullptr, nullptr, bytes, sizeof(bytes), 0, nullptr); status)
                return JS::throw_completion(entropy_error(vm, status));
            bytes[6] = (bytes[6] & 0x0f) | 0x40;
            bytes[8] = (bytes[8] & 0x3f) | 0x80;
            char uuid[37];
            size_t index = 0;
            constexpr char digits[] = "0123456789abcdef";
            for (size_t i = 0; i < sizeof(bytes); ++i) {
                if (i == 4 || i == 6 || i == 8 || i == 10)
                    uuid[index++] = '-';
                uuid[index++] = digits[bytes[i] >> 4];
                uuid[index++] = digits[bytes[i] & 15];
            }

            uuid[index] = '\0';
            return JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(uuid, index)));
        },
        0, JS::default_attributes);
    return *host;
}
}
