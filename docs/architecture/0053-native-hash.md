# 0053: Checked native Hash/createHash with Transform integration

Status: partial node:crypto hashing, tested on macOS ARM64. HMAC, ciphers, signatures,
KDFs, keys, Web Crypto and native Windows/Linux validation remain outstanding.

## Decision

Add createHash and Hash, with incremental update, digest, independent copy and the
existing pinned readable-stream Transform implementation. Use an owned C++ adapter
to the existing pinned OpenSSL EVP library (locally 3.6.5). LibCrypto already uses
this backend, but its OpenSSLHashFunction asserts on provider failures. Calling EVP
with checked return values allows JS errors for unknown/unavailable algorithms
without changing Ladybird. No new backend, Node/V8, LibWeb dependency or vendor seam.
Explicit OpenSSL::Crypto linkage belongs in the common Mite runtime graph, including
Firefly main; dependency pins and license notices remain unchanged.

Each hash owns a native EVP_MD_CTX through shared RAII state captured by private
LibJS functions; no JS references enter the provider. The descriptor returned by
EVP_get_digestbyname is borrowed static storage. Finalization releases the context;
copy uses EVP_MD_CTX_copy_ex, preserving independent incremental state. The public
JS WeakMap tracks receiver identity and finalization. Private digest caching supports
Node's stream-finalize then manual-digest behavior. Output is copied into rooted VM
storage and exposed as Buffer; cached native bytes live until the handle is collected.
This is neither a secure heap nor a zeroization guarantee. Constructor failures and
abandoned copies release their provider context through RAII.

String encoding uses existing Buffer codecs; views hash raw bytes with native byte
bounds, including DataView and typed-array offsets. Raw ArrayBuffer input rejects.
Fixed digests reject a differing outputLength; SHAKE supports requested lengths and
copy overrides. Public validation follows uint32 rules, with an explicit Mite Buffer
limit of 2 GiB-minus-one (larger lengths throw ERR_MITE_UNSUPPORTED_API). Huge
allocations/failures are unforced. No fallback algorithm is substituted when provider
initialization fails. EVP failures receive JS errors; complete OpenSSL error-stack,
provider configuration, FIPS and Node message/metadata parity are not implemented.
Only Hash/createHash and the earlier random methods are exported; getHashes and
other absent APIs are not advertised.

## Evidence

mite-crypto-hash compares 162 observations with Node 24.15.0: MD5, SHA1, SHA2,
SHA3, BLAKE2 and SHAKE known/empty/Unicode/large inputs; algorithm aliases and NUL
prefix behavior; Buffer/typed-array/DataView offsets and resizable-buffer bounds;
input/output codecs and fallback/coercion; invalid types/lengths, unknown algorithms
and disabled MD4; finalized receivers, copy branching and SHAKE length overrides.
Additional controls cover thrown getter identity, Symbol output recovery, ESM/CJS
identity, callable Hash and Transform inheritance, write/end and pipeline/setEncoding,
fatal stream callback propagation and 128 independent copies under allocation stress. Stress does not force GC.
SHA256('abc') is checked against its fixed known answer as well as Node.

Two isolated reference children record Node 24.15 native assertion aborts: SHAKE
outputLength -0 (IsUint32) and invalid object-mode stream input (IsSharedArrayBuffer).
Mite safely accepts negative zero as zero and reports invalid stream input through a
TypeError/ERR_INVALID_ARG_TYPE error event. Tests accept a future fixed Node result
as well as those specific reference assertions. Node deprecation warnings are disabled
for the explicitly tested legacy callable Hash constructor.

All 35 standalone CTest suites pass in build/crypto-hash-ctest.log; differential
output is in build/crypto-hash-differential.log. The existing Firefly/Electron gate
passes in build/crypto-hash-firefly-test.log, including named ESM createHash in its
main process before renderer loading. examples/mite-cli/hash.mjs runs with both
Mite and Node for default input and an actual source file. Standalone linkage remains
platform-only; upstream integrity passes with the unchanged gitlink/manifest.

## Limits

Provider inventory/configuration, uncommon algorithms and parameterized digests,
full stream/backpressure/metadata/error parity, hostile monkey-patched intrinsics,
forced GC/allocator/provider failures, extreme lengths, full security audit and native
Windows/Linux remain unverified. Public preload/contextBridge/IPC, cross-platform
packaging and the wider specification remain incomplete. No newer Ladybird candidate
was evaluated. Existing linker/SDK warnings and locked-desktop validation limits remain.

References: [Node Hash API](https://nodejs.org/docs/latest-v24.x/api/crypto.html#class-hash)
and [OpenSSL EVP digest lifecycle](https://docs.openssl.org/3.6/man3/EVP_DigestInit/).
These libraries' existing notices are preserved; Node is the behavioral reference,
not an embedded dependency.
