# utilityProcess and process.parentPort

Status: **partial, locally tested on macOS** with real Firefly and Electron 45.2.3.
Actual LibJS child execution, binary messaging, stdio/lifecycle, rapid failed entries
and parent teardown pass, as do six native boundary tests. Full Electron parity,
Code-OSS shared-process execution and fresh Linux/Windows remain unverified.
Native MessagePort handoff remains unavailable.

## Fork a utility module

```js
import { app, utilityProcess } from 'firefly';

app.whenReady().then(() => {
  const child = utilityProcess.fork('./worker.mjs', ['example'], {
    stdio: 'pipe'
  });
  child.stdout.on('data', (bytes) => console.log(bytes.toString()));
  child.on('message', (message) => console.log(message));
  child.on('error', (error) => console.error(error));
  child.on('exit', (code) => console.log('Utility exited:', code));
  child.postMessage({ operation: 'describe' });
});
```

The Electron alias exposes utilityProcess when Electron compatibility is enabled.
Call fork(modulePath[, args][, options]) after app ready. The currently maintained
signature accepts an args array before options; pass [] when only configuring
options. Paths resolve against the parent's current working directory. The module
runs in a separate OS process using the same Firefly executable in its early
utility mode, before Qt/application/window initialization. Its JavaScript runtime
is Mite/LibJS; Node, V8 and a renderer are not embedded in this child.

The returned object is an EventEmitter, not a directly constructible exported
UtilityProcess class. Its pid is undefined before spawn and after exit. A spawn
event announces the PID; message receives the raw copied value sent by the child;
exit receives an integer exit code. Error and output events reflect the native
process/channel, rather than simulated child responses. Natural exit code 7, final
message delivery, PID retirement, pipe EOF and kill behavior pass the shared local reference fixture. Exact task timing, crash attribution
and full Electron lifecycle parity remain unverified.

## Options and streams

| Option | Current source behavior |
| --- | --- |
| env | Object of string values; replaces the inherited environment when supplied. Undefined entries are omitted. Defaults to process.env. |
| cwd | Optional child working directory. |
| stdio | inherit by default; pipe and ignore also supported. A three-element array may independently select stdout/stderr, with stdin fixed to ignore. |
| serviceName | Explicitly unsupported; no crash attribution or app child-process-gone reporting is implemented. |
| execArgv | Empty array accepted; nonempty engine flags explicitly refused. |
| allowLoadingUnsignedLibraries | Omitted/false accepted; enabling explicitly refused. |
| respondToAuthRequestsFromMainProcess | Omitted/false accepted; enabling explicitly refused. |

Piped stdout/stderr are actual Readable streams receiving native byte chunks. They
are null for inherit/ignore and become null when their respective EOF notifications
arrive. Save a stream reference when observing its end event. Ignored/unconsumed
piped output is bounded: exceeding the 1 MiB retained stream budget destroys the
Readable, requests SIGTERM and reports ERR_FIREFLY_UTILITY_LIMIT. A child can handle
SIGTERM, so this request does not guarantee termination; owned-process teardown
uses native SIGKILL. stdin pipes/additional descriptors,
unknown options, session/partition selection and native-addon loading are unavailable.

kill() requests native termination and returns whether a request was accepted;
a repeated request returns false. Application shutdown closes owned channels and
terminates owned children. Native exit/reaping is independent of JavaScript wrapper
reachability. A delivered message is not an acknowledgement that an application task
completed; implement a reply protocol for that purpose.

## Parent and child messages

A child module uses its installed process.parentPort:

```js
process.parentPort.once('message', ({ data, ports }) => {
  // Native endpoint transfer is not available yet; ports is currently empty.
  process.parentPort.postMessage({ operation: data.operation, runtime: 'LibJS' });
  process.exit(0); // Immediate exit can truncate unrelated pending asynchronous writes.
});
```

The parent calls child.postMessage(message[, transfer]); child messages arrive as
raw values on child.on('message'). The child receives an event containing
{ data, ports: [] }. Messages queue before its first message listener and delivery
is asynchronous FIFO. parentPort.start() also enables delivery. The reference and Firefly keep an idle
utility alive even without listeners; explicitly exit or kill it when done. Broader
reference management and exact task timing remain unverified.
No close/ref/unref parity is promised. ParentPort is installed only in a genuine
utility child with the private inherited native channel, not ordinary Mite scripts.

Both directions use the native [IPC data subset](ipc.md#data-and-resource-limits):
plain supported data graphs, cycles/aliases and fixed ArrayBuffer/view snapshots.
The aggregate distinct binary backing-store budget is 64 KiB per message and the
encoded frame limit is 128 KiB. Data is copied into independently writable receiver
storage. This is not zero-copy transfer. Nonempty transfer lists fail explicitly
with ERR_FIREFLY_UNSUPPORTED_API; no public IDs stand in for actual ports.

The child facade retains at most 64 incoming data messages / 1 MiB encoded bytes.
The native channel allows 16 pending writes / 2 MiB framed bytes per endpoint, with
32 owned native children at once. Admission failures do not indicate successful
delivery. Invalid graphs/oversized frames fail during encoding. Native send failures
report ERR_FIREFLY_UTILITY_CHANNEL; spawn failures use ERR_FIREFLY_UTILITY_SPAWN.
An already exited parent's child wrapper ignores postMessage, matching the current
compatibility target; a closed child parentPort explicitly refuses new sends.
These implementation bounds and error codes are not Electron API guarantees.

## Remaining Code-OSS requirements

The fresh actual ready/main probe completes its entry and reaches app ready, then
stops at the next missing static export, WebContentsView. Resolving utilityProcess at
import time does not demonstrate that Code-OSS has forked a utility child.

The actual pinned utility wrapper first sends configuration, then expects genuine
ipc-ready/init-done responses from bootstrap-fork and the shared-process module.
A connection subsequently sends an actual MessageChannelMain endpoint into that
child and often transfers another endpoint to a renderer. Neither native port
handoff nor renderer/DOM endpoint binding is implemented by this payload channel.
The default shared-process configuration also enables authentication forwarding,
which is explicitly unsupported. These are separate blockers before running the
actual shared-process services or a VS Code workbench.

See [ADR 0155](../architecture/0155-native-utility-processes.md),
[main ports](message-channel-main.md) and [Code-OSS status](../compatibility/vscode.md).
Reference contracts: [Electron utilityProcess](https://www.electronjs.org/docs/latest/api/utility-process)
and [parentPort](https://www.electronjs.org/docs/latest/api/parent-port).
