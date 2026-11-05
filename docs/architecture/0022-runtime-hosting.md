# 0022: Share Mite's runtime between its CLI and native hosts

Status: standalone and external-loop embedding gate implemented. Connecting the
native Qt window and public Firefly modules remains required.

## Decision

Move the existing CLI initialization/execution body into Mite::run in Runtime.cpp.
The CLI main simply calls it. Build one static mite-runtime library containing
that implementation, event loop, loader and native services, with no Firefly,
LibWeb or Qt dependency in the standalone graph. Generate its bootstrap/builtin
sources and upstream metadata in a single reusable CMake function. Native hosts
must reuse the same sources, not copy a second runtime implementation.

Keep C++ for the hosting API: it passes the actual LibJS VM/realm and Mite loader
and loop to C++ platform bindings, without inventing an FFI ownership boundary.
HostHooks.initialize runs after Mite globals are installed and before the entry
module executes. A host can install privileged main-process bindings there.
HostHooks.drive owns the external loop until host/application work ends.
HostHooks.shutdown disconnects its callbacks before loader/loop/VM teardown,
including a partially failed initialize and fatal exits with pending native work.
These hooks exist only in the embedding C++ API, not as new application JS APIs.

The runtime owns the realm, process object, module loader and event loop for the
whole run. Hosts must stay on the VM owner thread, root retained JS values using
LibGC roots, dispatch native callbacks through EventLoop::dispatch and release
callbacks/roots on shutdown. No callback may outlive run(). Nonblocking pump_once
runs libuv with UV_RUN_NOWAIT and the same nextTick/promise/rejection checkpoints.
is_alive reports runtime liveness; it does not account for external windows.
The host is responsible for keeping its external loop alive for those windows.
Do not invoke the pump recursively or run it on another thread.

The standalone driver remains run_until_idle, preserving its existing blocking
libuv behavior. An external driver must return the loop's failure status; timeout
or an unresolved required operation cannot be silently treated as success. The
existing unsettled entry-module top-level-await and process.exitCode rules remain.

## Shared build and upcoming GUI integration

mite_add_runtime accepts an upstream JS target. The standalone graph uses
LibJSStandalone. A host also linking the pinned LibWeb must use the matching
LibJS target: at this revision LibJS may be an interface to the copy folded into
LibWeb, whereas LibJSStandalone links an independent runtime. Loading two copies
into one host would be an invalid ownership/ABI shortcut. The caller must supply
compatible pinned dependencies, including libuv and llhttp; the upstream browser
manifest does not supply those Mite dependencies yet. A Firefly dependency
manifest/build adapter must provide them outside the Ladybird submodule.

The test host is a console embedding fixture, not the Qt integration. It models
host-owned lifetime after libuv is idle, delivers a rooted external callback and
pumps actual Mite work without blocking. The next integration must join this
API with the already verified native window, register the real firefly module
for both CJS/ESM and implement app/BrowserWindow lifecycle. Merely running the
embedding fixture does not establish those features or Electron compatibility.

## Verification

Compare embedded and CLI outcomes for process/Buffer identity, ticks/promises,
timers, streams, callback errors and explicit exit codes. Execute an ESM entry
using real asynchronous filesystem I/O in both. Keep an otherwise idle host alive
to settle top-level await from an external callback and verify its checkpoint
ordering. Require exactly one shutdown on normal completion, partial host
initialization failure, native callback exception and unhandled rejection. On
POSIX, also leave a real FIFO read blocked while a host callback fails: shutdown
must run and fatal termination must exit without waiting for the blocked worker.
Also fail initialization after queuing blocking FIFO work, before entry execution;
this must disconnect the partial host and terminate promptly rather than hanging
in the event-loop destructor.
Run all existing differential/native suites after extracting the runtime, then
inspect standalone linkage. Native Windows/Linux embedding remains unverified.
