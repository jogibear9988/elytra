/** Keep OS trust loading separate from the VM/record adapter and fail closed on errors. */
#include "platform/network/tls/TrustStore.h"
#include "platform/WindowsSockets.h"
#include <initializer_list>
#include <memory>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

namespace Mite {
bool tls_system_certificates(std::vector<std::string>& certificates)
{
#ifdef __APPLE__
    // Use a fresh real trust store, not a bundled list or cached package data.
    // It snapshots this query's anchors without changing any live TLS context.
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(
        SSL_CTX_new(TLS_client_method()), SSL_CTX_free);
    if (!context || !load_tls_system_trust(context.get()))
        return false;
    auto* objects = X509_STORE_get0_objects(SSL_CTX_get_cert_store(context.get()));
    auto count = sk_X509_OBJECT_num(objects);
    if (count < 0 || count > 4096)
        return false;
    std::vector<std::string> result;
    size_t total = 0;
    for (int index = 0; index < count; ++index) {
        auto* object = sk_X509_OBJECT_value(objects, index);
        if (X509_OBJECT_get_type(object) != X509_LU_X509)
            continue;
        std::unique_ptr<BIO, decltype(&BIO_free)> output(BIO_new(BIO_s_mem()), BIO_free);
        if (!output || PEM_write_bio_X509(output.get(), X509_OBJECT_get0_X509(object)) != 1)
            return false;
        char* bytes = nullptr;
        auto size = BIO_get_mem_data(output.get(), &bytes);
        if (!bytes || size <= 0 || size > 128 * 1024
            || static_cast<size_t>(size) > 16 * 1024 * 1024 - total)
            return false;
        result.emplace_back(bytes, static_cast<size_t>(size));
        total += static_cast<size_t>(size);
    }
    certificates = std::move(result);
    return true;
#else
    (void)certificates;
    return false;
#endif
}

bool load_tls_system_trust(SSL_CTX* context)
{
#ifdef _WIN32
    // OpenSSL's native store provider applies Windows ROOT trust configuration.
    return SSL_CTX_load_verify_store(context, "org.openssl.winstore://") == 1;
#elif defined(__APPLE__)
    CFArrayRef anchors = nullptr;
    if (SecTrustCopyAnchorCertificates(&anchors) != errSecSuccess || !anchors)
        return false;
    size_t loaded = 0;
    auto* store = SSL_CTX_get_cert_store(context);
    for (CFIndex index = 0; index < CFArrayGetCount(anchors); ++index) {
        auto certificate = static_cast<SecCertificateRef>(
            const_cast<void*>(CFArrayGetValueAtIndex(anchors, index)));
        auto data = SecCertificateCopyData(certificate);
        if (!data)
            continue;
        auto* cursor = CFDataGetBytePtr(data);
        auto* x509 = d2i_X509(nullptr, &cursor, CFDataGetLength(data));
        if (x509) {
            if (X509_STORE_add_cert(store, x509) == 1)
                ++loaded;
            X509_free(x509);
        }

        CFRelease(data);
    }

    CFRelease(anchors);
    ERR_clear_error();
    return loaded > 0;
#else
    // vcpkg OpenSSL may have a relocatable prefix rather than the distribution's
    // configured CA path. Load the standard distribution bundle/directory too.
    bool loaded = SSL_CTX_set_default_verify_paths(context) == 1;
    auto* store = SSL_CTX_get_cert_store(context);
    for (auto path : { "/etc/ssl/certs/ca-certificates.crt", "/etc/pki/tls/certs/ca-bundle.crt" }) {
        if (X509_STORE_load_file(store, path) == 1)
            loaded = true;
    }

    if (X509_STORE_load_path(store, "/etc/ssl/certs") == 1)
        loaded = true;
    ERR_clear_error();
    return loaded;
#endif
}
}
