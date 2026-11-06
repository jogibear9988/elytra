# Web Streams

Status: partial, tested locally with standalone Mite on macOS against Node 24.15.0.
The primitive/bridge baseline dc3308b also passes all 55 hosted suites on Linux
x64 and Windows x64. Fresh Fetch acceptance remains pending. This supplies stream primitives;
global [Fetch, Request, Response and Headers](fetch.md) now have a partial
transport/body implementation.

The Mite CLI and Firefly main process expose the same constructors through globals,
CommonJS `require('node:stream/web')` and ESM imports. Changing a writable global
does not replace the module's original constructor. Ladybird renderer streams
remain a separate object world; main-process streams cannot cross IPC as streams.

```js
import { ReadableStream, TransformStream, WritableStream } from 'node:stream/web';

await ReadableStream.from(['mite', 'firefly'])
  .pipeThrough(new TransformStream({
    transform(value, controller) {
      controller.enqueue(value.toUpperCase());
    }
  }))
  .pipeTo(new WritableStream({
    write(value) {
      console.log(value);
    }
  }));
```

## Readable streams

`new ReadableStream(source?, strategy?)` accepts a source with `start(controller)`,
`pull(controller)` and `cancel(reason)` callbacks. Controllers provide `enqueue`,
`close`, `error` and `desiredSize`. A producer should use pull/desiredSize to bound
queued work. The default queue counts chunks; `CountQueuingStrategy` and
`ByteLengthQueuingStrategy` accept `{ highWaterMark }` to select queue accounting.

`getReader()` locks a stream until `releaseLock()`. `read()` returns a Promise of
`{ value, done }`; `closed` tracks terminal completion or failure. Calling stream
`cancel()` while locked rejects with TypeError; use the reader's `cancel(reason)`.
Cancellation passes the original reason to the source and completes pending reads.
Source errors reject reads/closed with the original error value.

`ReadableStream.from(iterable)` accepts synchronous or asynchronous iterables.
Streams support `for await`, plus `values({ preventCancel })`. Leaving iteration
early normally cancels the source; `preventCancel: true` releases the lock while
preserving the source. Cancellation of an iterable-backed stream closes its iterator.

`tee()` returns two branches. Cancelling only one branch leaves the source alive;
both cancellations supply an ordered pair of reasons to the source. Byte-stream
branches receive independent byte storage. Ordinary object chunks are not cloned.

## Byte readers and buffer ownership

A source with `type: 'bytes'` creates a byte stream. `getReader({ mode: 'byob' })`
returns a ReadableStreamBYOBReader; pass an ArrayBuffer view to `read(view)`.
The source fills `controller.byobRequest.view`, then calls `respond(bytesWritten)`.
The result may be shorter than the supplied view; retain the returned view.

Buffers are transferred using LibJS's native `ArrayBuffer.prototype.transfer`.
The caller's buffer detaches on a BYOB read and on enqueueing bytes into a byte
stream. Do not reuse the original view. Tests verify detachment, offsets, returned
bytes, EOF and byte-tee storage isolation. Shared/resizable buffers and every
BYOB minimum-fill/close boundary are not covered by the local suite.

## Writable streams and transforms

`new WritableStream(sink?, strategy?)` accepts `start(controller)`,
`write(chunk, controller)`, `close()` and `abort(reason)`. A returned write Promise
holds back later sink writes. `getWriter()` locks the stream; writer `write`,
`close` and `abort` return Promises. `desiredSize` reports queue capacity and
`ready` waits for pressure to clear. `closed` reports terminal success/failure.
Observe rejected operation/closed Promises. Release the writer when finished.

`WritableStreamDefaultController.signal` exposes cancellation to an active sink.
Calling writer/stream abort preserves the original reason, aborts that signal,
and invokes the sink's abort callback after active writes settle.

`new TransformStream(transformer?, writableStrategy?, readableStrategy?)` exposes
`readable` and `writable`. Transformer callbacks include `start`, `transform`,
`flush` and `cancel`; the controller supports `enqueue`, `error`, `terminate` and
`desiredSize`. Tests verify transformation/flush ordering and error propagation
to both reader and writer. All termination/cancellation timing is not yet verified.

## Piping and cancellation

`source.pipeTo(destination, options?)` handles pressure and acquires both locks.
Success closes the destination; source failure aborts it; destination failure
cancels the source. `preventClose`, `preventAbort` and `preventCancel` change those
propagation policies. `pipeThrough({ readable, writable }, options?)` returns the
readable end while connecting the writable end.

`options.signal` supports live and already-aborted AbortSignals. The original
reason reaches the pipe Promise and affected endpoints. Tests cover both error
directions, signal cancellation, prevention flags and released locks. This portable
implementation subscribes to public abort events; it does not use Mite's private
native-consumer cancellation hooks. Suppressing or synthesizing those events can
therefore affect piping. Prefer ordinary `controller.abort(reason)`.

## Exports and limits

The module exports ReadableStream, ReadableStreamDefaultReader,
ReadableStreamBYOBReader, ReadableStreamDefaultController,
ReadableByteStreamController, ReadableStreamBYOBRequest, WritableStream,
WritableStreamDefaultWriter, WritableStreamDefaultController, TransformStream,
TransformStreamDefaultController, ByteLengthQueuingStrategy and CountQueuingStrategy.

Implementation: pinned MIT-licensed `web-streams-polyfill` 4.5.1, bundled into the
runtime with integrity/source metadata and distributed license notices. It runs
on LibJS without Node, V8 or LibWeb. No runtime npm installation is required.
The differential suite covers selected behaviors, not the complete Web Platform
Tests or full Node compatibility. Async iterator prototype details follow the
portable package. Replacing globals used by the package (such as AbortController)
can affect future stream construction.

TextEncoderStream, TextDecoderStream, CompressionStream and DecompressionStream
are absent. Missing exports fail ordinary API/import checks. Readable, Writable
and Duplex `toWeb`/`fromWeb` now have owned adapters; see [Node/Web stream
bridges](stream.md). Independent HTTP tests cover incremental responses, streamed
uploads and cancellation. Filesystem Web Stream factories, transferable streams/
postMessage and worker integration remain absent. [Fetch body objects](fetch.md)
have a separate tested partial contract.

References: [Node 24.15 Web Streams API](https://nodejs.org/download/release/v24.15.0/docs/api/webstreams.html),
[portable implementation](https://github.com/MattiasBuelens/web-streams-polyfill).
