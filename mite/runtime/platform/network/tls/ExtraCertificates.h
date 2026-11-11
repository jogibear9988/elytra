/** Immutable process-start NODE_EXTRA_CA_CERTS input, shared by queries and real TLS. */
#pragma once
#include "platform/WindowsSockets.h"
#include <openssl/ssl.h>
#include <string>
#include <vector>

namespace Mite {
struct TLSExtraCertificates {
    std::vector<std::string> certificates;
    std::string error;
};

void capture_tls_extra_certificates();

TLSExtraCertificates const& tls_extra_certificates();

bool load_tls_extra_certificates(SSL_CTX*);
}
