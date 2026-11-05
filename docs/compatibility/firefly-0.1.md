# Firefly 0.1 development compatibility

The first Mite-driven native macOS ARM64 Firefly slice is implemented. Shared
Electron 45.2.3 migration fixtures pass in both runtimes with the module name
selected at import time. This does not establish general Electron compatibility.
Windows/Linux, preload/IPC isolation and distribution remain outstanding.

| Surface | Status | Evidence / remaining acceptance gate |
| --- | --- | --- |
| `app`, lifecycle | partial | isReady/whenReady, ready, window-all-closed, before-quit, will-quit, quit, cancellation/retry and app.exit signed codes tested; nested quit is coalesced as an intentional difference; OS activation/dock/Cmd-Q and beforeunload remain incomplete |
| `BrowserWindow` | partial | Native macOS Qt/IOSurface view; width/height/title/show, show/hide/focus, size/title controls, registry, cancellable close and destroy; most options, beforeunload and native OS event parity remain incomplete |
| `webContents`, loadFile/loadURL | partial | Shared object, URL/title/PID access, did-finish-load and load promise; local files and an actual Mite HTTP server tested; file/connect errors and renderer-crash event tested; redirects, fragment navigation, overlapping loads and full error shapes need work |
| Renderer capability defaults | partial | No process/require/Buffer/Mite globals, node:fs import denied in shared HTML fixture; this is not a full sandbox/IPC security audit |
| `ipcMain`, `ipcRenderer` | unsupported | Validated, bounded transport and malicious-input tests |
| preload, `contextBridge` | unsupported | Isolated preload realm and constrained main/renderer bridge; preload is explicitly rejected |
| dialog, Menu, MenuItem, Tray | unsupported | Native services and observable Electron fixture behavior |
| clipboard, shell, screen, nativeImage, session | unsupported | Native services and migration fixtures |
| Chromium/V8 APIs, Chrome extensions | unsupported | Outside the initial scope |
| Packaging on Windows/macOS/Linux | unsupported | Native installation/signing/launch smoke tests; current helpers/resources come from the development Ladybird bundle |

Use `require('firefly')` or ESM imports from `firefly` in the main process. There
is no `electron` alias. Standalone Mite does not register or depend on Firefly.
Only width, height, title, show and the secure default webPreferences settings are
accepted initially. Unknown options, preload, nodeIntegration=true,
contextIsolation=false or sandbox=false fail explicitly. Dimensions are restricted
to integer values in 1..16384. Extra load options are not implemented. The
contextIsolation setting does not imply an implemented preload isolated world.

Named ESM exports are fixed snapshots of app and BrowserWindow; CJS and ESM share
the same objects. Relative loadFile paths use the application entry directory.
No BrowserWindow creation is allowed before app is ready. The GUI driver currently
polls Mite's nonblocking libuv pump every millisecond; idle-efficient backend
notifier integration is still required. Main callbacks enter the Mite checkpoint
boundary, and reentrant GUI pumps are deferred.

Native screenshot/input proof from the preceding window gate remains recorded in
PROGRESS.md. In the latest combined build, the screenshot gate could not complete
because macOS reports the desktop locked. Functional Firefly, Electron reference,
ESM/HTTP and headless tests pass, but current visible presentation must be rechecked
after unlocking. No replacement image or fake screenshot pass is used.

app.exit(code) accepts signed 32-bit integers, bypasses quit/close cancellation and
emits closed before quit with the requested code. The explicit code takes
precedence over process.exitCode. Like the tested Electron binary, it returns to
the current JS call; later async callback timing is not claimed compatible.
Nested app.quit calls within a running quit attempt are coalesced, preventing
recursive duplicate events. Electron 45.2.3 repeats before-quit/close in the bounded
reference fixture; this difference is deliberate and tested separately (ADR 0025).

An internal synchronous isolated-world adapter now passes native LibWeb tests for
separate globals/intrinsics/wrapper expandos, shared DOM and GC/detach lifetime.
A separate macOS prototype helper also proves before-first-script ordering in
top-level/srcdoc documents and cleanup through 70 frame cycles, with the original
helper as a negative control. It runs a harmless compiled marker, not user preload,
and is not selected by the public app. Async revocation, contextBridge and IPC
remain unimplemented; those public APIs remain unsupported (ADRs 0026, 0027).

The internal helper now also verifies frozen, data-only cross-world snapshots
before first author scripts. Native tests cover cycles, sparse arrays, aliases,
GC, prototype safety, trap-free rejection and snapshot size limits (ADR 0028).
This does not enable the public bridge: function/Promise forwarding, document
revocation, IPC validation and hard enumeration scratch-memory bounds remain open.

Internal synchronous function endpoints now round-trip copied data with fixed
error sanitation, native reentry rejection and explicit/document-inactivity
revocation. SDK and actual helper tests cover retained functions after frame
removal, including 70 cycles (ADR 0029). Public preload/contextBridge/IPC remain
unsupported: async Promise/callback lifetime and validated transport are still open.

Internal function results may now forward actual source Promises to separate
target Promises with copied/frozen fulfillment and fixed rejection errors. SDK and
actual helper gates cover pending limits, frame-removal rejection and ignored late
results (ADR 0030). This revokes forwarding only; source job cancellation, callback
arguments and transport validation remain open. Public APIs remain unsupported.

The internal helper now also blocks registered-world Promise jobs after detach or
document removal, including tested await/async-generator and bound/proxy paths.
SDK controls and actual helper PID/DOM checks pass (ADR 0031). Timers, WebIDL/DOM
callbacks, queueMicrotask and other async boundaries remain open; public
preload/contextBridge/IPC remain unsupported.

Guarded Promise jobs now retain native ownership through in-flight detach/last
external release. SDK and actual helper reaction/await/generator tests pass,
including self-removing frames and independent target Promise settlement (ADR 0032).
This does not establish timer/DOM/WebIDL callback safety or enable public preload.

Source Promise reasons/Promises are now excluded from principal page rejection
events, with a reproduced pre-filter leak and passing SDK/helper controls (ADR 0033).
Foreign Promise constructors and later handling are covered without invoking source
reason getters. This is an internal boundary fix; public preload remains rejected
and other callback/error routes still require work.
