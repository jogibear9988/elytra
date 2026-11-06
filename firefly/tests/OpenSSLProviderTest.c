/* Exercise the shared provider's TLS and EVP boundary, without external network. */
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void check(int condition, char const* message)
{
    if (condition)
        return;
    fprintf(stderr, "FAIL shared OpenSSL: %s\n", message);
    ERR_print_errors_fp(stderr);
    exit(1);
}

static X509* certificate(EVP_PKEY* key)
{
    X509* cert = X509_new();
    check(cert != NULL, "certificate allocation");
    check(X509_set_version(cert, 2) == 1, "certificate version");
    check(ASN1_INTEGER_set(X509_get_serialNumber(cert), 1) == 1, "certificate serial");
    check(X509_gmtime_adj(X509_getm_notBefore(cert), -60) != NULL, "certificate start");
    check(X509_gmtime_adj(X509_getm_notAfter(cert), 600) != NULL, "certificate expiry");
    check(X509_set_pubkey(cert, key) == 1, "certificate key");
    X509_NAME* name = X509_get_subject_name(cert);
    check(X509_NAME_add_entry_by_txt(
              name, "CN", MBSTRING_ASC, (unsigned char const*)"localhost", -1, -1, 0)
            == 1,
        "certificate subject");
    check(X509_set_issuer_name(cert, name) == 1, "certificate issuer");
    X509_EXTENSION* san = X509V3_EXT_conf_nid(NULL, NULL, NID_subject_alt_name, "DNS:localhost");
    check(san != NULL && X509_add_ext(cert, san, -1) == 1, "certificate SAN");
    X509_EXTENSION_free(san);
    check(X509_sign(cert, key, EVP_sha256()) > 0, "certificate signature");
    return cert;
}

static int handshake_step(SSL* connection)
{
    if (SSL_is_init_finished(connection))
        return 1;
    int result = SSL_do_handshake(connection);
    if (result == 1)
        return 1;
    int error = SSL_get_error(connection, result);
    return error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE ? 0 : -1;
}

static void tls(int version, EVP_PKEY* key, X509* cert, int wrong_hostname)
{
    SSL_CTX* server_context = SSL_CTX_new(TLS_server_method());
    SSL_CTX* client_context = SSL_CTX_new(TLS_client_method());
    check(server_context != NULL && client_context != NULL, "TLS contexts");
    check(SSL_CTX_set_min_proto_version(server_context, version) == 1
            && SSL_CTX_set_max_proto_version(server_context, version) == 1,
        "server protocol");
    check(SSL_CTX_set_min_proto_version(client_context, version) == 1
            && SSL_CTX_set_max_proto_version(client_context, version) == 1,
        "client protocol");
    check(SSL_CTX_use_certificate(server_context, cert) == 1
            && SSL_CTX_use_PrivateKey(server_context, key) == 1,
        "server identity");
    check(X509_STORE_add_cert(SSL_CTX_get_cert_store(client_context), cert) == 1, "client trust");
    SSL_CTX_set_verify(client_context, SSL_VERIFY_PEER, NULL);
    SSL* server = SSL_new(server_context);
    SSL* client = SSL_new(client_context);
    check(server != NULL && client != NULL, "TLS connections");
    check(SSL_set1_host(client, wrong_hostname ? "wrong.example" : "localhost") == 1,
        "hostname validation");
    BIO* server_bio = NULL;
    BIO* client_bio = NULL;
    check(BIO_new_bio_pair(&server_bio, 0, &client_bio, 0) == 1, "TLS transport");
    SSL_set_bio(server, server_bio, server_bio);
    SSL_set_bio(client, client_bio, client_bio);
    SSL_set_accept_state(server);
    SSL_set_connect_state(client);
    int rejected = 0;
    for (int step = 0;
        step < 256 && (!SSL_is_init_finished(server) || !SSL_is_init_finished(client)); ++step) {
        if (handshake_step(client) < 0) {
            check(wrong_hostname && SSL_get_verify_result(client) == X509_V_ERR_HOSTNAME_MISMATCH,
                "expected hostname rejection");
            rejected = 1;
            ERR_clear_error();
            break;
        }

        check(handshake_step(server) >= 0, "server handshake");
    }

    if (wrong_hostname) {
        check(rejected, "wrong hostname must fail verification");
        goto cleanup;
    }

    check(SSL_is_init_finished(server) && SSL_is_init_finished(client),
        "bounded handshake completion");
    check(SSL_version(client) == version && SSL_get_verify_result(client) == X509_V_OK,
        "verified negotiated protocol");
    char const payload[] = "Firefly shared TLS provider\0binary";
    char buffer[sizeof(payload)] = { 0 };
    size_t written = 0;
    size_t received = 0;
    check(
        SSL_write_ex(client, payload, sizeof(payload), &written) == 1 && written == sizeof(payload),
        "TLS write");
    check(SSL_read_ex(server, buffer, sizeof(buffer), &received) == 1 && received == sizeof(payload)
            && memcmp(payload, buffer, sizeof(payload)) == 0,
        "TLS read");
cleanup:
    SSL_free(client);
    SSL_free(server);
    SSL_CTX_free(client_context);
    SSL_CTX_free(server_context);
}

int main(void)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int size = 0;
    unsigned char const expected[] = { 0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41,
        0x40, 0xde, 0x5d, 0xae, 0x22, 0x23, 0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4,
        0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad };
    check(EVP_Digest("abc", 3, digest, &size, EVP_sha256(), NULL) == 1 && size == sizeof(expected)
            && memcmp(digest, expected, sizeof(expected)) == 0,
        "SHA-256 known answer");
    EVP_PKEY* key = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "prime256v1");
    check(key != NULL, "EC key generation");
    X509* cert = certificate(key);
    tls(TLS1_2_VERSION, key, cert, 0);
    tls(TLS1_3_VERSION, key, cert, 0);
    tls(TLS1_3_VERSION, key, cert, 1);
    X509_free(cert);
    EVP_PKEY_free(key);
    puts("PASS shared OpenSSL SHA-256 and verified TLS 1.2/1.3 encrypted binary round trips; wrong "
         "hostname rejected");
    return 0;
}
