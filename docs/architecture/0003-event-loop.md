# ADR 0003: libuv event loop and host microtask checkpoints

Status: accepted; timer slice tested on macOS ARM64.

Use libuv 1.52.1 (pinned by the vcpkg baseline) for Mite's asynchronous operating
system services. It supplies native Windows, macOS and Linux timers, filesystem,
sockets and process operations without adding a Rust/C++ ownership boundary.
LibCore remains a LibJS dependency, but Mite does not run a LibCore event loop.
Compared with building Node-compatible phases and portable I/O adapters on
LibCore, libuv reduces our platform adaptation work. This is reuse of an I/O
library, not embedding Node.js. No libuv objects enter the JavaScript heap.

`EventLoop` owns a private `uv_loop_t`, all active timer handles, rooted callbacks,
receiver objects and arguments. One VM thread dispatches every JavaScript callback.
One-shot timers are removed before invocation; native close callbacks release
roots only after dispatch finishes. Cancelled and unreferenced handles are closed
and drained before the VM dies, including on exceptions. Failed callbacks stop
further JavaScript dispatch and produce a nonzero exit status.

Promise jobs and `queueMicrotask` callbacks share one FIFO queue of rooted GC
functions. A checkpoint runs after the entry script and after each timer callback.
Jobs added during a checkpoint are drained before the next native callback.
Unhandled rejections fail the run after that drain. A promise without native
handles does not keep the process alive. `process.nextTick`, immediates and process
rejection events remain unsupported; this is not yet the entire Node event loop.

A small embedded JavaScript bootstrap implements Timeout objects and argument
coercion over validated native operations. Internal installation hooks are removed
from the global object before user code runs. Timeout supports cancellation by
object or primitive ID, callback arguments/receiver, `ref`, `unref`, `hasRef`,
`close`, and disposal. `refresh` and Node's timeout warning emissions are pending.

Firefly integration must reconcile libuv dispatch with the native UI/LibCore
renderer transport while preserving single-threaded VM access. That integration
is still an open proof-of-concept requirement, not a working renderer backend.

References: [libuv timers](https://docs.libuv.org/en/v1.x/timer.html),
[Node timers](https://nodejs.org/api/timers.html),
[Node queueMicrotask](https://nodejs.org/api/globals.html#queuemicrotaskcallback).
