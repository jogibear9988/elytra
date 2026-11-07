# ADR 0097: Native Windows Qt/DirectX window prototype

Status: implemented prototype/build gate; native Windows compilation and window
acceptance pending. The production Windows Firefly runtime remains unsupported.

## Context

The pinned SDK contains the native Qt WebContentViewWindows DirectX implementation.
The first native Windows renderer workflow builds headless/realm/wire probes; it
cannot establish a native presentation/input window. Mite CLI acceptance and the
independent Win32 process observer are separate evidence. Windows RendererSandbox
is currently unimplemented, so a working window must not enable product claims.

## Decision

Select a small Windows.cmake adapter for the curated Qt view. Require the actual
HAS_DIRECTX SDK backend and compile upstream's unchanged WebContentViewWindows.cpp
with d3d11. Reuse the same owned Qt view/window probe as macOS/Linux. Apply the
upstream console/DLL deployment helper to the probe; its upstream ladybird build
dependency deploys the native Qt platform plugins into the same development SDK.
No copied browser UI, alternate engine or offscreen platform substitutes for it.

Permit the diagnostic firefly-window-probe target on native Windows. Keep the
production firefly and owned preload/runtime-helper platform guards unchanged.
The native Windows workflow builds/runs the window gate only after existing DOM,
compositor, realm and cross-process wire gates pass.

## Native acceptance

Retain all existing input/resize/navigation/close and painted PNG assertions.
Require Native Qt platform: windows, so offscreen/minimal backends cannot pass.
The shared probe synthesizes Qt mouse/key events; this establishes that documented
route, not complete SendInput/physical keyboard/window-manager or display coverage.

Port only the Windows process-control branch of the renderer-loss gate. Read its
actual loaded PID through PeekNamedPipe and bounded descriptor reads because
selectors cannot wait on Windows anonymous pipes. No background reader competes
with communicate; silent/closed/oversized startup cannot yield a fabricated PID.
A native Python-pipe component test validates readiness and timeout mechanics,
separately from the actual LibWeb window acceptance.

Independently enumerate native HWNDs, require a visible-style, positive-client-size
window owned by the probe PID and retain its metadata. Visibility style does not
prove physical monitor presentation or absence of occlusion. Verify the renderer
with Toolhelp parent relation, full SDK executable path and creation time. Open
that exact process with terminate/query/liveness rights, recheck its identity on
the held handle, terminate only that owned test renderer and wait for completion.
The window host must report renderer loss and fail. Token/job observations are not
substitutes for the missing Windows renderer sandbox.

## Validation and remaining work

Existing macOS/Linux behavior remains under its original gates. Source formatting,
syntax, unsupported-host guards and local macOS build configuration do not prove
Windows execution. The new native HWND/view/input/capture/crash gate must pass on
hosted Windows. Full production Mite/window integration, sandbox/broker/handle
restrictions, OS services, Designer editing, packaging/signing and release support
remain required. No local desktop UI tests are necessary for this bring-up step.

API references: [PeekNamedPipe](https://learn.microsoft.com/en-us/windows/win32/api/namedpipeapi/nf-namedpipeapi-peeknamedpipe),
[TerminateProcess](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess),
and the pinned Qt DirectX and existing native window-probe sources.


## Native protocol component evidence

The Windows observer/pipe component at 3c8999e passes in
[run 42354246345](https://github.com/jogibear9988/elytra/actions/runs/42354246345),
job 128348719912: native process facts, actual pipe readiness, bounded silent timeout
and one-MiB startup-output refusal all succeed with owned Python peers. This proves
the observation/protocol component only. It does not execute the Qt window, HWND
assertions, owned WebContent termination or product sandbox. Their full native
renderer/window gate remains pending.
