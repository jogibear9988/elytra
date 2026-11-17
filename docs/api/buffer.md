# node:buffer

Mite exposes `Buffer`, `SlowBuffer`, `Blob`, `File` and `isUtf8` through the shared
`node:buffer`/`buffer` module. This page covers the strict native UTF-8 validator;
[crypto](crypto.md#randomfillsyncbuffer-offset-size) documents native random filling
of existing byte storage.

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
