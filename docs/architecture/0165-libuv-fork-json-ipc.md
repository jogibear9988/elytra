# Independent Mite fork, JSON IPC and synchronous exit

Status: local macOS standalone Node/Mite and exact-production-object Firefly checks
pass. The unchanged pinned Code-OSS IPC component passes; the full ready graph next
requires fs.watchFile. Full watcher/workbench and fresh Windows/Linux acceptance
remain open.

## Decision

Extend Mite's existing C++ libuv child/pipe owner with a fourth duplex byte channel.
Share read/write/shutdown helpers and pending-root retirement with ordinary stdio;
preserve the process-handle mask and use a separate IPC bit. This needs no Node,
V8, LibWeb or Firefly dependency in Mite. Keep JSON framing/API policy in maintained
ESM, separate from process/pipe native ownership. Use libuv's native duplex pipe
flags on Unix and Windows; handle passing and foreign-runtime wire compatibility
are not supplied by this bytes-only implementation.

Fork the current executable into an explicit runtime-only role, with descriptor 3
and an internal environment marker. Validate the inherited descriptor before any
file read, VM or loop initialization can reuse a missing fd for a wakeup pipe.
A marker alone grants no capability. Strip the role flag/marker before application
entry and eagerly install child process IPC. File/eval execArgv remains truthful.
Firefly selects the same standalone Mite path before Qt/compositor or desktop/API
initialization; it does not create utility parentPort. Ordinary main processes
cannot enable child IPC through mutable public properties.

## State, pressure and cleanup

JSON snapshots precede native admission; native writes own copied bytes through
actual completion. Bound wire frames to 1 MiB, pending writes to 64/4 MiB and report
pressure at 128 KiB. Validate UTF-8/newline framing and partial-frame bounds. Retain
at most 64 messages/4 MiB before listener registration, replaying FIFO on nextTick.
Reserved NODE_ messages have their separate internalMessage event.

Completion is not application receipt. Disconnect drains admitted local writes,
marks connected false and retires the channel; EOF/failed spawn/error paths retain
correct callbacks and terminal state. Reference ownership is distinct for process
and channel. Child IPC starts unreferenced until listeners/writes require it;
explicit channel ref/unref overrides automatic holds. Queued messages alone do
not hold a listener-free child alive.

Detach callback GC roots before libuv shutdown drains canceled native requests.
Keep pipe/child storage alive until native close/completion callbacks finish, with
no application JS after VM shutdown. Parent VM teardown does not kill independent
children. Firefly utility processes retain their separate owned-child kill policy.

## Exit integration

The actual Code-OSS IPC client registers synchronous process.once('exit') cleanup.
Use one private native exit state for explicit process.exit and orderly Runtime
termination, marking before callback so reentrant exits never emit twice. Notify
while native services still exist, re-read listener-selected code and preserve
explicit host exit status. Uncaught failures select code 1 before listeners;
listener-selected exitCode changes follow the documented Node behavior. Terminal callbacks
cannot extend lifetime with new blocking work; preexisting host pending-work stop
policy remains explicit.

No new event-loop phase is driven after exit. Node24 can drain Promise reactions
at natural exit while Mite currently does not; document that difference, and keep
beforeExit/rejection/exception-recovery APIs explicitly unsupported.

## Evidence

Actual Node/Mite fixtures compare typed/base64 snapshots, pressure, late listeners,
failed spawn, disconnect/errors, process/channel references, exit ordering and
independent child survival. Eight actual exit cases pass. Controlled channel policy
is labeled separately from real transport. Negative bootstrap checks prove the
marker cannot attach an internally reused wakeup descriptor.

The exact Firefly application objects fork a child with an invalid Qt plugin name;
it still performs real IPC and exits without desktop imports or parentPort. The
unchanged pinned Code-OSS ipc.cp Client/Server passes 512 KiB VSBuffer/base64,
concurrent requests, remote errors, events and natural child exit, checking 44
compiled dependencies including NLS. No full watcher service or workbench follows
from this component. Source bytes and license remain pinned.

See [public fork/IPC](../api/child-process.md#forkmodulepath-args-options),
[exit contract](../api/process.md#exit-notification),
[Node reference](https://nodejs.org/download/release/v24.15.0/docs/api/child_process.html)
and [libuv process flags](https://docs.libuv.org/en/v1.x/process.html).
