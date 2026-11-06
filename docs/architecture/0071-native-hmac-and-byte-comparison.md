# 0071: Native HMAC and equal-length byte comparison

Status: implemented and tested on local macOS ARM64. Hosted Windows/Linux and
new packaged release acceptance remain pending. No Ladybird changes.

## Decision

Extend the existing node:crypto module with createHmac, Hmac and timingSafeEqual.
Use the pinned OpenSSL 3 EVP_MAC HMAC provider, not deprecated HMAC_CTX APIs or a
JavaScript reimplementation. The independent Mite build already links OpenSSL;
Firefly's shared-provider experiment exports the complete pinned API. No dependency,
engine replacement or Node/V8 runtime is added. The C++ implementation owns provider
contexts with RAII; JS provides validation, Buffer encodings and Transform behavior.

HMAC consumes the exact native key byte range at construction, including empty and
long keys, and does not retain a JS key reference. Incremental updates consume raw
byte ranges immediately. Free the provider context after digest or on handle
collection/VM teardown. Reject unknown/unavailable digest providers and XOF inputs
without asserts or success placeholders. Fixed HMAC outputs are bounded to
EVP_MAX_MD_SIZE; the provider cannot return uninitialized stack data to JS.
KeyObject/CryptoKey are not yet implemented. No secure-heap, FIPS, full zeroization
or forced-provider-failure acceptance is inferred from correct digest tests.

Use the existing byte-view adapter for ArrayBuffer/view branding, raw offsets and
shared snapshots. Type/encoding validation happens in JS, not through user-defined
coercion in native byte comparison. CRYPTO_memcmp handles exactly equal-length
native byte ranges; unequal lengths throw the Node-compatible RangeError. The
caller, allocation and type/length checks are outside the equal-length comparison's
content-independent property. Concurrent shared mutation is not an atomic snapshot
of both operands. No page or renderer realm receives the module.

## Stream and compatibility policy

Hmac inherits the pinned Transform, but initializes its stream state lazily like
Node's crypto streams. options.encoding can decode string keys and select readable
stream output. Eager initialization wrongly rejects unknown readable encodings
before a manual digest; wrapping/stripping options would lose actual streaming
behavior. Retain the original options until the first stream state access instead.
Ordinary constructor/update/digest and explicit readable/pipeline workflows are
compared against the pinned Node 24.15.0 reference.

Repeated manual HMAC digest yields empty Buffer/string according to the requested
encoding, unlike Hash's repeated-digest error. Streaming also marks Mite's public
state finalized. A manual digest after streaming produced unstable native bytes in
the Node reference; never use those bytes as an expected result or mirror that
behavior. Return deterministic empty output and reject subsequent updates. Invalid
object-mode stream chunks remain ordinary JS errors. Hash/Hmac cross-receiver
internal tricks, all stream/error ordering and property descriptors are not promised.

## Evidence

The dedicated mite-crypto-hmac CTest compares 357 Node observations, including
algorithms/aliases, key and message encodings, sparse/offset/detached/resized byte
views, empty/repeated digest, invalid inputs, validation/getter recovery, namespace
identity, native state allocation pressure, lazy stream validation and encoded
Transform/pipeline output. Independent Python stdlib HMAC vectors plus an RFC 4231
known answer check 127 cases, including both SHA256/SHA512 block-size boundaries
and 65537-byte messages updated in 13-byte chunks. They do not rely solely on a
same-source adapter mock. Byte equality tests vary mismatch position and length;
they verify semantics, not an empirical timing/security certification.

The modern CLI example signs and verifies a supplied message, with invalid-signature
exit status. The existing Firefly ESM migration fixture exercises the new native
exports inside the actual Mite main process against Electron's Node reference.
Full runtime regression and native host results are recorded in PROGRESS.

References: [Node 24.15 crypto](https://nodejs.org/download/release/v24.15.0/docs/api/crypto.html),
[OpenSSL EVP_MAC](https://docs.openssl.org/3.0/man3/EVP_MAC/),
[CRYPTO_memcmp](https://docs.openssl.org/3.0/man3/CRYPTO_memcmp/),
[RFC 4231](https://www.rfc-editor.org/rfc/rfc4231).
