/** Populate OpenSSL's default client trust from the host without copying roots into apps. */
#pragma once
#include "platform/WindowsSockets.h"
#include <openssl/ssl.h>
#include <string>
#include <vector>

namespace Mite {
bool load_tls_system_trust(SSL_CTX*);

// Enumerate the same macOS anchors loaded into default TLS clients. Other
// backends can load roots lazily and must not return an incomplete catalog.
bool tls_system_certificates(std::vector<std::string>&);
}
