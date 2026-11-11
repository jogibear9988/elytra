/**
 * Owns OpenSSL HMAC contexts and fixed-length byte comparison. Key bytes are consumed at
 * construction; finalization or collection releases the provider state without exposing it to
 * JavaScript.
 */

#include "core/GCCaptures.h"
#include "platform/WindowsSockets.h"
#include "platform/crypto/Crypto.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <array>
#include <cstring>
#include <memory>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/params.h>

namespace Mite {
namespace {
    template <typename ErrorType = JS::Error>
    JS::Completion hmac_error(JS::VM& vm, char const* code, Utf16View message)
    {
        auto error = ErrorType::create(vm.realm(), message);
        if (code)
            error->define_direct_property("code"_utf16,
                JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
                JS::default_attributes);
        return JS::throw_completion(error);
    }

    struct HmacState {
        GC_HOLDS_NO_CELL_EDGES;

        EVP_MAC_CTX* context { nullptr };

        ~HmacState()
        {
            close();
        }

        void close()
        {
            EVP_MAC_CTX_free(context);
            context = nullptr;
        }
    };

    JS::ThrowCompletionOr<JS::Uint8Array*> bytes_argument(JS::VM& vm, size_t index)
    {
        auto value = vm.argument(index);
        if (!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Invalid internal crypto bytes"_utf16);
        return &static_cast<JS::Uint8Array&>(value.as_object());
    }

    size_t byte_length(JS::Uint8Array& array)
    {
        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);
        return JS::is_typed_array_out_of_bounds(witness) ? 0 : JS::typed_array_byte_length(witness);
    }

    GC::Ref<JS::Object> hmac_handle(JS::Realm& realm, std::shared_ptr<HmacState> state)
    {
        GC::Root handle { JS::Object::create(realm, nullptr) };
        handle->define_native_function(realm, "update"_utf16,
            GC::create_function(realm.vm().heap(), GC::captures(shared_capture(state)),
                [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto* array = TRY(bytes_argument(vm, 0));
                    if (!state->context)
                        return JS::Value(false);
                    auto size = byte_length(*array);
                    ERR_clear_error();
                    auto status = size
                        ? array->viewed_array_buffer()->with_readonly_bytes(array->byte_offset(),
                              size,
                              [&](ReadonlyBytes bytes) {
                                  return EVP_MAC_update(state->context, bytes.data(), bytes.size());
                              })
                        : EVP_MAC_update(state->context, nullptr, 0);
                    return JS::Value(status == 1);
                }),
            1, JS::default_attributes);
        handle->define_native_function(realm, "digest"_utf16,
            GC::create_function(realm.vm().heap(), GC::captures(shared_capture(state), realm),
                [](auto& state, auto& realm, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    if (!state->context)
                        return hmac_error(
                            vm, "ERR_CRYPTO_HASH_FINALIZED", u"Digest already called"sv);
                    // Fixed-output HMAC only. Never expose unwritten stack/provider storage.
                    std::array<unsigned char, EVP_MAX_MD_SIZE> output {};
                    size_t size = 0;
                    ERR_clear_error();
                    auto status
                        = EVP_MAC_final(state->context, output.data(), &size, output.size());
                    state->close();
                    if (status != 1 || size > output.size())
                        return hmac_error(
                            vm, "ERR_CRYPTO_HASH_FINALIZED", u"HMAC finalization failed"sv);
                    GC::Root bytes { TRY(JS::Uint8Array::create(realm, size)) };
                    if (size)
                        bytes->viewed_array_buffer()->overwrite(0, output.data(), size);
                    return bytes.ptr();
                }),
            0, JS::default_attributes);
        return *handle;
    }
}

GC::Ref<JS::Object> create_hmac_host(JS::Realm& realm)
{
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(realm, "create"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(realm),
            [](auto& realm, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_string())
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid internal HMAC algorithm"_utf16);
                auto* key = TRY(bytes_argument(vm, 1));
                auto name = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
                ERR_clear_error();
                auto* digest = EVP_get_digestbyname(name.characters());
                if (!digest)
                    return hmac_error<JS::TypeError>(
                        vm, "ERR_CRYPTO_INVALID_DIGEST", u"Invalid HMAC digest"sv);
                // SHAKE/XOF is not a valid HMAC digest, including in the Node reference.
                if ((EVP_MD_get_flags(digest) & EVP_MD_FLAG_XOF) != 0)
                    return hmac_error(vm, nullptr, u"HMAC requires a fixed-output digest"sv);
                auto* method = EVP_MAC_fetch(nullptr, "HMAC", nullptr);
                if (!method)
                    return hmac_error(
                        vm, "ERR_OSSL_EVP_UNSUPPORTED", u"HMAC provider is unavailable"sv);
                auto state = std::make_shared<HmacState>();
                state->context = EVP_MAC_CTX_new(method);
                EVP_MAC_free(method);
                if (!state->context)
                    return hmac_error(
                        vm, "ERR_CRYPTO_HASH_CREATION_FAILED", u"Cannot allocate HMAC context"sv);
                auto* digest_name = const_cast<char*>(EVP_MD_get0_name(digest));
                OSSL_PARAM params[]
                    = { OSSL_PARAM_construct_utf8_string(OSSL_MAC_PARAM_DIGEST, digest_name, 0),
                          OSSL_PARAM_construct_end() };
                auto size = byte_length(*key);
                // nullptr means reuse an existing key to EVP_MAC_init; an empty key must
                // instead be passed with a non-null pointer and length zero.
                unsigned char empty_key = 0;
                auto status = size
                    ? key->viewed_array_buffer()->with_readonly_bytes(key->byte_offset(), size,
                          [&](ReadonlyBytes bytes) {
                              return EVP_MAC_init(
                                  state->context, bytes.data(), bytes.size(), params);
                          })
                    : EVP_MAC_init(state->context, &empty_key, 0, params);
                if (status != 1)
                    return hmac_error(
                        vm, "ERR_OSSL_EVP_UNSUPPORTED", u"HMAC digest provider is unavailable"sv);
                if (EVP_MAC_CTX_get_mac_size(state->context) > EVP_MAX_MD_SIZE)
                    return hmac_error(vm, "ERR_MITE_UNSUPPORTED_API",
                        u"HMAC provider output exceeds supported digest size"sv);
                return hmac_handle(realm, std::move(state));
            }),
        2, JS::default_attributes);
    host->define_native_function(
        realm, "equal"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* left = TRY(bytes_argument(vm, 0));
            auto* right = TRY(bytes_argument(vm, 1));
            auto size = byte_length(*left);
            if (size != byte_length(*right))
                return hmac_error<JS::RangeError>(vm, "ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH",
                    u"Input buffers must have the same byte length"sv);
            if (!size)
                return JS::Value(true);
            // The same fixed byte count reaches CRYPTO_memcmp for equal-length data;
            // no JS callback, coercion or content-dependent early exit occurs here.
            bool equal = left->viewed_array_buffer()->with_readonly_bytes(
                left->byte_offset(), size, [&](ReadonlyBytes a) {
                    return right->viewed_array_buffer()->with_readonly_bytes(
                        right->byte_offset(), size, [&](ReadonlyBytes b) {
                            return CRYPTO_memcmp(a.data(), b.data(), size) == 0;
                        });
                });
            return JS::Value(equal);
        },
        2, JS::default_attributes);
    return *host;
}
}
