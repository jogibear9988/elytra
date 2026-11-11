# utilityProcess and process.parentPort

Status: **partial, locally tested on macOS** with real Firefly and Electron 45.2.3.
Actual LibJS child execution, binary messaging, stdio/lifecycle, rapid failed entries
and parent teardown pass, as do six native boundary tests. Full Electron parity,
Code-OSS shared-process execution and fresh Linux/Windows remain unverified.
Initial main-to-utility MessagePort handoff now passes a real no-window native
fixture on macOS. Renderer/DOM handoff and transfer back from the child remain
unavailable; this does not establish the Code-OSS SharedProcess handshake.

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
| serviceName | Bounded UTF-8 label retained by the native child owner; defaults to Node Utility Process. Abnormal exits emit app child-process-gone with the actual PID and this name. App process metrics remain unsupported. |
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
  // A main-to-utility transfer supplies native-backed endpoints in ports.
  process.parentPort.postMessage({ operation: data.operation, runtime: 'LibJS' });
  process.exit(0); // Immediate exit can truncate unrelated pending asynchronous writes.
});
```

The parent calls child.postMessage(message[, transfer]); child messages arrive as
raw values on child.on('message'). The child receives an event containing
{ data, ports }. Messages without attachments have an empty ports array.
Messages queue before its first message listener and delivery
is asynchronous FIFO. parentPort.start() also enables delivery. The reference and Firefly keep an idle
utility alive even without listeners; explicitly exit or kill it when done. Broader
reference management and exact task timing remain unverified.
No close/ref/unref parity is promised. ParentPort is installed only in a genuine
utility child with the private inherited native channel, not ordinary Mite scripts.

Both directions use the native [IPC data subset](ipc.md#data-and-resource-limits):
plain supported data graphs, cycles/aliases and fixed ArrayBuffer/view snapshots.
The aggregate distinct binary backing-store budget is 64 KiB per message and the
encoded frame limit is 128 KiB. Data is copied into independently writable receiver
storage. This is not zero-copy transfer.

The main process can transfer up to 16 owned MessageChannelMain endpoints with
child.postMessage(value, [port]). Native write admission precedes detaching the
old wrapper. Failed admission leaves the old endpoint usable. The utility receives
a stopped endpoint in event.ports: register message/close listeners and call start().
Port messages are asynchronous, preserve FIFO and copy their data/binary snapshots
over the child's private native pipe. Internal route identities never appear in
application messages. Existing queued data follows the transferred endpoint.

The initial native slice refuses child-to-parent transfers, attachments inside
port.postMessage, queued messages that themselves carry ports, and forwarding an
already native-backed endpoint to another utility. These refusals occur before
changing the old owner. ArrayBuffers are copied; they are not transfer-list entries.
See [MessageChannelMain](message-channel-main.md) for wrapper/GC behavior.

Native routes have at most 16 outstanding messages and 1 MiB payload bytes per
direction. Each child reserves separate 4 MiB send/receive windows (8 MiB total),
so simultaneous duplex traffic cannot consume the other's receive reservation.
The native ledger supports at most 256 active routes per child; the shared wrapper
core also enforces its 256-endpoint-record budget. ACKs release exact FIFO byte
credits after delivery. Paused ports retain bounded work; native writable events
retry data and control frames without duplicate delivery. Close/channel loss/exit
retire routes, and stale native identities cannot reopen them. These are copied,
brokered endpoints, not descriptor transfer or a zero-copy performance claim.

The child facade retains at most 64 incoming data messages / 1 MiB encoded bytes.
The native channel allows 16 pending writes / 2 MiB framed bytes per endpoint, with
32 owned native children at once. Admission failures do not indicate successful
delivery. Invalid graphs/oversized frames fail during encoding. Native send failures
report ERR_FIREFLY_UTILITY_CHANNEL; spawn failures use ERR_FIREFLY_UTILITY_SPAWN.
An already exited parent's child wrapper ignores postMessage, matching the current
compatibility target; a closed child parentPort explicitly refuses new sends.
These implementation bounds and error codes are not Electron API guarantees.

## Remaining Code-OSS requirements

The actual local entry now creates its renderer, loads the Workbench graph and
forks the file-watcher and shared-process utilities. Genuine renderer relay now
passes a focused offscreen fixture and the actual watcher channel connects. The
pinned local adaptation discloses omission of implicit inspector defaults; explicit
inspector requests remain unsupported. Follow the [Code-OSS status](../compatibility/vscode.md);
component port acceptance does not establish complete SharedProcess service startup.

The actual pinned utility wrapper first sends configuration, then expects genuine
ipc-ready/init-done responses from bootstrap-fork and the shared-process module.
A connection subsequently sends an actual MessageChannelMain endpoint into that
child and transfers another endpoint to a renderer. Main-to-utility handoff now
has actual native component acceptance, including the subsequent renderer/DOM relay.
The default shared-process configuration also enables authentication forwarding,
which is explicitly unsupported. These are separate blockers before running the
actual shared-process services or a VS Code workbench.

See [ADR 0155](../architecture/0155-native-utility-processes.md),
[main ports](message-channel-main.md) and [Code-OSS status](../compatibility/vscode.md).
Reference contracts: [Electron utilityProcess](https://www.electronjs.org/docs/latest/api/utility-process)
and [parentPort](https://www.electronjs.org/docs/latest/api/parent-port).

Utility exit attribution preserves the native exit status; a signal termination is
reported as a negative signal number in `child-process-gone.exitCode`. A clean
zero exit emits the child `exit` event without an app failure event. This does not
claim Electron process metrics or identical platform crash classification.

No-session utilities use their own system network context. They do not inherit
main `defaultSession.webRequest` filters, matching the
[Electron45.2.3 factory branches](https://github.com/electron/electron/blob/v45.2.3/shell/browser/api/electron_api_utility_process.cc#L539-L557).
`session` and `partition` options remain explicitly unsupported; this separation
does not implement session sharing or main-process HTTP/client-certificate auth.
