# 0062: Native process ownership and selective helper monitoring

Status: implemented; local acceptance recorded in PROGRESS. New hosted platforms
remain pending.

## Decision

Use C++ for direct LibJS/libuv integration, with the existing portable JS streams
and EventEmitter facade. Add no new FFI or runtime dependency. uv_spawn and native
pipes share Mite's owned EventLoop. JS argument/environment normalization completes
before native allocation; the native boundary checks strings, arrays, indices,
typed-array bounds and byte limits again.

Each child owns its process monitor and initialized pipes. Root its callback while
any handle remains; release native state only after every close callback. Pending
writes own bytes and rooted completion callbacks. Exit closes stdin but leaves
output readable until EOF/consumption. Stream destruction closes its native pipe;
the child close event follows process and stream closure. Teardown disconnects
monitoring/pipes and suppresses JS through EventLoop.dispatch; it does not kill an
independent detached child. Ref/unref semantics include separate pipe references.

## Firefly integration

The first real GUI child test timed out while standalone tests passed. The pinned
LibWebView ProcessMonitor installs SIGCHLD and calls waitpid(-1), including children
not in its monitored set. That conflicts with libuv's ownership of application
children and process-exit notifications.

Substitute one owned ProcessMonitor translation unit in the non-Windows renderer
build. Keep the pinned class ABI and copyright notice, but monitor only registered
helper PIDs with specific waitpid calls on a 10 ms Core timer. Do not install a
second SIGCHLD handler. Snapshot the set before notifications and remove stopped
timers on monitor destruction. The timer registry is process-static storage whose
entries are explicitly released. Windows keeps its upstream handle monitor.
This adds bounded helper-exit notification latency and polling overhead, not a
claim of identical signal timing.

tooling/check_process_monitor.py pins both upstream implementation and header;
configure/build fail on drift or an unexpected source inventory. Review/remove the
adapter when upstream offers selective/reusable monitoring. Ladybird remains
unmodified. Standalone Mite never links this LibWebView adapter.

## Scope and evidence

Implement spawn and ChildProcess observations/kill/ref/unref with three native
stdio slots, environment/cwd/argv0 and selected native flags/timeouts. Shells, IPC,
synchronous APIs, exec/execFile/fork, privilege switching and complete Socket/stdio
error parity remain separate requirements. Unsupported modes fail explicitly.

The differential suite uses independent Python children for bytes, cursor-independent
pipe flow, environment/quoting and lifecycle, including detached child survival
after an unreferenced parent exits. Fatal callback tests retain failure status.
Native Firefly tests must validate both application-child completion and continued
registered renderer/helper crash detection with the owned monitor. Follow
[libuv's process API](https://docs.libuv.org/en/v1.x/process.html) and the public
[Node contract](https://nodejs.org/download/release/v24.15.0/docs/api/child_process.html);
do not replace native process execution with a shell, Node embedding or JS success
placeholders.

## Windows CLI inheritance policy

The CLI calls uv_disable_stdio_inheritance before configuring/duplicating inherited
streams, following the [libuv startup recommendation](https://docs.libuv.org/en/v1.x/process.html#c.uv_disable_stdio_inheritance).
Pinned libuv's Windows spawn path uses inheritable handles; explicit stdio modes
still deliberately duplicate their requested handles. This is CLI process-start
policy, not a global side effect inside the embeddable Mite::run entry point.
The library call is best effort and does not certify arbitrary-handle confinement.

The detached-child regression observes parent process exit separately from output
pipe EOF, using the same combined three-second deadline. An independent Python
control deliberately keeps inherited capture pipes open after its parent exits,
then releases through an acknowledgement. The normal Node/Mite ignored-stdio
workflow must deliver parent exit AND EOF before acknowledging its still-living
child. Cleanup deadlines never extend the acceptance window. Windows acceptance
of the startup correction remains pending.
