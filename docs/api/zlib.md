# node:zlib

Status: **partial**, tested locally on macOS with Node 24.15.0, independent Python
codecs and the non-GUI native VM host. Fresh Linux/Windows execution remains open.
Mite uses the already pinned native zlib 1.3.2#2 without Node/V8 or LibWeb.

## Formats and imports

ESM imports from node:zlib or zlib and CommonJS require share one module instance.
The implemented classes are DeflateRaw, InflateRaw, Deflate, Inflate, Gzip and
Gunzip, with matching createDeflateRaw/createInflateRaw/createDeflate/createInflate/
createGzip/createGunzip factories. Construct classes with new. They inherit the
implemented Node-compatible Transform stream and support write/end, pipe, data/
error/end/close events and readable/writable pressure within that stream contract.

```js
import { createGzip, gunzipSync } from 'node:zlib';

const encoder = createGzip();
const chunks = [];
encoder.on('data', chunk => chunks.push(chunk));
encoder.on('error', console.error);
encoder.on('end', () => {
  const compressed = Buffer.concat(chunks);
  console.log(gunzipSync(compressed).toString());
});
encoder.end('Hello from Mite');
```

Matching convenience functions gzip/gunzip/deflate/inflate/deflateRaw/inflateRaw
accept input, optional options and an error-first callback. Their Sync variants
return a Buffer. Convenience input supports strings, Buffer/typed-array/DataView
bytes and ArrayBuffer. The implementation snapshots convenience inputs. Streaming
write supports the underlying Transform's string/Uint8Array/Buffer contract.

## Flush, close and options

flush([kind,] callback) queues a private empty write marker in the same writable
queue as data. The default kind is Z_FULL_FLUSH. Z_NO_FLUSH, Z_SYNC_FLUSH,
Z_FULL_FLUSH and Z_FINISH are implemented. Callbacks run after preceding data and
that flush output have been processed, subject to readable pressure. Z_SYNC_FLUSH
preserves the compression dictionary, including Code-OSS's per-message raw-deflate
trailer removal/reinsertion. close([callback]) destroys the stream and closes its
native handle; the callback is deferred. bytesWritten counts processed input bytes.
Exact trailing-byte accounting/error/event parity remains incomplete.

Supported codec options are windowBits (8–15; gzip compression requires 9–15,
raw/zlib compression maps 8 to 9), level (-1–9), memLevel (1–9), strategy (0–4),
chunkSize (64–1,048,576, default 16,384), flush (default 0), finishFlush (default 4)
and maxOutputLength (1–67,108,864, default 67,108,864). Common byte-stream options
highWaterMark, readableHighWaterMark, writableHighWaterMark, encoding, decodeStrings,
emitClose, autoDestroy and signal are passed to the existing Transform. Explicit
false for decodeStrings/emitClose is unavailable. Unknown options reject explicitly;
complete Node option coercion and stream-option parity are not claimed.

The frozen constants object contains the implemented flush/status/error/strategy/
level constants, plus identifiers for unavailable flush kinds. Z_PARTIAL_FLUSH,
Z_BLOCK and Z_TREES reject explicitly. Dictionary/info options, params and reset,
Unzip autodetection, Brotli, Zstd and crc32 remain unavailable and throw
ERR_MITE_UNSUPPORTED_API at the declared entry points. Other missing Node exports
are not advertised. Class calls without new and full Node constants/errno/provider/
threadpool behavior are not implemented.

## Bounds, errors and acceptance

Native handles own z_stream memory and retain no JS byte pointers between calls.
Each step copies at most 1 MiB input and produces at most chunkSize output. Stream
work yields through the real event loop between steps and waits when readable push
signals pressure; the readable queue can exceed its high-water mark by one output
chunk. Convenience output is capped by maxOutputLength; collection and the final
Buffer concatenation add bounded copies. This is not a throughput or zero-copy claim.
Gzip members and zero padding work; malformed/truncated/checksum/dictionary failures
reject with explicit Z_DATA_ERROR/Z_BUF_ERROR/Z_NEED_DICT as applicable. Error text,
errno, unusual trailing input and all Node provider differences remain partial.

One measured cancellation difference is intentional: destroying a pressure-blocked
Mite transform retires its pending write callback once. The same Node 24.15.0 case
retains that callback. Tests keep both expected results explicit. This is not a
claim of identical cancellation/event ordering in every stream state.

The mite-zlib CTest covers real Node/Mite sync/callback/fragmented stream roundtrips,
RFC7692-style context takeover, independent Python gzip/zlib/raw bytes in both
directions (including input above the native step size), output caps, malformed and
truncated input, native GC lifetime, read pressure, close and cancellation. The
actual pinned Code-OSS ZlibDeflateStream/ZlibInflateStream classes also pass locally
with real compiled VSBuffer/Emitter/Disposable/IPC dependencies. The real Code-OSS
ready path advances from unsupported node:zlib to unsupported node:assert; no
workbench or desktop workflow is accepted. See [ADR 0151](../architecture/0151-node-zlib-streams.md).

The independent [Web Streams/Fetch decoder](decompression.md) retains its existing
implementation and contract. Node zlib stream support does not add CompressionStream.
API reference: [Node zlib](https://nodejs.org/docs/latest-v24.x/api/zlib.html),
[native zlib manual](https://zlib.net/manual.html).
