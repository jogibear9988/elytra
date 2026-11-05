# 0024: Run Firefly app/BrowserWindow on the actual Mite VM

Status: first native macOS ARM64 application slice and shared Electron migration
fixture pass. Visible screenshot revalidation is blocked by the locked desktop;
Windows/Linux and the remainder of the framework remain required.

## Source and API reference

Use Electron's documented [app lifecycle](https://www.electronjs.org/docs/latest/api/app),
[BrowserWindow](https://www.electronjs.org/docs/latest/api/browser-window) and
[webContents](https://www.electronjs.org/docs/latest/api/web-contents) as the public
contract. The implemented subset uses familiar names and Promise/event patterns.
The shared fixture checks observed behavior against the native Electron 45.2.3
binary; it does not infer parity from these names or documentation alone.

## Integration and ownership

Build a separate firefly executable with our shared Mite runtime, the upstream
LibJS target appropriate for LibWeb, and the existing Qt view/event-loop adapter.
Do not link LibJSStandalone as a second engine into this process. The reusable
Qt sources now form one static library shared by the window probe and actual
application. Sources still compile directly from the unmodified upstream pin.

A separate generated dependency manifest adds libuv and pinned llhttp to the
browser manifest and resolves overlay paths against the original checkout. It
lives in the build directory, requires matching browser/Mite baselines and rejects
conflicting overrides. No upstream manifest or source is edited. Configure/build checks compare the
generated manifest with current pinned inputs and reject stale or modified data. Electron and its
lockfile live solely in a behavioral-reference tool installation under build/;
they are not linked into or used to run Firefly.

Our WebView::Application subclass owns QApplication and Core's Qt event loop.
Mite::run owns the main VM/realm, builtins, loader, libuv loop and entry execution.
Host initialization obtains the same Mite events/path/url builtins, evaluates the
Firefly JS API factory and registers its real exports as the firefly host module.
Mite itself contains no Firefly names or GUI dependencies. The application's entry
directory supplies relative loadFile resolution; the CLI currently takes a script,
not a packaged Electron application directory.

The JS facade owns EventEmitter lifecycle and window wrappers. The native bridge
owns widgets, IDs and view callbacks, retaining a rooted main-realm event handler.
Native callbacks are queued onto its QObject context and dispatched through Mite's
exception/checkpoint boundary. Window destruction disconnects ownership before
closed is delivered; shutdown destroys all windows, cancels queued callbacks and
releases roots while the realm still exists. No main-realm objects or native host
functions are installed in WebContent.

The initial GUI driver polls pump_once with a one-millisecond precise Qt timer.
This is a working development integration, not the final idle-efficient notifier
backend. pump_once guards nested entry, dispatch marks the active host turn, and
the bridge defers external callbacks while it is busy. This matters when Qt/Core
synchronous IPC pumps a nested GUI loop: it must not recursively run libuv or
execute pending ticks/promises in the middle of another JS callback.

The desktop host explicitly requests termination of pending native work when it
stops. After host shutdown, Mite uses _Exit with the selected status if native work
is still pending. This permits a normal app.quit even with a blocked filesystem
worker; standalone CLI behavior remains unchanged. This forced path skips later
C++ teardown (including temporary-profile cleanup), so cleanup and graceful
cancellation still need improvement before distribution.

## Loading and security limits

The bridge uses the actual WebContentView and its existing process launcher,
transport and IOSurface presentation. File existence is checked before loading;
matching navigation request failures reject the main Promise rather than resolving
an error page. Initial about:blank completion is not accepted as the requested
load. New load calls abort the prior JS pending Promise. Complete navigation-ID,
redirect, fragment and supersession parity still requires implementation/tests.
Renderer process death rejects pending loading and emits render-process-gone.

Keep temporary profiles, no browser-process coordination and default helper
sandbox flags. Accept only the initial window options and secure default
webPreferences; explicitly reject preload or privileged/nonisolated alternatives.
There is no preload/contextBridge/ipcMain/ipcRenderer API yet. The Node/Mite global
absence checks are selected tests, not proof of the full sandbox or malicious IPC
surface. Native Windows sandbox work from ADR 0020 remains mandatory.

## Verification

The shared CJS fixture runs unchanged except its selected module name. It checks
asynchronous ready, rejecting window creation before readiness, registry identity,
main fs access, a real isolated renderer, load completion before Promise resolution,
size/title/show/hide, cancellation of the first close and ordered closed/quit events.
Cache webContents before closed: Electron no longer permits retrieving it from the
destroyed BrowserWindow. The reference fixture was corrected accordingly rather
than weakening the shared lifecycle assertions.

A separate static-ESM Firefly entry waits for app readiness, starts the actual Mite
HTTP server and loads its HTML through the renderer's RequestServer. Failure tests
cover missing files, refused connections, unsupported/security options, an actual
owned WebContent OS process killed by the harness, fatal main timer callbacks and
normal quit while a real FIFO worker is blocked. The original 27 Mite suites also
pass, including a new nested-pump ordering check in the embedding suite. Inspect
standalone and Firefly linkage and retain upstream integrity/Skia confinement gates.

The old headless renderer gate still passes. The native window gate reaches its
capture stage but fails to obtain an image in the current locked macOS session:
IOConsoleLocked and CGSSessionScreenIsLocked are true; WindowServer lists our
window but not as onscreen. Do not change that gate to pass without an image.
Re-run and visually review the example/window capture after unlocking. The prior
native presentation proof remains historical evidence, not a new screenshot pass.
