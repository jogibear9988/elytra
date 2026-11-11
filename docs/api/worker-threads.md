# node:worker_threads

Status: partial implementation using real native threads and separate LibJS VMs,
heaps, module loaders and libuv loops. The focused Node 24 reference and actual macOS ARM64 Mite lifecycle fixtures pass,
including same-PID isolation, clone ownership, CJS messaging, local process.exit,
error propagation, tight-loop termination and terminate after unref. Fresh native
Linux/Windows acceptance remains open.
There is no Node/V8 runtime or subprocess standing in for a thread.

```js
import { Worker, isMainThread, parentPort } from 'node:worker_threads';

if (isMainThread) {
  const worker = new Worker(new URL(import.meta.url), { name: 'Example' });
  worker.once('message', (reply) => console.log(reply));
  worker.postMessage({ text: 'hello' });
} else {
  parentPort.once('message', (value) => parentPort.postMessage(value));
}
```

## Entry points and lifetime

`new Worker(filename[, options])` accepts an absolute path, an explicitly relative
`./` or `../` path, or a file URL. CJS and ESM entries use the same actual module
loader as the CLI. `name` and an empty `execArgv` are the supported options; other
options, including eval/workerData/custom environment/stdio/resource limits and
transfer lists, explicitly reject. Each VM thread receives an 8 MiB native stack.
At most 32 workers, including workers awaiting join, may exist process-wide.

The EventEmitter interface supports `online`, `message`, `error` and `exit` events.
`postMessage`, `ref`, `unref` and Promise-returning `terminate` are implemented.
Termination interrupts actual JavaScript execution, wakes the native loop and
joins the owner thread before freeing its handles. This includes entries that
never import worker_threads. Native operations must still complete safely; this
is not a guarantee of immediate cancellation of blocking operating-system work.

Worker `process.pid` remains the process PID. Each worker has an independent
JavaScript global object and environment snapshot; worker environment mutations
stay local, chdir rejects, and process.exit exits only that worker. Nested workers
inherit the creating worker's environment snapshot. CLI process startup and signal
ownership are not repeated on worker threads. Default console/stdout writes use
borrowed process descriptors; custom worker stream routing is unsupported.

`isMainThread`, `threadId`, `threadName`, `parentPort` and `workerData` describe the
actual VM role. `isInternalThread` is false because these are user-created workers.
No workerData option is supported yet. A worker's threadId becomes -1 after exit.

## Messages and limits

Messages are independent snapshots of plain objects, arrays, strings, numbers,
bigints, ArrayBuffers and Uint8Arrays/Buffers. Cycles and repeated references are
preserved within the supported graph; Buffer values arrive as Uint8Arrays.
Shared memory, transfer/detachment, other typed-array classes, custom prototypes
and unsupported object kinds reject rather than silently changing their semantics.

Limits are depth 128, 65,536 graph nodes, 131,072 properties, 4,194,304 UTF-16 string
code units, one MiB of aggregate binary backing storage and eight MiB encoded
wire per message. Each native direction queues at most 64 messages and 16 MiB.
Queue exhaustion fails explicitly. No JavaScript object or GC handle crosses a
thread: native queues own bytes and each receiving VM materializes its own values.

The genuine parentPort supports EventEmitter message listeners, postMessage,
start, close, ref/unref and hasRef. MessageChannel, transferred ports, shared
memory, Atomics coordination, Web-style MessagePort event APIs and advanced Worker
profiling/resource operations remain unsupported. This is not full Node structured
clone or worker_threads parity. The public baseline is the
[Node 24 worker reference](https://nodejs.org/download/release/v24.15.0/docs/api/worker_threads.html).
