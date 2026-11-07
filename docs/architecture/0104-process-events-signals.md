# 0104: Process events with explicitly owned native signal watches

## Decision

Reuse Mite's authored ESM EventEmitter for process methods. Supply a separate
C++/libuv host for a restricted signal subset. The pinned Code-OSS bootstrap
registers SIGPIPE during evaluation, so this is a concrete bootstrap prerequisite.
Keep Mite independent of Firefly/LibWeb and leave Ladybird unmodified.

Each VM retains at most 32 native handles including deferred closes. Callback
roots remain live until removal; uv_close releases native storage asynchronously.
Native delivery enters the owner VM through the existing EventLoop::dispatch and
checkpoint path. Shutdown stops watches and clears roots while the VM is alive.
Unreferenced watches do not prolong process lifetime. Listener additions that
throw through newListener release unused watches.

libuv signal handlers are process-global. Embedders must opt in through HostHooks;
the standalone CLI and Firefly executable explicitly own that policy. A mutex
guards exclusive Mite ownership per signal. On POSIX, refuse existing custom
handlers before starting a watcher. Restore prior dispositions when libuv resets
the last handler to default, retaining another live libuv handler or a host's
observed replacement. Coordination remains mandatory: arbitrary competing host
mutations cannot be made atomic by this adapter.

## Limits and evidence

Only SIGINT/SIGTERM, available SIGPIPE/SIGHUP, and POSIX SIGUSR2 are permitted.
Do not take ownership of SIGCHLD, profiler/fault signals or uncatchable signals.
Lifecycle/rejection/IPC events reject explicitly until their actual sources exist.
Windows libuv signal registration is not proof of native event delivery.

The non-GUI process-events test compares actual custom events and Node behavior,
then sends two real signals to each owned child PID after readiness handshakes.
It checks once order, arguments, microtasks, cleanup and embedded-host collection.
Native protected-handler and ignored-disposition fixtures verify refusal and
restoration. Local macOS Release passes all 69 standalone suites. New native
Linux/Windows gates remain pending; Code-OSS desktop is still unsupported.

References: [Node 24.15 signal hooks](https://github.com/nodejs/node/blob/v24.15.0/lib/internal/process/signal.js),
[libuv signals](https://docs.libuv.org/en/v1.x/signal.html),
and [pinned Code-OSS bootstrap](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/bootstrap-node.ts).
