/** Keep OS trust loading separate from the VM/record adapter and fail closed on errors. */
#include "platform/network/tls/TrustStore.h"
#include <initializer_list>
#include <openssl/err.h>
#include <openssl/x509.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#endif

namespace Mite {
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
