# node:crypto

Availability: Mite CLI/server applications and the Firefly main process. Partial
implementation tested on macOS ARM64 and in the scoped
[Linux Mite baseline](../development/linux-build.md). Windows and subsequent
compiler/SDK-profile acceptance remain pending.
Ladybird renderers do not receive this module or a privileged global crypto object.

```js
import {randomBytes, randomUUID, randomInt, createHash, Hash, createHmac, Hmac, timingSafeEqual} from 'node:crypto';
```

CommonJS require('crypto') and require('node:crypto') return the same module.
ESM named exports refer to the same functions, and its default export is that
module. These eight exports are the current public surface. The
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

## randomInt([min,] max[, callback])

Return a cryptographically random safe integer in [min, max), with min defaulting
to zero. Both endpoints must be safe integers, max must exceed min, and max - min
must be less than 2**48. There is no numeric coercion. Invalid endpoints throw
TypeError / ERR_INVALID_ARG_TYPE; reversed, empty or excessive ranges throw
RangeError / ERR_OUT_OF_RANGE. A supplied callback must be a function; validation
is synchronous in either form.

Without a callback, return the integer synchronously. With a callback, return
undefined and later call callback(undefined, integer), or callback(error) on
native entropy failure. The strict callback receiver is undefined. Pending entropy
work keeps the runtime alive; callback exceptions follow the uncaught-error path.
Rejection sampling avoids modulo bias, including near the 48-bit range limit.

Mite requests six fresh OS entropy bytes for every attempt and does not cache
integers. Consequently callback timing relative to nextTick/Promise callbacks can
differ from Node's warm entropy-cache path. This is an explicit timing/performance
difference; callers must not depend on that cache. Native behavior is tested on
macOS ARM64 and in the [Linux baseline](../development/linux-build.md); Windows
acceptance remains pending.

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

## createHmac(algorithm, key[, options]) / Hmac

Return a new Hmac instance, also a Transform stream. Hmac(algorithm, key, options)
and new Hmac(algorithm, key, options) are supported. It has update/digest; there is
no Hmac.copy method. Hmac is separate from Hash.

| Parameter | Type / supported values |
| --- | --- |
| algorithm | String naming an available fixed-output OpenSSL digest; no default |
| key | String, Buffer, TypedArray, DataView, ArrayBuffer or SharedArrayBuffer |
| options.encoding | String-key encoding; omitted/null uses UTF-8; Buffer encodings supported |
| Other options | The existing Transform options, initialized when a stream operation first needs state |

The native OpenSSL 3 EVP_MAC provider holds the incremental state. Key bytes are
consumed synchronously at construction: later mutation, detachment or resizing
of the supplied key does not change the HMAC. Typed arrays/DataViews use their raw
byte range and offset, not their numeric elements. Empty keys and messages are
accepted. No key material is logged or stored in a main-process JS state field.
Temporary JS/string/shared-buffer copies are not promised to be securely erased.

Measured algorithms include MD5, SHA-1, SHA-224/256/384/512, SHA3-256/512 and
BLAKE2b512/BLAKE2s256. Aliases depend on the pinned provider; this is not a complete
provider inventory or a recommendation to use every supported digest. XOF/SHAKE
is not accepted for HMAC. KeyObject/CryptoKey and key-management APIs are absent.

| Failure | Result |
| --- | --- |
| Algorithm is not a string; unsupported key type; nonstring nonnull options.encoding | TypeError, ERR_INVALID_ARG_TYPE |
| Unknown string-key encoding | TypeError, ERR_UNKNOWN_ENCODING |
| Unknown digest name | TypeError, ERR_CRYPTO_INVALID_DIGEST |
| Unavailable native provider/digest | Error, ERR_OSSL_EVP_UNSUPPORTED |
| XOF/SHAKE digest | Error, no stable code in the reference-compatible path |
| Native context allocation failure | Error, ERR_CRYPTO_HASH_CREATION_FAILED; not fault-injection tested |

Getter exceptions from options.encoding propagate unchanged. Binary keys ignore
unknown string encoding labels for key decoding, but the same option can be
validated later as a readable stream encoding. This follows Node's lazy stream
initialization: manual digest does not eagerly initialize stream state.
An outputLength option does not change the HMAC length. Extreme-size key allocation,
provider fault injection, FIPS and secure-heap behavior remain unverified.

### hmac.update(data[, inputEncoding])

Return this for chaining. Accept strings (UTF-8 by default), Buffer, TypedArray and
DataView. ArrayBuffer input must first be wrapped in a view. Byte inputs are
consumed before return and are not retained as JS data. Input encodings and raw
byte-view behavior use the same measured subset as Hash.update above. Unsupported
data throws TypeError / ERR_INVALID_ARG_TYPE; malformed odd-length hex strings
throw ERR_INVALID_ARG_VALUE. Updating after manual digest or stream completion
throws ERR_CRYPTO_HASH_FINALIZED in Mite. A rejected input does not finalize a
previously valid HMAC; a later valid update can still succeed.

