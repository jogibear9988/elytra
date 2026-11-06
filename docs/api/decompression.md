# DecompressionStream and compressed Fetch

Status: partial, tested locally on macOS against Node 24.15.0 and independent
Python zlib/HTTP fixtures. Fresh native Linux/Windows acceptance is pending.
Mite uses pinned zlib 1.3.2#2, matching the existing renderer dependency, without
LibWeb, Node or V8. Ladybird and zlib sources remain unmodified.

## Web Stream interface

`new DecompressionStream(format)` supports gzip, deflate (zlib wrapper) and
deflate-raw. It is available as a global and the same constructor from
`node:stream/web`. Invalid/unavailable formats throw TypeError. Readonly readable
and writable getters expose Web Streams; the output supports default/BYOB readers.

```js
const response = await fetch('https://example.com/data.gz');
// Use this only for a compressed file whose HTTP Content-Encoding is not already
// decoded by fetch. HTTP Content-Encoding handling below is automatic.
const decoded = response.body.pipeThrough(new DecompressionStream('gzip'));
for await (const bytes of decoded) {
  console.log(bytes.byteLength);
}
```

The writable accepts ArrayBuffer and ArrayBuffer views. String/object/null chunks
reject explicitly and error both endpoints. Input is snapshotted while being
processed. Consume both sides concurrently, normally with pipeThrough/pipeTo.
Writing and awaiting completion without reading can wait for output demand.

The native adapter consumes at most one MiB of input and returns at most 64 KiB of
owned output per step. It clears all native byte pointers before returning to the
VM. The JS layer produces one block per readable pull, even when a small compressed
input expands to a large output. Node can accept a compressed native write before
its output is read; Mite deliberately retains pressure until the input is consumed.
The differential suite records both observed behaviors.

Checksums and stream termination are validated. Bad input, missing dictionary or
truncated streams reject TypeError with the backend code/cause (for example
Z_DATA_ERROR or Z_BUF_ERROR). gzip supports concatenated members. The public Web
interface rejects trailing zero padding with ERR_TRAILING_JUNK_AFTER_STREAM_END,
matching the measured Node reference. Byte counts, exact messages and every native
provider error are not a general Node binding compatibility claim.

Readable cancellation errors the writable and rejects a pending write with the
reason. Writable abort errors the readable; explicit reasons preserve identity.
Abort owns its writable state transition so a signal notification does not turn a
successful abort into a second rejection. Completion/error/cancel release native
inflate storage; destruction also has RAII cleanup. Forced real VM collections
between steps validate live state in Node and the non-GUI Mite embedded host.

## Automatic HTTP content decoding

Fetch advertises Accept-Encoding `gzip, deflate` by default. Responses decode gzip/
x-gzip and deflate/x-deflate, including the tested raw-deflate HTTP fallback. Multiple
Content-Encoding values decode in reverse order. Concatenated gzip and trailing
HTTP gzip zero padding are supported. An explicitly empty Content-Length zero body
does not require a compressed wrapper. HEAD/null-body responses remain null.

Response headers keep the original encoded Content-Encoding and Content-Length,
as Node does; body/text/JSON/Blob consumers see decoded bytes. The encoded length
therefore need not equal the number of bytes read. Decoding remains incremental:
independent peers hold the rest of a gzip stream until the client receives decoded
bytes from its first flush. Cancel and AbortSignal propagate through decoder(s) to
the native HTTP connection.

Mite validates the full compressed trailer even when HTTP framing completes.
Node's HTTP decoder can return decoded data from a truncated gzip trailer; Mite
rejects that response body. This deliberate integrity difference is separately
asserted. Malformed/checksum failures reject the Fetch body as TypeError with the
decompression cause. Compression does not change HTTP status resolution semantics.

Brotli, zstd and other codings are unavailable and reject explicitly, rather than
returning encoded bytes as decoded content. Outbound compression, CompressionStream
and node:zlib are not implemented by this slice. Decoder work runs synchronously
in finite native steps on the VM thread; no Node worker-pool parity is promised.
Convenience consumers still collect data in memory, and a slower tee/clone branch
can accumulate output. There is no total decoded-size/memory quota.

## Validation and remaining work

Eight actual Node/Mite stream workflows cover shared constructors, fragmented
headers, one-MiB expansion, all three formats, members, checksum/truncation/padding,
abort and input validation. Separate pressure tests assert the timing difference.
Five native-only format/chunk guards reject unavailable/private inputs. Four
independent HTTP workflows cover formats/raw fallback/stacking, preserved headers,
pre-EOF delivery, CRC/empty framing and peer-observed cancel/abort. GC and a runnable
modern ESM file example are also exercised under both runtimes.

Full compression standards/WebIDL/WPT, every format/charset/flush/dictionary/error/
GC/half-close case, worker behavior, compression encoders and fresh hosted platform
acceptance remain open. See [Fetch](fetch.md) and [Web Streams](web-streams.md).
Run the [file example](../../examples/mite-compression/decompress.mjs) with
`mite examples/mite-compression/decompress.mjs file.gz gzip`.

References: [zlib manual](https://zlib.net/manual.html),
[Node 24.15 Web Streams](https://nodejs.org/download/release/v24.15.0/docs/api/webstreams.html#class-decompressionstream).
