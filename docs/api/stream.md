# Node streams and Web Stream bridges

Status: partial. The runtime uses pinned readable-stream 4.7.0 with owned ESM
adapters to the realm's [Web Stream classes](web-streams.md). Conversion is tested
locally on macOS against Node 24.15.0. Baseline dc3308b also passes all 55
hosted suites on Linux x64 and Windows x64, including these bridges. All of this runs in standalone Mite and Firefly's main process without
Node, V8 or LibWeb.

The existing Node stream subset includes Readable, Writable, Duplex, Transform,
PassThrough, callback/Promise pipeline and finished, byte/object mode, pressure,
cork/writev, async iteration and selected state helpers. It is not the complete
Node stream implementation. See the [compatibility matrix](../compatibility/mite-0.1.md).

## Conversion methods

Import Readable, Writable or Duplex from `node:stream`. Each conversion creates a
new adapter around the supplied endpoint without buffering its entire contents.

| Method | Input and result | Documented options |
| --- | --- | --- |
| `Readable.fromWeb(readable, options?)` | Unlocked Web ReadableStream → Node Readable | `objectMode`, `highWaterMark`, `encoding`, `signal` |
| `Readable.toWeb(readable, options?)` | Node readable → Web ReadableStream | `strategy: { highWaterMark, size }` |
| `Writable.fromWeb(writable, options?)` | Unlocked Web WritableStream → Node Writable | `objectMode`, `highWaterMark`, `decodeStrings`, `signal` |
| `Writable.toWeb(writable)` | Node writable, including HTTP ClientRequest → Web WritableStream | None |
| `Duplex.fromWeb(pair, options?)` | `{ readable, writable }` Web pair → Node Duplex | `objectMode`, `highWaterMark`, `encoding`, `decodeStrings`, `allowHalfOpen`, `signal` |
| `Duplex.toWeb(duplex)` | Node duplex → `{ readable, writable }` Web pair | None |

`objectMode` defaults to false for fromWeb conversion; opt into it for arbitrary
objects. `Duplex.fromWeb` defaults `allowHalfOpen` to false. Queue limits default
to the portable Node runtime's host defaults unless highWaterMark is supplied.
The adapter does not expose extra Node constructor override callbacks or per-side
Duplex mode/queue settings. Other undocumented options are not a supported contract.

```js
import { Readable } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { Writable } from 'node:stream';

await pipeline(
  Readable.fromWeb(ReadableStream.from([
    new TextEncoder().encode('hello\n')
  ])),
  new Writable({
    write(chunk, encoding, callback) {
      console.log(chunk.toString());
      callback();
    }
  })
);
```

## Reading and storage

`Readable.fromWeb` owns one default reader and reads only as the Node side requests
data. It preserves object identity in object mode. In byte mode it accepts the
chunks allowed by Node Readable; `encoding` uses the existing incremental decoder,
including multibyte characters split across Web chunks. Web failure destroys the
Node stream even if application code has not started consuming it.

`Readable.toWeb` pauses/resumes its Node source according to Web queue pressure.
By default the Web queue counts chunks and takes its highWaterMark from the Node
source. For byte-based accounting, supply `strategy.size: chunk => chunk.byteLength`
with a byte highWaterMark. This affects the Web queue; the Node stream and native
transport retain their own finite queues. A queue limit is not a total process
memory limit.

Node Buffer chunks become independent Uint8Array snapshots in Web byte mode,
so reusing a producer's Buffer does not mutate queued Web data. Object-mode chunks
retain identity. Sources using `setEncoding` continue producing strings. The Web
result is a default readable, not a byte stream with BYOB readers.

## Writing and completion

`Writable.fromWeb` holds an exclusive writer. Node write callbacks follow Web
write Promises; finishing Node closes the Web writer. `decodeStrings: false`
preserves string chunks, while the default passes encoded Buffer bytes. Rejected
Web writes/closed Promises destroy the Node side with the original error.

`Writable.toWeb` forwards each chunk to Node write. If Node accepts it without
backpressure, the Web write resolves before the Node write callback, as in Node.
If write returns false, the Promise waits for drain. Closing calls Node end and
waits for the writable terminal event, including finalization errors. Underlying
failure rejects pending drain/close and errors the Web controller. Already destroyed
or non-writable destinations convert to a closed Web writable. A completed write
does not promise durability or that the remote HTTP peer accepted the bytes.

Duplex conversions use the same two directional state machines. Destroying a
fromWeb duplex cancels the readable and aborts the writable; both shutdown Promises
settle before Node close. Tested workflows include TransformStream pairs, native
Transform pairs and a Node → Web → Node roundtrip.

## Ownership, cancellation and failures

Treat conversion as transfer of consumption responsibility. Mixing existing Node
data consumers or external end/destroy calls with an adapter is not fully tested.
fromWeb locks remain held even after successful completion or destruction, matching
the measured Node reference. Do not acquire a second reader/writer on that endpoint.

Node destroy cancels/aborts the Web endpoint with its reason. Web readable cancel
or writable abort destroys the Node resource with its reason. Mite waits for the
Node side's terminal event before resolving that cancellation Promise; Node can
resolve readable cancellation earlier. Custom sources whose destroy callback never
finishes can therefore keep Mite cancellation pending. Web abort waits for active
sink writes to settle; it cannot interrupt an indefinitely blocked user write.

fromWeb `signal` accepts Mite AbortSignals, including already-aborted ones. Aborting
destroys the adapter with AbortError (`code: 'ABORT_ERR'`, `cause: signal.reason`)
and propagates that error to Web cancellation. Trusted private signal subscriptions
work even if a public listener calls stopImmediatePropagation. They are removed
at destruction. Real error values otherwise retain their identity. A Node premature
close becomes AbortError with the original ERR_STREAM_PREMATURE_CLOSE as cause.

Wrong stream/options/signal values fail with TypeError/ERR_INVALID_ARG_TYPE.
Locked endpoints fail before acquisition, and malformed highWaterMark fails
ERR_INVALID_ARG_VALUE. Text, stack formatting and every validation edge are not
identical to Node. Mite validates Node constructor options before acquiring Web
locks; Node 24.15.0 can strand a lock after an invalid highWaterMark. Tests preserve
both observed outcomes explicitly. A pair's invalid second endpoint does not lock
the first. Mite removes its own data/drain listeners at termination; the measured
Node reference retains one. A small error guard remains to own late legacy errors.

## Verified network integration and remaining work

Independent Python HTTP peers hold a one-MiB response after its first 4096 bytes.
The client must read those bytes through Readable.toWeb before releasing the peer,
proving incremental delivery. A 256-KiB Web readable uploads through
Writable.toWeb(ClientRequest), with exact peer-side byte validation. A separate
held response is cancelled after its first block; the peer must observe connection
failure instead of accepting the remaining body. Actual Node and Mite both pass.

[Fetch/Request/Response/Headers](fetch.md) now use these bridges. Filesystem stream
factories, worker transfer, full mixed Web/Node pipeline/finished integration and arbitrary legacy stream parity
remain open. Native TLS bridge pressure/closure stress and every half-close/race/GC
boundary still require more coverage. These bridge tests are not full Web Platform
Tests or a complete Node stream conformance suite.

Run the [modern ESM bridge example](../../examples/mite-stream/web-bridge.mjs) with
`mite examples/mite-stream/web-bridge.mjs` or Node. It connects Web source/sink
endpoints through a Node Transform and is exercised under both runtimes.

Reference: [Node 24.15 stream interoperability API](https://nodejs.org/download/release/v24.15.0/docs/api/stream.html#streamreadablefromwebreadablestream-options).
