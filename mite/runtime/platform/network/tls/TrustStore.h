/** Populate OpenSSL's default client trust from the host without copying roots into apps. */
#pragma once
#include <openssl/ssl.h>

namespace Mite {
bool load_tls_system_trust(SSL_CTX*);
}
