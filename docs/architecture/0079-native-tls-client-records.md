# 0079: Owner-thread OpenSSL TLS records over existing TCP

Implement the first TLS client slice using OpenSSL already present in the build.
A memory-BIO adapter owns SSL_CTX/SSL state and returns copied encrypted/plaintext
snapshots. Libuv TCP remains the only transport/event loop; no OpenSSL socket or
LibWeb/Node/V8 dependency is added. No Ladybird source changes are required.

TLS 1.2/1.3 handshake, certificate-chain and DNS/IP SAN verification precede
secureConnect/application data. Disable renegotiation and CN fallback; support
full-label wildcards only. Explicit PEM ca replaces default anchors. Default trust
loads macOS Security anchors, OpenSSL's native Windows ROOT provider or distribution
OpenSSL/system bundles on Linux. These anchor sets intentionally differ from Node's
bundled Mozilla CA set; this is not a complete OS trust-settings/revocation policy.
rejectUnauthorized:false still records verification failure, but permits the
connection only through that explicit option. NODE_TLS_REJECT_UNAUTHORIZED is not
an implicit bypass.

A TLSSocket subclasses the existing Socket/portable Duplex while owning a separate
private TCP transport. Only connect constructs it in this slice. Proxy transport
controls/addresses; serialize application records in 16 KiB writes and respect TCP
completion/read pressure. Cap native pending record/snapshot operations at one MiB.
Native providers never retain JS buffer pointers. GC-owned handle closures release
provider state, and explicit destruction closes it and disposes abort subscription.

Signals close pending/established transports with original AbortError cause. Shutdown
sends close_notify before TCP half-close. Unexpected TCP EOF without close_notify
is rejected; do not claim every Node truncation tolerance. Unknown reusable contexts,
server/client-cert/session/custom-verifier/cipher modes fail explicitly. HTTPS/fetch
and direct wrapping remain separate work.

Actual Node/Mite clients use independent Node/OpenSSL peers for both TLS versions,
including trusted/untrusted/self-signed/expired/hostname/wildcard/IP cases, ALPN/SNI,
CA arrays, explicit permissive verification, one MiB transfer/read pressure and
abort. Forced-GC clients use the non-GUI embedded host. An independent raw OS peer
observes ClientHello then EOF during cancelled stalled handshakes. Public test
certificates/keys are generated project fixtures, never production credentials.

References: [OpenSSL identity verification](https://docs.openssl.org/3.6/man3/SSL_set1_host/),
[operation/error sequencing](https://docs.openssl.org/3.6/man3/SSL_get_error/) and
[Windows store provider](https://docs.openssl.org/3.6/man7/OSSL_STORE-winstore/).
Native Windows/Linux acceptance and deployed default-trust policies remain hosted
or future validation, not implied by the local macOS slice.
