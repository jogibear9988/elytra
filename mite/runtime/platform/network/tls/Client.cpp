/**
 * Own OpenSSL contexts/records without an OpenSSL socket or a second event loop.
 * Every operation returns owned byte snapshots; no provider retains JS buffers.
 */
#include "platform/network/tls/Client.h"
#include "core/GCCaptures.h"
#include "platform/WindowsSockets.h"
#include "platform/network/tls/TrustStore.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <memory>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <string>
#include <vector>

namespace Mite {
namespace {
    constexpr size_t record_limit = 1024 * 1024;

    JS::Value text(JS::VM& vm, char const* value)
    {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(value, strlen(value))));
    }

    JS::Completion failure(JS::VM& vm, char const* code, char const* message)
    {
        auto error = JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message))));
        error->define_direct_property("code"_utf16, text(vm, code), JS::default_attributes);
        return JS::throw_completion(error);
    }

    char const* verification_code(long result)
    {
        switch (result) {
        case X509_V_OK:
            return "";
        case X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT:
            return "DEPTH_ZERO_SELF_SIGNED_CERT";
        case X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN:
            return "SELF_SIGNED_CERT_IN_CHAIN";
        case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
            return "UNABLE_TO_GET_ISSUER_CERT_LOCALLY";
        case X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE:
            return "UNABLE_TO_VERIFY_LEAF_SIGNATURE";
        case X509_V_ERR_CERT_HAS_EXPIRED:
            return "CERT_HAS_EXPIRED";
        case X509_V_ERR_CERT_NOT_YET_VALID:
            return "CERT_NOT_YET_VALID";
        case X509_V_ERR_HOSTNAME_MISMATCH:
        case X509_V_ERR_IP_ADDRESS_MISMATCH:
            return "ERR_TLS_CERT_ALTNAME_INVALID";
        default:
            return "ERR_TLS_CERTIFICATE_VERIFY_FAILED";
        }
    }

    struct Client {
        GC_HOLDS_NO_CELL_EDGES;

        SSL_CTX* context { nullptr };
        SSL* ssl { nullptr };
        bool ready { false };
        bool ended { false };

        ~Client()
        {
            close();
        }

        void close()
        {
            SSL_free(ssl);
            ssl = nullptr;
            SSL_CTX_free(context);
            context = nullptr;
        }

        JS::ThrowCompletionOr<void> status(JS::VM& vm, int result)
        {
            if (result > 0)
                return {};
            // Call immediately after the SSL operation, before any provider API
            // changes this thread's error queue. WANT_READ is ordinary progress.
            int kind = SSL_get_error(ssl, result);
            if (kind == SSL_ERROR_WANT_READ || kind == SSL_ERROR_WANT_WRITE)
                return {};
            if (kind == SSL_ERROR_ZERO_RETURN) {
                ended = true;
                return {};
            }

            auto verify = SSL_get_verify_result(ssl);
            if (verify != X509_V_OK)
                return failure(
                    vm, verification_code(verify), X509_verify_cert_error_string(verify));
            auto reason = ERR_reason_error_string(ERR_peek_last_error());
            std::string code = "ERR_SSL_";
            for (char ch : std::string(reason ? reason : "TLS protocol error"))
                code += std::isalnum(static_cast<unsigned char>(ch))
                    ? std::toupper(static_cast<unsigned char>(ch))
                    : '_';
            return failure(vm, code.c_str(), reason ? reason : "TLS protocol error");
        }
    };

    JS::ThrowCompletionOr<std::string> string(JS::VM& vm, JS::Value value)
    {
        if (!value.is_string())
            return vm.throw_completion<JS::TypeError>("Invalid internal TLS string"_utf16);
        auto view = value.as_string().utf16_string_view();
        if (!view.validate())
            return failure(vm, "ERR_INVALID_ARG_VALUE", "TLS strings must be well-formed UTF-16");
        auto bytes = MUST(view.to_byte_string());
        return std::string(bytes.characters(), bytes.length());
    }

    JS::ThrowCompletionOr<std::vector<unsigned char>> bytes(JS::VM& vm, JS::Value value)
    {
        if (value.is_undefined())
            return std::vector<unsigned char> {};
        if (!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Invalid internal TLS bytes"_utf16);
        auto& array = static_cast<JS::Uint8Array&>(value.as_object());
        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);
        if (JS::is_typed_array_out_of_bounds(witness))
            return vm.throw_completion<JS::TypeError>("Detached TLS input"_utf16);
        auto size = JS::typed_array_byte_length(witness);
        if (size > record_limit)
            return failure(vm, "ERR_TLS_BUFFER_LIMIT", "TLS record operation exceeds one MiB");
        return array.viewed_array_buffer()->with_readonly_bytes(
            array.byte_offset(), size, [](ReadonlyBytes data) {
                return std::vector<unsigned char>(data.begin(), data.end());
            });
    }

    GC::Ref<JS::Uint8Array> snapshot(JS::Realm& realm, std::vector<unsigned char> const& bytes)
    {
        auto result = MUST(JS::Uint8Array::create(realm, bytes.size()));
        if (!bytes.empty())
            result->viewed_array_buffer()->overwrite(0, bytes.data(), bytes.size());
        return result;
    }

    JS::ThrowCompletionOr<JS::Value> step(JS::VM& vm, Client& client, int operation)
    {
        if (!client.ssl)
            return failure(vm, "ERR_TLS_INVALID_STATE", "TLS context is closed");
        auto input = TRY(bytes(vm, vm.argument(0)));
        size_t written = 0;
        ERR_clear_error();
        if (operation == 0 && !input.empty()) {
            if (BIO_ctrl_pending(SSL_get_rbio(client.ssl)) + input.size() > record_limit)
                return failure(vm, "ERR_TLS_BUFFER_LIMIT", "TLS pending input exceeds one MiB");
            if (BIO_write(SSL_get_rbio(client.ssl), input.data(), input.size())
                != static_cast<int>(input.size()))
                return failure(vm, "ERR_TLS_WRITE_FAILED", "TLS encrypted input failed");
        }

        if (!client.ready) {
            int result = SSL_do_handshake(client.ssl);
            TRY(client.status(vm, result));
            client.ready = result == 1;
        }

        if (client.ready && operation == 1 && !input.empty()) {
            ERR_clear_error();
            int result = SSL_write_ex(
                client.ssl, input.data(), std::min(input.size(), size_t { 16384 }), &written);
            TRY(client.status(vm, result));
        }

        if (client.ready && operation == 2) {
            ERR_clear_error();
            int result = SSL_shutdown(client.ssl);
            if (result < 0)
                TRY(client.status(vm, result));
        }

        std::vector<unsigned char> plain;
        if (client.ready && !client.ended) {
            unsigned char buffer[16384];
            while (plain.size() < record_limit) {
                size_t size = 0;
                ERR_clear_error();
                int result = SSL_read_ex(client.ssl, buffer,
                    std::min(sizeof(buffer), record_limit - plain.size()), &size);
                TRY(client.status(vm, result));
                if (result != 1)
                    break;
                plain.insert(plain.end(), buffer, buffer + size);
            }
        }

        auto pending = BIO_ctrl_pending(SSL_get_wbio(client.ssl));
        if (pending > record_limit)
            return failure(vm, "ERR_TLS_BUFFER_LIMIT", "TLS pending output exceeds one MiB");
        std::vector<unsigned char> cipher(pending);
        if (pending
            && BIO_read(SSL_get_wbio(client.ssl), cipher.data(), static_cast<int>(pending))
                != static_cast<int>(pending))
            return failure(vm, "ERR_TLS_READ_FAILED", "TLS encrypted output failed");
        auto& realm = vm.realm();
        GC::Root result { JS::Object::create(realm, nullptr) };
        result->define_direct_property(
            "output"_utf16, snapshot(realm, cipher), JS::default_attributes);
        result->define_direct_property(
            "data"_utf16, snapshot(realm, plain), JS::default_attributes);
        result->define_direct_property(
            "written"_utf16, JS::Value(static_cast<double>(written)), JS::default_attributes);
        result->define_direct_property(
            "ready"_utf16, JS::Value(client.ready), JS::default_attributes);
        result->define_direct_property(
            "ended"_utf16, JS::Value(client.ended), JS::default_attributes);
        if (client.ready) {
            auto verify = SSL_get_verify_result(client.ssl);
            result->define_direct_property(
                "authorized"_utf16, JS::Value(verify == X509_V_OK), JS::default_attributes);
            result->define_direct_property("authorizationError"_utf16,
                text(vm, verification_code(verify)), JS::default_attributes);
            result->define_direct_property(
                "protocol"_utf16, text(vm, SSL_get_version(client.ssl)), JS::default_attributes);
            unsigned char const* alpn = nullptr;
            unsigned alpn_size = 0;
            SSL_get0_alpn_selected(client.ssl, &alpn, &alpn_size);
            result->define_direct_property("alpn"_utf16,
                JS::PrimitiveString::create(vm,
                    Utf16String::from_utf8(alpn_size
                            ? StringView(reinterpret_cast<char const*>(alpn), alpn_size)
                            : ""sv)),
                JS::default_attributes);
        }

        return result.ptr();
    }

    JS::ThrowCompletionOr<JS::Value> create(JS::VM& vm)
    {
        auto identity = TRY(string(vm, vm.argument(0)));
        auto servername = TRY(string(vm, vm.argument(1)));
        auto ca = vm.argument(2);
        auto reject = vm.argument(3).to_boolean();
        auto minimum = vm.argument(4).as_i32();
        auto maximum = vm.argument(5).as_i32();
        auto alpn = TRY(bytes(vm, vm.argument(6)));
        if (identity.empty() || identity.find('\0') != std::string::npos
            || servername.find('\0') != std::string::npos)
            return failure(vm, "ERR_INVALID_ARG_VALUE", "Invalid TLS peer identity");
        auto client = std::make_shared<Client>();
        client->context = SSL_CTX_new(TLS_client_method());
        if (!client->context)
            return failure(vm, "ERR_TLS_CONTEXT", "Could not allocate TLS context");
        SSL_CTX_set_options(client->context, SSL_OP_NO_RENEGOTIATION);
        if (SSL_CTX_set_min_proto_version(client->context, minimum) != 1
            || SSL_CTX_set_max_proto_version(client->context, maximum) != 1)
            return failure(vm, "ERR_TLS_INVALID_PROTOCOL_VERSION", "Invalid TLS version limits");
        SSL_CTX_set_verify(
            client->context, SSL_VERIFY_PEER,
            reject ? nullptr : +[](int, X509_STORE_CTX*) { return 1; });
        if (ca.is_undefined()) {
            if (!load_tls_system_trust(client->context))
                return failure(vm, "ERR_TLS_TRUST_STORE", "System TLS trust store is unavailable");
        } else {
            auto pem = TRY(string(vm, ca));
            auto* bio = BIO_new_mem_buf(pem.data(), static_cast<int>(pem.size()));
            if (!bio)
                return failure(vm, "ERR_TLS_INVALID_CA", "Could not allocate CA input");
            size_t loaded = 0;
            while (auto* certificate = PEM_read_bio_X509(bio, nullptr, nullptr, nullptr)) {
                if (X509_STORE_add_cert(SSL_CTX_get_cert_store(client->context), certificate) == 1)
                    ++loaded;
                X509_free(certificate);
            }

            BIO_free(bio);
            ERR_clear_error();
            if (!pem.empty() && !loaded)
                return failure(
                    vm, "ERR_TLS_INVALID_CA", "CA input contains no valid PEM certificate");
        }

        client->ssl = SSL_new(client->context);
        if (!client->ssl)
            return failure(vm, "ERR_TLS_CONTEXT", "Could not allocate TLS connection");
        auto* input = BIO_new(BIO_s_mem());
        auto* output = BIO_new(BIO_s_mem());
        if (!input || !output) {
            BIO_free(input);
            BIO_free(output);
            return failure(vm, "ERR_TLS_CONTEXT", "Could not allocate TLS record buffers");
        }

        BIO_set_mem_eof_return(input, -1);
        SSL_set_bio(client->ssl, input, output);
        SSL_set_connect_state(client->ssl);
        auto* parameters = SSL_get0_param(client->ssl);
        X509_VERIFY_PARAM_set_hostflags(
            parameters, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
        // inet parsing accepts IP SANs; host names use DNS SAN matching only.
        if (X509_VERIFY_PARAM_set1_ip_asc(parameters, identity.c_str()) != 1) {
            ERR_clear_error();
            if (SSL_set1_host(client->ssl, identity.c_str()) != 1)
                return failure(
                    vm, "ERR_TLS_CONTEXT", "Could not configure TLS hostname verification");
        }

        if (!servername.empty() && SSL_set_tlsext_host_name(client->ssl, servername.c_str()) != 1)
            return failure(vm, "ERR_TLS_CONTEXT", "Could not configure TLS server name");
        if (!alpn.empty()
            && SSL_set_alpn_protos(client->ssl, alpn.data(), static_cast<unsigned>(alpn.size()))
                != 0)
            return failure(vm, "ERR_TLS_CONTEXT", "Could not configure ALPN");
        auto& realm = vm.realm();
        GC::Root handle { JS::Object::create(realm, nullptr) };
        for (auto [name, operation] :
            { std::pair { "receive", 0 }, { "write", 1 }, { "shutdown", 2 } }) {
            handle->define_native_function(realm,
                Utf16String::from_utf8(StringView(name, strlen(name))),
                GC::create_function(realm.vm().heap(),
                    GC::captures(shared_capture(client), operation),
                    [](auto& client, auto& operation, JS::VM& vm) {
                        return step(vm, *client, operation);
                    }),
                1, JS::default_attributes);
        }

        handle->define_native_function(realm, "close"_utf16,
            GC::create_function(realm.vm().heap(), GC::captures(shared_capture(client)),
                [](auto& client, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                    client->close();
                    return JS::js_undefined();
                }),
            0, JS::default_attributes);
        return handle.ptr();
    }
}

GC::Ref<JS::Object> create_tls_host(JS::Realm& realm)
{
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(realm, "create"_utf16, create, 7, JS::default_attributes);
    return host;
}
}