### hmac.digest([encoding])

Finalize and return a new Buffer, or encoded text for a recognized output label.
The output length is fixed by the digest algorithm. Native provider state is freed
on finalization; abandoned handles release it on collection/runtime teardown.
Invalid output-label coercion, such as a Symbol, throws before finalization.
Unknown output labels return a Buffer in the measured first-call behavior.

Unlike Hash.digest, a repeated manual HMAC digest returns empty output: a Buffer
for omitted/falsey encoding or 'buffer', otherwise an empty string. That repeated
path does not coerce the encoding again. Calls with invalid receivers throw
TypeError; cross-calling Hash methods on Hmac or Hmac methods on Hash is unsupported.

### HMAC as a stream

write/end, pipe/pipeline, readable output encoding and the tested backpressure
machinery come from the pinned Transform implementation. The stream emits its
MAC once. options.encoding decodes a string key and also controls readable output
when stream state initializes; setEncoding can explicitly select the output codec.

Use either the manual or stream lifecycle for application code. Mite deliberately
marks stream completion finalized: subsequent digest returns empty output rather
than reproducing unstable post-stream native output seen in Node 24.15. Invalid
object-mode chunks emit a JS error rather than triggering the native assertions
observed in related Node crypto paths. Full stream/receiver/error parity and
forced-GC/zeroization guarantees are not claimed.

```js
import {createHmac} from 'node:crypto';
const signature = createHmac('sha256', 'example-key')
  .update('message', 'utf8')
  .digest('hex');
console.log(signature);
```

## timingSafeEqual(left, right)

Both parameters must be Buffer, TypedArray, DataView, ArrayBuffer or
SharedArrayBuffer. Compare their exact byte ranges; typed-array values are not
converted to numbers. Return a boolean. Equal zero-length inputs return true.
Different byte lengths throw RangeError / ERR_CRYPTO_TIMING_SAFE_EQUAL_LENGTH;
unsupported types, including strings, throw TypeError / ERR_INVALID_ARG_TYPE.

Equal-length byte comparison uses OpenSSL CRYPTO_memcmp without content-dependent
early exit in our adapter. Type/length checks, argument evaluation, caller logic
and allocation are outside that property; this does not make an entire signature
verification flow constant-time. Shared buffers use the engine's native snapshots;
concurrent mutation is not an atomic two-buffer comparison contract. Detached and
out-of-bounds views follow the current empty-byte normalization of the encoding
adapter. Do not infer equal numeric arrays from equal native byte patterns across
machines with different byte orders.

## Run an example

The [random example](../../examples/mite-cli/random.mjs) demonstrates native random
bytes, UUIDs and bounded integers. The [hash example](../../examples/mite-cli/hash.mjs) demonstrates
SHA-256 and independent hash copies, with an optional input file:

```sh
build/release-mite/bin/mite examples/mite-cli/random.mjs
build/release-mite/bin/mite examples/mite-cli/hash.mjs
MITE_HMAC_KEY=example-key build/release-mite/bin/mite examples/mite-cli/hmac.mjs message
```

The [HMAC example](../../examples/mite-cli/hmac.mjs) signs a message using an
environment-supplied key. Pass an optional 64-character hex signature after the
message to verify it: prints VALID/INVALID and exits 1 for invalid input. Its format
checks concern public signature data; the fixed-size MAC bytes use timingSafeEqual.
The example key above is demonstration data.

For supported behavior, the same example runs under Node. UUID values naturally
differ between executions. See [modules](../concepts/modules-and-packages.md) and
[resource lifetime](../concepts/async-and-lifetime.md).

## Unsupported APIs and verification

randomFill, ciphers, KeyObject/CryptoKey, asymmetric signatures, KDFs, getHashes
and Web Crypto are absent. Calling an absent property fails as an ordinary JS
TypeError; importing an absent named export fails during module loading. There is
no fallback to Node/V8. Do not treat incremental hashing as encryption or password
storage support.

The mite-crypto-random, mite-crypto-random-int, mite-crypto-hash and mite-crypto-hmac CTest suites compare supported contracts
against Node, including validation, byte views, copy independence, callback ordering,
Transform/pipeline, CJS/ESM identity and error paths. The Firefly main-process gate
also imports these APIs. Mite safely handles negative-zero SHAKE lengths and invalid
object-mode chunks where the pinned Node reference can hit native assertions.
These are intentional differences, not omitted tests. See the
[compatibility matrix](../compatibility/mite-0.1.md),
[random design](../architecture/0052-native-crypto-random-foundation.md) and
[hash design](../architecture/0053-native-hash.md).

HMAC verification also uses independent Python stdlib vectors and an RFC 4231
known answer, including digest block-size boundaries, long/empty keys and messages.
See [the HMAC/equality design](../architecture/0071-native-hmac-and-byte-comparison.md).
HMAC/equality exports are included in [macOS ARM64 Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2)
at `19ad113`, whose native macOS/Linux/Windows Release CLI suites all pass.
Alpha 1 predates these exports; later changes need their own platform acceptance.
