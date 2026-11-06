# node:crypto

Availability: Mite CLI/server applications and the Firefly main process. Partial
implementation tested on macOS ARM64 and in the scoped
[Linux Mite baseline](../development/linux-build.md). Windows and subsequent
compiler/SDK-profile acceptance remain pending.
Ladybird renderers do not receive this module or a privileged global crypto object.

```js
import {randomBytes, randomUUID, createHash, Hash} from 'node:crypto';
```

CommonJS require('crypto') and require('node:crypto') return the same module.
ESM named exports refer to the same functions, and its default export is that
module. These four exports are the current public surface. The
[Node 24.15 crypto reference](https://nodejs.org/download/release/v24.15.0/docs/api/crypto.html)
is the compatibility baseline; it also contains many APIs Mite does not implement.

## randomBytes(size[, callback])

| Parameter | Accepted value / default |
| --- | --- |
| size | Number from 0 through 2,147,483,647; fractional values truncate toward zero |
| callback | Optional function; undefined selects synchronous operation |

Without a callback, return a new Buffer of the requested length. With a callback,
return undefined and call callback(null, buffer) on success. Each result owns
independent storage. Entropy comes from the native OS through libuv, not Math.random.

A size that truncates to zero calls the callback synchronously. Nonempty work
runs on a native worker and completes on the runtime owner thread after the
current Promise checkpoint. Pending work keeps the runtime alive. For nonempty
callbacks the strict receiver is undefined; this differs from Node's internal
job receiver. Callback exceptions follow the runtime's uncaught-exception path.

| Failure | Error contract |
| --- | --- |
| Non-number size or non-function callback | TypeError, ERR_INVALID_ARG_TYPE |
| NaN, negative, infinite or excessive size | RangeError, ERR_OUT_OF_RANGE |
| Native entropy failure | Throw synchronously or callback(error); native code/errno and syscall uv_random |

Maximum-size allocations and forced OS/provider failure are not acceptance-tested.
The accepted numeric bound is not a guarantee that an allocation will succeed.

## randomUUID([options])

Return a lowercase UUIDv4 string generated from fresh OS entropy. options may be
undefined or an object other than null/Array. Its optional disableEntropyCache
property must be boolean. Invalid options/property types throw TypeError with
ERR_INVALID_ARG_TYPE. Getter exceptions propagate unchanged.

Mite uses fresh entropy for either value of disableEntropyCache: it has no UUID
entropy cache. This is a documented policy difference. Native entropy failures
throw with the libuv error metadata described above.

## createHash(algorithm[, options])

Return a Hash that extends the implemented Transform stream. Hash(algorithm,
options) and new Hash(algorithm, options) are also available. algorithm must be a
string; other types throw TypeError with ERR_INVALID_ARG_TYPE.

| Option | Contract |
| --- | --- |
| outputLength | Optional nonnegative integer byte count, through 0xffffffff; undefined uses the provider default |
| Transform options | Passed to the implemented Transform constructor; complete Node stream option parity is not certified |

SHAKE supports variable outputLength. A fixed-length algorithm accepts its natural
digest length; a different value rejects with ERR_OSSL_EVP_NOT_XOF_OR_INVALID_LENGTH.
Non-number lengths throw ERR_INVALID_ARG_TYPE; invalid integer/range values throw
ERR_OUT_OF_RANGE. Values above 0x7fffffff fail with ERR_MITE_UNSUPPORTED_API because
of Mite's Buffer limit. Options getters can throw; those exceptions are preserved.

The native backend is the pinned OpenSSL EVP implementation. Locally tested
families include MD5, SHA1, SHA2, SHA3, BLAKE2 and SHAKE. This describes coverage,
not algorithm recommendations or a complete provider inventory. Unknown algorithms
throw; unavailable providers can report ERR_OSSL_EVP_UNSUPPORTED. Native context
creation/copy failures report ERR_CRYPTO_HASH_CREATION_FAILED. Exact error messages,
full provider configuration and FIPS behavior are not compatibility guarantees.

## Hash methods and lifetime

| Signature | Return / behavior |
| --- | --- |
| hash.update(data[, inputEncoding]) | Return this; append string, Buffer, typed-array or DataView bytes |
| hash.digest([outputEncoding]) | Finalize; Buffer by default, encoded string for a recognized output encoding |
| hash.copy([options]) | Independent Hash snapshot of the current state; outputLength follows the rules above |

String input defaults to UTF-8. Views hash their selected raw bytes, not numeric
element values; use a view for an ArrayBuffer. Unsupported input types throw
TypeError with ERR_INVALID_ARG_TYPE. Odd-length hex strings throw
ERR_INVALID_ARG_VALUE. An invalid native update reports ERR_CRYPTO_HASH_UPDATE_FAILED.

Manual digest marks the public hash finalized. Subsequent update, digest or copy
throws ERR_CRYPTO_HASH_FINALIZED. A Symbol output-encoding coercion throws before
finalizing, so a later valid digest can still succeed. Unrecognized output labels
return a Buffer in the measured behavior; use an explicit supported encoding for
application code. Method calls with an invalid receiver throw TypeError.

The stream path accepts write/end and the implemented pipeline workflows. It emits
one digest chunk; setEncoding('hex') converts readable output to text. A manual
digest after stream completion can read the cached digest once. Native EVP contexts
are freed on completion or collection; there is no public disposal method.
Do not mix stream and manual mutation without checking the documented/tested path.
Complete stream/error parity, forced-GC behavior and secure-heap zeroization are
not certified.

## Run an example

The [random example](../../examples/mite-cli/random.mjs) demonstrates native random
bytes and UUIDs. The [hash example](../../examples/mite-cli/hash.mjs) demonstrates
SHA-256 and independent hash copies, with an optional input file:

```sh
build/release-mite/bin/mite examples/mite-cli/random.mjs
build/release-mite/bin/mite examples/mite-cli/hash.mjs
```

For supported behavior, the same example runs under Node. UUID values naturally
differ between executions. See [modules](../concepts/modules-and-packages.md) and
[resource lifetime](../concepts/async-and-lifetime.md).

## Unsupported APIs and verification

HMAC, randomInt/randomFill, ciphers, keys/signatures, KDFs, getHashes, timingSafeEqual
and Web Crypto are absent. Calling an absent property fails as an ordinary JS
TypeError; importing an absent named export fails during module loading. There is
no fallback to Node/V8. Do not treat incremental hashing as encryption or password
storage support.

The mite-crypto-random and mite-crypto-hash CTest suites compare supported contracts
against Node, including validation, byte views, copy independence, callback ordering,
Transform/pipeline, CJS/ESM identity and error paths. The Firefly main-process gate
also imports these APIs. Mite safely handles negative-zero SHAKE lengths and invalid
object-mode chunks where the pinned Node reference can hit native assertions.
These are intentional differences, not omitted tests. See the
[compatibility matrix](../compatibility/mite-0.1.md),
[random design](../architecture/0052-native-crypto-random-foundation.md) and
[hash design](../architecture/0053-native-hash.md).
