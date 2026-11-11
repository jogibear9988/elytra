/** Parse real OpenSSL certificates and release every provider allocation before returning
 * snapshots. */
#include "platform/WindowsSockets.h"
#include "platform/crypto/Crypto.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cstring>
#include <memory>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <vector>

namespace Mite {
namespace {
    JS::Completion certificate_error(JS::VM& vm, char const* code, char const* message)
    {
        ERR_clear_error();
        auto error = JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message))));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }
}

GC::Ref<JS::Object> create_x509_host(JS::Realm& realm)
{
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(
        realm, "parse"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            if (!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object()))
                return vm.throw_completion<JS::TypeError>(
                    "Certificate parser requires bytes"_utf16);
            auto& array = static_cast<JS::Uint8Array&>(value.as_object());
            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness))
                return vm.throw_completion<JS::TypeError>(
                    "Certificate bytes are detached or out of bounds"_utf16);
            auto size = JS::typed_array_byte_length(witness);
            if (size > 1024 * 1024)
                return vm.throw_completion<JS::RangeError>("Certificate exceeds 1 MiB"_utf16);
            std::vector<unsigned char> bytes(size);
            if (size)
                array.viewed_array_buffer()->with_readonly_bytes(array.byte_offset(), size,
                    [&](ReadonlyBytes input) { memcpy(bytes.data(), input.data(), size); });
            ERR_clear_error();
            auto const* cursor = bytes.data();
            std::unique_ptr<X509, decltype(&X509_free)> certificate(
                d2i_X509(nullptr, &cursor, static_cast<long>(size)), X509_free);
            if (!certificate) {
                ERR_clear_error();
                std::unique_ptr<BIO, decltype(&BIO_free)> input(
                    BIO_new_mem_buf(
                        size ? bytes.data() : reinterpret_cast<unsigned char const*>(""),
                        static_cast<int>(size)),
                    BIO_free);
                if (input)
                    certificate.reset(PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr));
            }
            if (!certificate) {
                auto failure = ERR_peek_last_error();
                char const* code = ERR_GET_LIB(failure) == ERR_LIB_PEM
                        && ERR_GET_REASON(failure) == PEM_R_NO_START_LINE
                    ? "ERR_OSSL_PEM_NO_START_LINE"
                    : "ERR_MITE_X509_PARSE";
                ERR_clear_error();
                return certificate_error(vm, code, "Unable to parse X509 certificate");
            }
            ERR_clear_error();
            std::unique_ptr<BIO, decltype(&BIO_free)> output(BIO_new(BIO_s_mem()), BIO_free);
            if (!output || PEM_write_bio_X509(output.get(), certificate.get()) != 1)
                return certificate_error(
                    vm, "ERR_MITE_X509_ENCODE", "Unable to encode X509 certificate");
            char* pem = nullptr;
            auto pem_size = BIO_get_mem_data(output.get(), &pem);
            int raw_size = i2d_X509(certificate.get(), nullptr);
            if (pem_size <= 0 || pem_size > 2 * 1024 * 1024 || raw_size <= 0
                || raw_size > 1024 * 1024)
                return certificate_error(vm, "ERR_MITE_X509_ENCODE", "Invalid encoded X509 size");
            std::vector<unsigned char> raw(raw_size);
            auto* raw_cursor = raw.data();
            if (i2d_X509(certificate.get(), &raw_cursor) != raw_size)
                return certificate_error(vm, "ERR_MITE_X509_ENCODE", "Unable to encode X509 DER");
            auto& realm = vm.realm();
            GC::Root result { JS::Object::create(realm, nullptr) };
            result->define_direct_property("pem"_utf16,
                JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(pem, pem_size))),
                JS::default_attributes);
            GC::Root data { TRY(JS::Uint8Array::create(realm, raw.size())) };
            data->viewed_array_buffer()->overwrite(0, raw.data(), raw.size());
            result->define_direct_property("raw"_utf16, data.ptr(), JS::default_attributes);
            return result.ptr();
        },
        1, JS::default_attributes);
    return *host;
}
}
