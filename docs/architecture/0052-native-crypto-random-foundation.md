# 0052: Add OS-backed randomBytes and randomUUID to Mite

Status: partial node:crypto foundation tested on macOS ARM64. Other crypto APIs,
Web Crypto, native Windows/Linux, provider failures and a full security audit remain open.

## Decision

Use the existing pinned libuv uv_random CSPRNG API, not a JS PRNG or custom entropy
algorithm. Add an owned C++/LibJS host with a small Node-facing JS module. No Node/V8,
LibWeb, new cryptographic library or Ladybird source change. Publish only implemented
randomBytes/randomUUID in the CJS builtin and ESM named-export graph.

randomBytes validates number/range before callback type, supports fractional size
truncation and a 2 GiB-minus-one API limit. Sync output allocates a rooted private VM
Uint8Array and fills its fixed backing storage directly through uv_random. A Buffer
view shares those owned bytes, avoiding an extra JS payload copy. Zero-length async
calls invoke their callback synchronously, as observed in Node.

Nonempty async calls reuse EventLoop::queue_work: a worker owns only native bytes
and calls the OS RNG; a rooted GC completion invokes JS on the owner thread. The
existing loop handles referenced work, checkpoints, fatal errors and shutdown without
calling application JS during teardown. Copy bytes into a rooted VM array, move
native storage out of the GC closure and release it before user code; allocation
failure also releases local native storage. Buffers cannot be exposed/detached while
a worker writes them. RNG errors propagate without publishing partial output or
falling back to Math.random. UV/provider and queue-admission error shapes are not
claimed equivalent to OpenSSL errors and were not forced on this host.

randomUUID validates its optional object and boolean disableEntropyCache option.
Each call obtains 16 fresh OS bytes, applies UUIDv4 version/variant bits and serializes
lowercase hex natively. It keeps no entropy cache for either option value. This is
a documented caching/performance difference. Nonempty async randomBytes callbacks
also receive undefined as their strict receiver; Node exposes an internal
RandomBytesJob receiver. Public err/data signatures and async return values match.

## Evidence

The added mite-crypto-random suite compares 76 validation/shape observations with
Node 24.15.0: invalid types/ranges/callbacks and ordering, fractions/negative zero,
UUID options/inheritance/getters and supported CJS/ESM identities/function metadata.
256 UUIDs pass format/version/variant/uniqueness smoke checks, independent Buffers
survive mutation, and a poisoned Math.random is unused. These are API/provider smoke
checks, not a statistical proof of cryptographic quality.

Zero-size synchronous callback, nonempty async ordering after the current turn and
Promise checkpoint, callback arity, 64 KiB result shape, 16 queued requests with JS
allocation stress and fatal callback exceptions pass. The allocation stress does
not assert a forced GC. Separate Mite controls verify absent hash/fill/Web Crypto
APIs fail on calls or named import; no placeholder crypto algorithms are advertised.
The provider failure and extreme allocation paths remain untested.

All 34 standalone CTest suites pass in build/crypto-random-ctest.log; detailed
contract output is in build/crypto-random-differential.log. Firefly's main ESM/HTTP
fixture awaits random bytes and creates a UUID before opening its isolated renderer;
its existing full Electron-reference gate passes in
build/crypto-random-firefly-test.log. The standalone random CLI example runs in both
Mite and Node. Runtime library linkage stays free of Node/V8/LibWeb/Qt.

## Limits

Hash/HMAC/cipher/signature/KDF/key generation, randomFill/randomInt, timingSafeEqual,
Web Crypto/global crypto and other crypto APIs remain absent. Missing methods/imports
throw JS API/link errors; they do not return success stubs. Native Windows/Linux,
entropy/provider/queue failures, extreme allocations, complete error/receiver/metadata
and monkey-patched-intrinsics parity remain unverified. No secure-heap zeroization,
FIPS status, certification or full security audit is claimed. No new npm/package,
platform/visible-desktop or public preload/IPC support. Existing warnings remain;
Ladybird's pin/source are unchanged and the overall project goal is incomplete.

Reference: official Node v24.15.0 [random API implementation](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/crypto/random.js)
and [libuv OS CSPRNG contract](https://docs.libuv.org/en/v1.x/misc.html#c.uv_random).
The Node implementation is not embedded.
