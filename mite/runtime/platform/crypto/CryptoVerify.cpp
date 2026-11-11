/** All key/BN/digest allocations are native RAII; only independent DER snapshots cross to JS. */
#include "platform/crypto/CryptoVerify.h"
#include "platform/WindowsSockets.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <array>
#include <cstring>
#include <memory>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/params.h>
#include <openssl/x509.h>
#include <vector>

namespace Mite {
namespace {
    JS::Completion failure(JS::VM& vm, Utf16View message)
    {
        ERR_clear_error();
        auto error = JS::Error::create(vm.realm(), message);
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, u"ERR_CRYPTO_OPERATION_FAILED"sv),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<std::vector<unsigned char>> bytes(
        JS::VM& vm, size_t index, size_t maximum)
    {
        auto value = vm.argument(index);
        if (!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Verification requires byte snapshots"_utf16);
        auto& array = static_cast<JS::Uint8Array&>(value.as_object());
        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);
        if (JS::is_typed_array_out_of_bounds(witness))
            return vm.throw_completion<JS::TypeError>("Verification bytes are detached"_utf16);
        auto size = JS::typed_array_byte_length(witness);
        if (size > maximum)
            return vm.throw_completion<JS::RangeError>("Verification byte budget exceeded"_utf16);
        std::vector<unsigned char> result(size);
        if (size)
            array.viewed_array_buffer()->with_readonly_bytes(array.byte_offset(), size,
                [&](ReadonlyBytes input) { memcpy(result.data(), input.data(), size); });
        return result;
    }

    using Key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
    using Context = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;

    bool valid(Key const& key)
    {
        if (!key || !EVP_PKEY_is_a(key.get(), "EC"))
            return false;
        char group[64];
        size_t size = 0;
        if (EVP_PKEY_get_utf8_string_param(
                key.get(), OSSL_PKEY_PARAM_GROUP_NAME, group, sizeof(group), &size)
                != 1
            || size != strlen("prime256v1") || strcmp(group, "prime256v1"))
            return false;
        Context check(EVP_PKEY_CTX_new(key.get(), nullptr), EVP_PKEY_CTX_free);
        return check && EVP_PKEY_public_check(check.get()) == 1;
    }
}

GC::Ref<JS::Object> create_verify_host(JS::Realm& realm)
{
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(
        realm, "create"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto x = TRY(bytes(vm, 0, 32));
            auto y = TRY(bytes(vm, 1, 32));
            if (x.size() != 32 || y.size() != 32)
                return failure(vm, u"P-256 coordinates must be 32 bytes"sv);
            std::array<unsigned char, 65> point {};
            point[0] = 4;
            memcpy(point.data() + 1, x.data(), 32);
            memcpy(point.data() + 33, y.data(), 32);
            char group[] = "prime256v1";
            OSSL_PARAM params[] { OSSL_PARAM_construct_utf8_string(
                                      OSSL_PKEY_PARAM_GROUP_NAME, group, 0),
                OSSL_PARAM_construct_octet_string(
                    OSSL_PKEY_PARAM_PUB_KEY, point.data(), point.size()),
                OSSL_PARAM_construct_end() };
            ERR_clear_error();
            Context context(EVP_PKEY_CTX_new_from_name(nullptr, "EC", nullptr), EVP_PKEY_CTX_free);
            EVP_PKEY* raw = nullptr;
            if (!context || EVP_PKEY_fromdata_init(context.get()) != 1
                || EVP_PKEY_fromdata(context.get(), &raw, EVP_PKEY_PUBLIC_KEY, params) != 1) {
                EVP_PKEY_free(raw);
                return failure(vm, u"Invalid P-256 public key"sv);
            }
            Key key(raw, EVP_PKEY_free);
            if (!valid(key))
                return failure(vm, u"Invalid P-256 public point"sv);
            int size = i2d_PUBKEY(key.get(), nullptr);
            if (size <= 0 || size > 1024)
                return failure(vm, u"Unable to encode P-256 public key"sv);
            std::vector<unsigned char> der(size);
            auto* cursor = der.data();
            if (i2d_PUBKEY(key.get(), &cursor) != size)
                return failure(vm, u"Unable to encode public key"sv);
            GC::Root result { TRY(JS::Uint8Array::create(vm.realm(), der.size())) };
            result->viewed_array_buffer()->overwrite(0, der.data(), der.size());
            ERR_clear_error();
            return result.ptr();
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "verify"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto der = TRY(bytes(vm, 0, 1024));
            auto data = TRY(bytes(vm, 1, 1024 * 1024));
            auto signature = TRY(bytes(vm, 2, 64));
            if (signature.size() != 64)
                return JS::Value(false);
            if (der.empty())
                return failure(vm, u"Empty public-key snapshot"sv);
            auto const* cursor = der.data();
            ERR_clear_error();
            Key key(d2i_PUBKEY(nullptr, &cursor, static_cast<long>(der.size())), EVP_PKEY_free);
            if (cursor != der.data() + der.size() || !valid(key))
                return failure(vm, u"Invalid private public-key snapshot"sv);
            std::unique_ptr<ECDSA_SIG, decltype(&ECDSA_SIG_free)> parsed(
                ECDSA_SIG_new(), ECDSA_SIG_free);
            std::unique_ptr<BIGNUM, decltype(&BN_free)> r(
                BN_bin2bn(signature.data(), 32, nullptr), BN_free);
            std::unique_ptr<BIGNUM, decltype(&BN_free)> s(
                BN_bin2bn(signature.data() + 32, 32, nullptr), BN_free);
            if (!parsed || !r || !s || ECDSA_SIG_set0(parsed.get(), r.get(), s.get()) != 1)
                return failure(vm, u"Unable to decode signature"sv);
            r.release();
            s.release();
            int size = i2d_ECDSA_SIG(parsed.get(), nullptr);
            if (size <= 0 || size > 80)
                return failure(vm, u"Invalid encoded signature size"sv);
            std::vector<unsigned char> encoded(size);
            auto* output = encoded.data();
            if (i2d_ECDSA_SIG(parsed.get(), &output) != size)
                return failure(vm, u"Unable to encode signature"sv);
            std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> digest(
                EVP_MD_CTX_new(), EVP_MD_CTX_free);
            if (!digest
                || EVP_DigestVerifyInit(digest.get(), nullptr, EVP_sha256(), nullptr, key.get())
                    != 1)
                return failure(vm, u"Unable to initialize SHA-256 verification"sv);
            int verified = EVP_DigestVerify(
                digest.get(), encoded.data(), encoded.size(), data.data(), data.size());
            if (verified < 0)
                return failure(vm, u"Signature verification provider failed"sv);
            ERR_clear_error();
            return JS::Value(verified == 1);
        },
        3, JS::default_attributes);
    return *host;
}
}
