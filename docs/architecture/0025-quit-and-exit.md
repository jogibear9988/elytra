# 0025: Cancellable quit and explicit application exit

Status: native macOS implementation and Electron 45.2.3 comparison; public lifecycle
support remains partial. Renderer beforeunload and OS activation/quit are separate
remaining work.

## Observable contract

Use the official [Electron app API](https://www.electronjs.org/docs/latest/api/app/)
and actual reference runs. A before-quit cancellation keeps windows open. A close
cancellation prevents will-quit. Cancelling will-quit leaves an initialized app
with its windows already closed; a later window and quit attempt must work. The
shared cancellation/retry fixture checks these transitions and exact event order
in both runtimes.

Set the Firefly quit-in-progress flag before calling before-quit listeners. Nested
quit calls during any stage are coalesced into the current attempt; cancellation
resets the flag so a later attempt can proceed. This is an intentional difference:
Electron 45.2.3 repeats before-quit and close in the bounded nested-call fixture.
Firefly emits each once, preventing an unbounded callback recursion. Keep separate
expected results for this policy difference rather than labelling it full parity.

app.exit(code) bypasses before-quit, close cancellation, will-quit and
window-all-closed. Native widgets are destroyed synchronously, wrappers are marked
destroyed and closed is emitted before quit. The explicit signed 32-bit code is
retained by the host and wins over process.exitCode, including explicit zero and
13 (which otherwise also denotes unsettled entry top-level await in Mite).
Invalid codes fail before changing lifecycle state.

Although the documentation describes exit as immediate, the reference returns to
the current JS call after emitting quit. Firefly therefore requests native-loop
termination and allows that current turn/checkpoint to finish, rather than killing
the process inside the native method. Further async callback timing after exit is
not claimed compatible. OS close requests enter JS through the queued bridge, so an app.exit listener
can destroy a widget after its native closeEvent has returned. The bridge
disconnects/cancels queued GUI callbacks on stop. It also guarantees a termination request if an application closed/quit
listener throws. An explicit exit before readiness must not turn a remaining
unsettled entry-module await into an error.

Mite's generic HostHooks gains an optional explicit exit code. It bypasses normal
entry-completion/process.exitCode selection once the host requests termination.
There are no Firefly names or GUI dependencies in this runtime interface. The
existing fatal/pending-work termination path remains, including its later C++
teardown/temporary-profile cleanup limitation from ADR 0024.

## Verification scope

Run the shared cancellation/retry and forced-exit fixtures in Electron and
Firefly. Compare closed/quit order and signed codes 0, 7, 13, -1, 255, 256 and
INT32_MAX, both with process.exitCode pre-set to another value. Check OS exit
status modulo 256 on the tested POSIX host, while the quit event keeps the signed
value. Both runtimes return to the current JS call. The bounded reentry fixture
records the intentional difference explicitly. Firefly-specific tests also cover
exit before readiness with unsettled ESM await and invalid argument rejection.
Rerun the complete app gate and standalone Mite regression after runtime changes.
Native Windows/Linux and exact post-exit async ordering remain unverified.

## Preload boundary found during this step

Source inspection at the pinned Ladybird revision found two distinct layers:

- LibWeb Bindings/WrapperWorld and HostDefined support per-world wrappers and
  intrinsics associated with a principal realm. Tests/LibWeb/TestWindowProxyWorld.cpp
  demonstrates extension-world Window/WindowProxy construction and shared DOM
  identity. These tests were read, not newly executed by Firefly here.
- The WebDriver BiDi sandbox path in WebDriver/BiDi/Script.cpp explicitly rejects
  sandbox realms; its preload path skips sandboxed scripts. WebContent/PageClient's
  run_javascript evaluates against the active document's existing settings/realm.
  Neither is an implemented isolated-preload entry point.

Keep preload/contextBridge unsupported. A concrete next prototype is a
Firefly-owned renderer helper using the existing wrapper-world primitives and
BiDi's synchronous document-is-ready-for-scripts/EventSink boundary before author
JS. The helper must own composition with WebContent's BiDi session-state/event
routing rather than replacing that boundary with main-world evaluation. The
existing service classes are final; a small, separately compiled service-entry
adapter (reusing upstream translation units) needs a build/runtime proof before
choosing it over a larger owned PageClient adapter. No vendored source change or
new upstream patch is introduced by this investigation.

Acceptance requires before-author-script ordering, wrapper/intrinsic isolation,
explicit world detach on navigation/crash/unload, rooted lifetimes, structured
copies and function/Promise membranes with no raw cross-world objects. Only then
add a bounded, authenticated, per-frame/document IPC channel and Electron-style
preload/contextBridge APIs. Do not expose WebDriver's general command surface or
pretend an IIFE/closure in the page realm establishes context isolation.
