# MessageChannelMain and main-process ports

Status: **partial, main-realm only**, locally tested on macOS with real Firefly,
Electron 45.2.3 and a headless native codec/GC host. Renderer/utility-process handoff,
fresh Linux/Windows execution and a working Code-OSS desktop remain unverified.

## Create and use a connected pair

```js
import { app, MessageChannelMain } from 'firefly';

app.whenReady().then(() => {
  const { port1, port2 } = new MessageChannelMain();
  port2.once('message', ({ data }) => {
    console.log(data.greeting);
    port1.close();
    port2.close();
    app.quit();
  });
  port2.start();
  port1.postMessage({ greeting: 'Hello through a real port' });
});
```

The Electron alias exposes the same constructor when --electron-compat is enabled.
Construct with new; subclassing is unavailable. port1 and port2 are connected
EventEmitter-style MessagePortMain instances. MessagePortMain is intentionally not
an exported constructor, matching Electron: endpoints come from channels/transfers.
Calling an endpoint's reflected constructor directly or borrowing methods for a
forged receiver rejects explicitly.

port.postMessage(message[, transfer]) copies a supported data graph at call time
and queues a message on its peer. port.start() enables asynchronous delivery;
adding a message listener alone does not start a main port. Each message event
contains { data, ports }, with ports an array of received main endpoints. FIFO
holds per endpoint, with one delivery scheduled per event-loop turn. There are no
Node worker_threads, JSON fallback clones or fake renderer endpoints underneath.

## Data and ownership transfer

The native [binary IPC data subset](ipc.md#data-and-resource-limits) is reused: undefined,
null, booleans, numbers, UTF-16 strings, plain objects/arrays and supported fixed
ArrayBuffer/typed-array/DataView values. Cycles, shared object references, array holes
and binary backing/view aliases survive within each snapshot. Buffer becomes
Uint8Array. The receiver owns independent writable copied storage; ArrayBuffers are
copied, not transferred/detached. The aggregate distinct backing-store limit is
64 KiB per message, including an entire backing store when sending a small view.
Wire encoding is bounded by 128 KiB. Functions, getters/proxies, symbols/BigInt,
Date/Map/Set, DOM/File/Blob and other special objects remain unsupported by this graph.

The optional transfer array contains at most 16 currently owned main ports. Only
ports move; an ArrayBuffer in this list is invalid. Pass ports in the transfer list,
not inside the data graph:

```js
const carrier = new MessageChannelMain();
const connection = new MessageChannelMain();
carrier.port2.on('message', ({ ports }) => {
  const received = ports[0];
  received.on('message', ({ data }) => console.log(data));
  received.start();
});
carrier.port2.start();
connection.port1.postMessage(new Uint8Array([1, 2, 3]));
carrier.port1.postMessage({ kind: 'connection' }, [connection.port2]);
```

The received wrapper is a new object with ownership of the same endpoint, its
queued messages and entanglement. It starts stopped even if the previous owner had
started it. The old wrapper is detached; its postMessage/start/close methods become
no-ops. Moves may be repeated through further main channels. Stale delivery tasks
and GC callbacks cannot address a newer ownership generation.

Validation, cloning, queue admission and wrapper preparation precede ownership
changes. A refused clone/quota/duplicate/self/stale transfer preserves existing
ownership. Inherited emitter construction can execute application code: stage those
wrappers first and revalidate source, destination, generations and queue quotas
before commit. Captured registration/slot-insertion operations protect the accepted
commit path from the tested reentrant/throwing hooks and replaced Array.push.
This is not a general security guarantee against arbitrary privileged main-realm
intrinsic replacement; renderer isolation/native process authentication is separate.

## Close, GC and resource bounds

close() disconnects an endpoint, drops its incoming queue and retires undelivered
transferred endpoints. The peer receives already accepted messages before its close
notification, after start. Local and peer close events occur once. Firefly defers
local close notification, whereas Electron can emit it synchronously. A pending local
notification retains its owner until delivery. Methods on explicitly closed/detached
wrappers are no-ops; postMessage on such a wrapper does not clone invalid data.
Application exit shuts down owned queues and stops new channels.

Unreachable endpoint collection disconnects a surviving peer. A weak record table
with scalar queue leases lets unreachable cycles of queued transferred ports collect
and releases their endpoint/byte budgets. It does not root those cycles forever.
This uses real VM GC and does not promise GC timing; close explicitly when done.

Bounds per API instance are 256 live endpoint records, 64 queued data messages and
1 MiB encoded queued bytes per endpoint, 4 MiB encoded queued bytes globally, and
16 transferred endpoints per message. Wire metadata is charged too. A draining
remote-close record still occupies a slot until it is started/closed/collected.
Limits reject with ERR_FIREFLY_PORT_LIMIT. Data errors use
ERR_FIREFLY_PORT_SERIALIZATION; invalid ownership uses ERR_FIREFLY_PORT_TRANSFER or
ERR_INVALID_ARG_TYPE. Error messages/codes and exact task/close timing differ from
Electron. These are bounded copies, not a zero-copy, throughput or file-transfer
performance claim.

## Missing native handoffs

Only main-realm pairs/transfers are accepted. webContents.postMessage currently
rejects explicitly. ipcRenderer.postMessage, DOM MessagePort handoff, native
endpoint/process/document binding and cross-process event.ports delivery remain
required work. A separate [utilityProcess payload slice](utility-process.md) now
implements a real LibJS fork and process.parentPort; six native boundary tests pass,
with local public Firefly/Electron lifecycle and binary payload acceptance passing. It explicitly refuses nonempty transfers
and authentication forwarding, and does not provide a native port endpoint. No serializable public
port IDs are offered as a substitute. Closed/destination/disconnected endpoint
transfers are explicitly unavailable even where Electron accepts them.

The public test compares real Firefly/Electron copy/FIFO/start/transfer/close behavior
without opening windows. A genuine headless Mite/native BridgeData probe covers GC,
queued cycles, quota reclamation, snapshots and reentrant ownership cases. The actual
pinned Code-OSS Protocol/once helper passes binary frames, echo, disconnect and main
transfer using real compiled base services. That does not run its utility/shared
process or renderer. The real ready graph now passes utilityProcess/safeStorage imports
and fails at missing shell. This is not acceptance of Code-OSS utility execution;
[current Code-OSS status](../compatibility/vscode.md) tracks subsequent blockers.
See [ADR 0154](../architecture/0154-main-message-port-ownership.md).

References: [Electron MessageChannelMain](https://www.electronjs.org/docs/latest/api/message-channel-main),
[MessagePortMain](https://www.electronjs.org/docs/latest/api/message-port-main).
