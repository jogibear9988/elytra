# ADR 0014: encoding globals with native streaming state

Status: implemented subset, tested on macOS ARM64 against Node v24.15.0.

Expose TextEncoder/TextDecoder globally and through node:util, using the existing
builtin registry for shared CJS/ESM/global identity. Other util APIs are still
unimplemented. Keep branded state in private JS WeakSet/WeakMap records; native
decoder functions retain GC-owned C++ state and release native converters when
that state is destroyed. No workers enter this API or access its state.

Reuse LibTextCodec's standardized encoding labels and StreamingDecoder for UTF-8,
UTF-16 and most legacy encodings. Label lookup calls its existing FFI directly
because the C++ convenience wrapper writes diagnostics for unknown labels. API
validation must report a caught RangeError without unsolicited stderr output.
Replacement encoding labels are rejected as Node does.

## Node's legacy conversion behavior

The executable reference differs from WHATWG/encoding_rs mappings for Big5,
EUC-JP, Shift_JIS and EUC-KR and from its error timing for ISO-2022-JP. Examples
include C1 control bytes and Big5 private-use mappings. Use the already-pinned ICU
UConverter for these five encodings, leaving the choice in our adapter rather
than patching Ladybird. Configure fatal conversion to stop; replacement conversion
uses an explicit U+FFFD callback so ICU's converter-specific substitution byte
cannot become a U+001A replacement. Preserve state for stream=true and reset after
final flush, including fatal errors. Other encodings continue through LibTextCodec.
This is a Node compatibility choice for Mite, not a modification of the browser's
WHATWG decoder.

## Buffer boundaries and UTF-8 output

Extract ArrayBuffer, SharedArrayBuffer, TypedArray and DataView ranges through
engine slots/witness records. User-overridden buffer/byteOffset/byteLength
properties cannot redirect access. Detached/out-of-bounds inputs behave as empty
ranges where observed in Node. Bytes are borrowed only during synchronous native
conversion; resulting strings/arrays have engine-owned storage.

TextEncoder.encode converts scalar text to UTF-8 in native code. encodeInto accepts
a Uint8Array (including Buffer/shared/resizable views) and writes only complete
Unicode scalars into the validated view range. Its read count measures UTF-16
code units and written count measures bytes; unpaired surrogates become U+FFFD.
No JS allocation/callback occurs while the destination byte pointer is borrowed.

## Validation and remaining scope

The 1,657 differential comparisons cover labels/aliases and unsupported labels,
all advertised flags, legacy mappings, malformed bytes, chunk splits, BOM handling,
fatal-error recovery, multi-buffer native output, bounded encoding, surrogate
replacement, argument/coercion order, brands and shared builtin identity. Byte
source tests include typed/DataView offsets, shadowed JS properties, shared,
detached and resizable/out-of-bounds buffers. Values and error names/codes are
compared; full diagnostic wording/stacks and all Web Platform Tests are not.
Forced-GC native decoder cleanup has not been stress-tested.

TextEncoderStream/TextDecoderStream and the remaining util surface are absent.
Native Windows/Linux execution remains unverified. All adapters are outside the
unmodified Ladybird submodule; upstream updates must rerun these compatibility
checks, including both the LibTextCodec and ICU paths.

References: [WHATWG Encoding](https://encoding.spec.whatwg.org/),
[Node util encoders/decoders](https://nodejs.org/docs/latest-v24.x/api/util.html).
