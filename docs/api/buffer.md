# node:buffer

Status: partial. Mite exposes the same `Buffer` constructor globally and through
`node:buffer`/`buffer`, with shared ESM/CommonJS exports. Instances use LibJS
Uint8Array/ArrayBuffer storage; Mite does not load Node or V8. The module also
exports `SlowBuffer`, [Blob and File](blob-formdata.md), and `isUtf8` below.
[Crypto](crypto.md#randomfillsyncbuffer-offset-size) documents native random filling
of existing byte storage.

The complete Buffer differential runner passes 3558 comparisons locally on macOS
against official Node 24.22.3. That covers the operations below, selected coercions,
encoding boundaries and errors; it does not establish every Node overload or
extreme allocation. Later runtime corrections still need fresh hosted Windows
acceptance. Earlier platform runs are recorded in the
[compatibility matrix](../compatibility/mite-0.1.md), separately from this checkpoint.

## Construction and allocation

```js
import { Buffer } from 'node:buffer';

const text = Buffer.from('Grüße', 'utf8');
const storage = Buffer.alloc(8);
storage.writeUInt32BE(42, 0);
console.log(storage.readUInt32BE(0)); // 42
```

| API | Result and ownership |
| --- | --- |
| `Buffer.from(string[, encoding])` | New encoded byte storage; defaults to UTF-8 |
| `Buffer.from(bufferOrTypedArray)` | Independent bytes; non-byte TypedArray elements are converted to byte values, not copied as their raw representation |
| `Buffer.from(arrayOrArrayLike)` | New storage with elements converted to bytes; supports `{ type: 'Buffer', data: [...] }` JSON records |
| `Buffer.from(arrayBuffer[, byteOffset[, length]])` | Shared view of the selected backing-store bytes; offset/length are byte units |
| `Buffer.copyBytesFrom(typedArray[, offset[, length]])` | Independent raw bytes; offset/length count source elements, not bytes; DataView is rejected |
| `Buffer.alloc(size[, fill[, encoding]])` | New zeroed storage, optionally filled |
| `Buffer.allocUnsafe(size)` / `Buffer.allocUnsafeSlow(size)` | New **zeroed** storage, without Node's pool or uninitialized-memory behavior |
| `Buffer(value[, encodingOrOffset[, length]])` | Callable/new-compatible legacy construction; a number selects allocation, other inputs select `from` |
| `SlowBuffer(length)` | Legacy zeroed allocation; no separate unsafe pool |

Allocation sizes are limited to `0x7fffffff` bytes (2 GiB minus one). The limit is
an API validation bound, not a promise that a host can allocate that amount.
Sizes must be finite nonnegative numbers; accepted fractional sizes are truncated.
Prefer `from` and `alloc` in new code. Legacy constructor deprecation warnings are
not emitted.

## Views, copying and comparison

`buffer.subarray(start?, end?)` and `buffer.slice(start?, end?)` return views,
not independent copies. Changes are visible through every view of the same bytes.
`buffer.buffer`, `byteOffset` and `byteLength` describe the native Uint8Array view;
legacy `parent` and `offset` expose the backing buffer and byte offset. An
ArrayBuffer may contain bytes outside the selected view.

```js
const original = Buffer.from([1, 2, 3]);
const view = original.subarray(1);
const copy = Buffer.from(view);
view[0] = 9;
console.log(original[1], copy[0]); // 9 2
```

| API | Behavior |
| --- | --- |
| `Buffer.isBuffer(value)` / `Buffer.isEncoding(value)` | Test the Buffer brand or a supported encoding name |
| `Buffer.byteLength(value[, encoding])` | Encoded string length, or the byte length of a view/backing buffer |
| `Buffer.concat(list[, totalLength])` | Copy Buffer/Uint8Array entries into new storage; explicit length truncates or leaves a zeroed tail |
| `Buffer.compare(a, b)` | Lexicographic byte comparison, returning -1, 0 or 1 |
| `buffer.equals(other)` | Byte equality |
| `buffer.compare(other[, targetStart[, targetEnd[, sourceStart[, sourceEnd]]]])` | Compare selected byte ranges |
| `buffer.copy(target[, targetStart[, sourceStart[, sourceEnd]]])` | Copy up to the available destination space; overlapping copies are supported; return bytes copied |
| `buffer.fill(value[, offset[, end]][, encoding])` | Repeat numeric, string or byte-view fill data; return the original Buffer |
| `buffer.toJSON()` | Return `{ type: 'Buffer', data: [...] }` |

Native consumers have their own byte-source checks. A Buffer-compatible JavaScript
view does not imply that every crypto, IPC or stream operation supports shared,
resizable or detached backing storage. Consult each consumer's contract.

## Text and search

`buffer.toString(encoding?, start?, end?)` decodes a byte range; `buffer.write(string,
offset?, length?, encoding?)` writes into existing storage and returns bytes written.
UTF-8 writes do not leave a partial encoded character, and UTF-16LE writes retain
whole code units. Malformed UTF-8 decoding uses replacement characters; use
`isUtf8` below when malformed input must be rejected instead.

Supported encodings are `utf8`/`utf-8`, `utf16le`/`utf-16le`/`ucs2`/`ucs-2`,
`latin1`/`binary`, `ascii`, `hex`, `base64` and `base64url`, case-insensitively.
UTF-8 string encoding replaces lone UTF-16 surrogates. Hex decoding stops at the
first invalid pair and ignores an incomplete final nibble. Base64 decoding accepts
the tested whitespace/URL-safe spellings; base64url output omits padding.
`Buffer.byteLength` is a length calculation, not a strict encoded-text validator.
Unknown encoding handling depends on the operation, as in the differential tests;
strict codec operations report `ERR_UNKNOWN_ENCODING`.

`indexOf(value, byteOffset?, encoding?)`, `lastIndexOf(...)` and `includes(...)`
accept numeric byte values, strings and byte views. They support negative offsets,
empty needles and UTF-16LE searches in the tested overloads. `indexOf` and
`lastIndexOf` return -1 when absent; `includes` returns a boolean.

## Numeric fields and byte order

The following instance methods read or write at an optional byte offset (default
zero for fixed-width methods). A write returns the offset immediately after its
field. Reads/writes validate the selected range before accessing storage.

| Family | Implemented methods |
| --- | --- |
| Fixed signed/unsigned integers | `readInt8`, `readUInt8`, `readInt16LE/BE`, `readUInt16LE/BE`, `readInt32LE/BE`, `readUInt32LE/BE`, and corresponding `write` methods |
| Variable-width integers | `readIntLE/BE(offset, byteLength)`, `readUIntLE/BE(...)` and corresponding `write` methods; byteLength 1–6 |
| Floating point | `readFloatLE/BE`, `readDoubleLE/BE` and corresponding `write` methods |
| BigInt | `readBigInt64LE/BE`, `readBigUInt64LE/BE` and corresponding `write` methods |
| In-place byte order | `swap16()`, `swap32()`, `swap64()`; length must be a multiple of the field size |

The `Uint` spellings alias their `UInt` counterparts. Integer value/offset/width
failures report the tested `ERR_OUT_OF_RANGE`, `ERR_BUFFER_OUT_OF_BOUNDS` or
`ERR_INVALID_ARG_TYPE` contracts; floating-point values include NaN and infinities.
BigInt writes require BigInt values. An invalid swap length throws RangeError
with `ERR_INVALID_BUFFER_SIZE`. Byte order is explicit and does not depend on
the machine's native integer layout.

## Coverage and remaining scope

The 3558-case runner compares encodings, byte ownership, copying/search/fills,
numeric boundaries and structured error results with Node. Separate Fetch codec
fixtures cover `utf8Slice`, `latin1Slice`, `ucs2Slice` and `base64Slice`, including
current empty-range validation order. The native UTF-8 validator has its own
brand/storage checks below.

```sh
python3 tooling/tests/test_buffer.py --mite build/upstream-review/bin/mite \
  --node build/node-reference-24.22.3/node-v24.22.3-darwin-arm64/bin/node
```

This module does not supply the full Node buffer namespace: `transcode`,
`resolveObjectURL`, the module-level `atob`/`btoa` exports and Node's allocation-pool
controls are absent. Buffer compatibility is not a zero-copy transfer or native
addon ABI guarantee. Shared Blob/File exports use their separate documented
contracts.

## isUtf8(input)

Return a boolean indicating whether every byte in an ArrayBuffer or TypedArray
(including Buffer) forms valid UTF-8. TypedArrays are inspected as raw bytes;
their numeric element values are not converted to text. A view's native byte
offset and byte length delimit the input. Empty input and ASCII, including NUL,
are valid.

```js
import { Buffer, isUtf8 } from 'node:buffer';

isUtf8(Buffer.from('Grüße', 'utf8')); // true
isUtf8(new Uint8Array([0xc0, 0x80])); // false: overlong encoding

const bytes = new Uint8Array([0xff, 0xc2, 0xa2, 0xff]);
isUtf8(bytes.subarray(1, 3)); // true: only the cent-sign bytes
isUtf8(bytes.buffer); // false: includes the surrounding invalid bytes
```

The validator uses Ladybird's strict native UTF-8 validation with lonely
surrogates disabled. It rejects overlong encodings, isolated continuation bytes,
UTF-8 encodings of UTF-16 surrogates, values above U+10FFFF and truncated
sequences. A BOM is valid UTF-8. Validation does not decode with replacement
characters or modify the input.

| Input failure | Error contract |
| --- | --- |
| DataView, Proxy, primitive or forged byte source | TypeError, `ERR_INVALID_ARG_TYPE` |
| Detached, shared or resizable backing storage | Error, `ERR_MITE_UNSUPPORTED_API` |

Intrinsic native brand/range checks avoid author getters and Proxy traps. Own
properties named `buffer`, `byteOffset`, `byteLength` or `length` cannot redirect
the byte range. Node's argument-error formatting may inspect a rejected Proxy;
Mite's trap-free rejection is a documented validation difference. Shared and
resizable storage support is explicitly deferred rather than read concurrently.

The focused Node/Mite differential passes locally on macOS for valid/invalid
boundary sequences, raw multi-byte TypedArray storage, exact subview ranges,
argument errors and native brands. It also checks Mite-specific storage limits
when the corresponding engine constructors are available. Fresh native
Linux/Windows acceptance for `isUtf8` remains pending.

```sh
python3 tooling/tests/test_crypto_byte_primitives.py --mite build/upstream-review/bin/mite
```

## Codec slices

Buffer instances expose utf8Slice, latin1Slice, ucs2Slice and base64Slice with
(start?, end?) byte bounds. They return decoded strings without modifying the
buffer. Empty buffers return an empty string. Negative indices throw RangeError
with ERR_OUT_OF_RANGE. Following current Node24.22.3, end <= start returns an
empty string before upper-bound validation; this includes an infinite or oversized
start. A nonempty selected range with end beyond the buffer still throws.
Exact Node24.15.0's older Infinity rejection is a recorded reference exception.
The focused codec comparisons and full3558-case Buffer runner pass locally;
fresh Windows acceptance remains pending.
