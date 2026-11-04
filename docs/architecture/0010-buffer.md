# ADR 0010: Buffer on engine-owned byte storage

Status: byte and numeric API subset implemented; tested against Node v24.15.0 on macOS ARM64.

Use LibJS Uint8Array/ArrayBuffer storage and our embedded JS API layer. This gives
GC-owned bytes, typed-array interoperability, shared ArrayBuffer/subarray/slice
views and native overlapping TypedArray.set behavior without an extra C++ byte
owner or changes to Ladybird. The builtin is initialized before application code;
global Buffer and node:buffer's Buffer export share the same cached constructor.
CommonJS and ESM use the existing registry. The implementation is independent;
Node is only the behavioral test reference.

Implement allocation/from/byteLength/isEncoding/isBuffer, concat/compare,
copyBytesFrom, string conversion/write, fill/copy, equality/range comparison,
search, swaps and JSON. Numeric access covers signed/unsigned 8/16/32-bit and
variable 1–6-byte integers, Float/Double and 64-bit BigInt, both byte orders and
UInt/Uint aliases. Validate type/range/bounds before accessing byte storage;
DataView handles floating-point representation at the actual view byte offset. UTF-8 handles surrogate replacement, invalid byte sequence
consumption and complete-code-point write limits. Other supported encodings are
UTF-16LE/UCS-2, latin1/binary, ASCII, hex, base64 and base64url. Array-like inputs
use indexed elements, while ArrayBuffer input shares its backing store. All JS
heap operations remain on the owning VM thread.

The differential corpus has 3,535 comparisons plus the explicit allocation-policy check, including invalid UTF-8
lead/continuation boundaries, encoding aliases/errors, partial writes, overlapping
copies, view aliasing, array-like conversion, JSON and builtin/global identity.
Numeric tests compare encoded bytes, decoded values, return offsets, coercions,
range/short-buffer errors, negative zero, infinities, NaN, BigInt boundaries and
subview offsets.
It compares values and error class/code, not diagnostic wording or stacks.

Allocation policy deliberately differs: allocUnsafe/allocUnsafeSlow return fresh
zeroed storage and use no shared pool. Allocation APIs cap buffers at 2 GiB minus
one byte; this is a runtime policy, not a claim of Node's engine limits. Deprecated
constructor/SlowBuffer calls do not emit Node's deprecation warnings. No poolSize
or Node-specific maximum/string-length constants are advertised.

Blob/File, atob/btoa, transcode, isAscii/isUtf8 and object
URLs are not yet implemented. Native filesystem/network services must consume
rooted Buffer/ArrayBuffer views or copy bytes before handing work to other
threads; workers must not access the JS heap. Windows/Linux runtime execution
remains unverified. No general Buffer compatibility claim is made.

Reference: [Node 24 Buffer API](https://nodejs.org/docs/latest-v24.x/api/buffer.html).
