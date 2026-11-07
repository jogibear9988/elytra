# Firefly 0.1 development compatibility

Historical internal-boundary notes below retain the status at their implementation
time; the table and current API reference describe public availability.

The first Mite-driven native macOS ARM64 Firefly slice is implemented. Shared
Electron 45.2.3 migration fixtures pass in both runtimes with the module name
selected at import time. This does not establish general Electron compatibility.
Broader preload/contextBridge/IPC compatibility, Windows desktop and complete
Linux distribution remain outstanding. [Current support](../status.md) records
exact native source/run scope, example results and unaccepted new integrations.

The experimental [Linux renderer/helper path](../development/linux-renderer.md)
passes its native renderer/helper/window and relocated public application gates
at `19ad113` ([run 42316853517](https://github.com/jogibear9988/elytra/actions/runs/42316853517)).
This scoped Ubuntu 24.04 x64 Xvfb/Openbox acceptance includes HTTP/LibWeb,
preload/IPC, window sizing/centering, clipboard text and live sandbox activation;
it does not establish complete containment, screen/theme services or packaging.

| Surface | Status | Evidence / remaining acceptance gate |
| --- | --- | --- |
| original-fs | partial | Separate raw main-process FS/promises facade, physical bytes, cross-facade descriptors and patch isolation pass in the local macOS Firefly starter and Electron console reference. Host GC/standalone resolution checks also pass natively on Linux/Windows at 678c85e; Linux product execution pending, Windows desktop unavailable. ASAR virtual paths absent. [Contract](../api/original-fs.md) |
| `app`, lifecycle | partial | isReady/whenReady, ready, window-all-closed, before-quit, will-quit, quit, cancellation/retry and app.exit signed codes tested; nested quit is coalesced as an intentional difference; OS activation/dock/Cmd-Q and beforeunload remain incomplete |
| `BrowserWindow` | partial | Native macOS Qt/IOSurface view; scoped Linux X11 runtime geometry/app fixture passes at 19ad113; width/height/title/show/frame/x/y/useContentSize, native outer/client bounds/size/position, AppKit centering, show/hide/focus, minimize/maximize/restore, registry, cancellable close and destroy; frame/constraint/mixed-DPI parity, most options and beforeunload remain incomplete. [Geometry contract](../api/browser-window.md#outer-and-content-geometry) |
| Custom window chrome | partial | frame:false, top-document app-region/-webkit-app-region drag/no-drag CSS adapter; native drag/exclusion/double-click and actual LibWeb style/layout tests pass. Stylesheet/geometry/polling limits in ADR 0055; inline/shadow/frame/CSP/zoom/native resize parity remain incomplete |
| `webContents`, loadFile/loadURL | partial | Shared object, URL/title/PID access, did-finish-load and load promise; local files and an actual Mite HTTP server tested; file/connect errors and renderer-crash event tested; redirects, fragment navigation, overlapping loads and full error shapes need work |
| Renderer browser modules | partial | External local static/dynamic ESM, live bindings/cache, Unicode paths, navigation, MIME/CSP and absent native imports tested against actual Electron. HTTP modules have Designer evidence. [Scope and file policy](../guides/renderer-modules.md); workers/full module types remain unverified |
| Renderer capability defaults | partial | No process/require/Buffer/Mite globals, node:fs import denied in shared HTML fixture; this is not a full sandbox/IPC security audit |
| `ipcMain`, `ipcRenderer` | partial | Native invoke/handle/handleOnce/removeHandler, early preload requests, mutable plain-data graphs, sender ownership, adversarial input, pending limits and navigation/closure tests pass; one-way/sync/port/subframe IPC and full clone types remain unsupported. [Contract](../api/ipc.md) |
| preload, `contextBridge` | partial | Bounded top-level CommonJS preloads and frozen data/function APIs; first-script, navigation, process replacement and failure controls pass. [Contract and differences](../api/context-bridge.md); fixed invoke actions can call main-process handlers |
| dialog, Tray | unsupported | Native services and observable Electron fixture behavior |
| Menu, MenuItem | partial | Native application menus/checkboxes/properties/callbacks pass local macOS Qt/LibJS and public-host gates. Popup, accelerators and broader roles remain unavailable; fresh Linux acceptance required. [Contract](../api/menu.md) |
| `screen` | partial | Native macOS display/work-area/scale/rotation/label/cursor snapshots pass a windowless OS probe; real API geometry/readiness/event policy passes in Node/Mite. Queued native notifications and the public Electron/Firefly macOS desktop fixture pass at 19ad113; physical hotplug/mixed-DPI acceptance pending. Linux X11 provider passes the independent native gate at 3936884; public query/error/notification wiring is implemented with its actual app gate pending; Windows display services remain absent. Conversions and further Display fields unsupported. [Contract](../api/screen.md) |
| `clipboard`, `ClipboardItem` | partial | Electron 45 async text/MIME-item API; private native macOS pasteboard probe and actual Node/Mite factory tests pass for text/HTML/RTF, UTF-16 conversion, snapshots, promises and failed preparation. Public macOS Electron/Firefly fixture passes at 19ad113; isolated X11 protocol peers and public Linux text integration also pass. Images/files/custom formats, multiple-item writes, broader interop/races, Wayland and Windows remain unfinished. [Contract](../api/clipboard.md) |
| shell, nativeImage, session | unsupported | Native services and migration fixtures |
| `session` | partial | defaultSession.protocol is the main singleton after readiness; actual file-route loading passes locally. Custom partitions and other session methods remain unsupported. [Contract](../api/session.md) |
| app.commandLine | partial | Actual launch-switch observation, before-ready native lang and startup-only compositor CPU painting pass local macOS, including shared Electron read observations. Chromium/Blink/V8 switches remain unsupported. [Contract](../api/command-line.md) |
| contentTracing | partial | Real native main event durations, bounded filters/buffers and async trace-event JSON output/retry pass local macOS. Renderer/helper/sampling/heap/Perfetto protobuf support remain unfinished; fresh Linux acceptance required. [Contract](../api/content-tracing.md) |
| `crashReporter` | partial | Real local main/owned-helper native crash capture and next-launch recovery pass on macOS, with frozen annotations and configured crashDumps. Uploads, dynamic extras, report lists and Windows capture remain unsupported. [Contract](../api/crash-reporter.md) |
| `protocol` | partial | registerSchemesAsPrivileged publishes the immutable native standard/secure origin table before readiness/helper launch. Atomic refusals and real main ESM/CommonJS exports pass locally on macOS. Default-session registerFileProtocol/unregisterProtocol/isProtocolHandled and actual custom-window ESM/CSS/file loading now pass locally, including asynchronous callbacks and revocation. General handlers/partitions/additional privileges remain unsupported. [Contract](../api/protocol.md) |
| `nativeTheme` | partial | Actual macOS dark/accessibility snapshots, application source selection and queued updates; native notification teardown and Node/Mite policy pass. Targeted Firefly/Electron CSS queries pass for existing/new hidden windows. Physical changes, visible native chrome, hidden media-event timing and Windows/Linux need acceptance. [Contract](../api/native-theme.md) |
| Chromium/V8 APIs, Chrome extensions | unsupported | Outside the initial scope |
| Packaging on Windows/macOS/Linux | partial | Experimental macOS ARM64 bundles and published Alpha 2; later optimized artifact. Ubuntu 24.04 x64 X11 stripped archive/ELF provenance and extracted bundled Designer edit/Monaco/HTML controls pass at 197cd2a ([run](https://github.com/jogibear9988/elytra/actions/runs/42379262065)); Actions candidate uploaded. Windows packages, broader Linux distribution, Developer-ID/notarization, installers and older-macOS validation remain outstanding |

Use `require('firefly')` or ESM imports from `firefly` in the main process. The
explicit [--electron-compat option](../api/firefly-cli.md) aliases the actual
main/preload subsets; default resolution and unsupported exports stay unchanged.
Standalone Mite does not register or depend on Firefly.
Only width, height, x/y, useContentSize, title, show, frame, absolute CommonJS preload and the secure
default webPreferences settings are accepted. Unknown options, nodeIntegration=true,
contextIsolation=false or sandbox=false fail explicitly. Dimensions are restricted
to integer values in 1..16384. Extra load options are not implemented. The
contextIsolation setting requires the implemented isolated preload world; it cannot
be disabled. Native ES module preloads remain unsupported; bundle authored ESM
into the supported payload. IPC currently supports request/reply only.

Named ESM exports are fixed snapshots of app, BrowserWindow and ipcMain; CJS and ESM share
the same objects. Relative loadFile paths use the application entry directory.
No BrowserWindow creation is allowed before app is ready. The GUI driver currently
polls Mite's nonblocking libuv pump every millisecond; idle-efficient backend
notifier integration is still required. Main callbacks enter the Mite checkpoint
boundary, and reentrant GUI pumps are deferred.

The completed macOS ARM64 package gate at 8dc5830
([run 42348529659](https://github.com/jogibear9988/elytra/actions/runs/42348529659))
passes native desktop/reference tests, all six shared example smoke slices and
the packaged Designer's edit/Monaco/HTML window-control checks. Earlier local
desktop-lock failures remain historical observations in PROGRESS.md; they do not
describe this completed hosted run. This is scoped macOS evidence, not Windows
desktop or complete Linux package acceptance.

app.exit(code) accepts signed 32-bit integers, bypasses quit/close cancellation and
emits closed before quit with the requested code. The explicit code takes
precedence over process.exitCode. Like the tested Electron binary, it returns to
the current JS call; later async callback timing is not claimed compatible.
Nested app.quit calls within a running quit attempt are coalesced, preventing
recursive duplicate events. Electron 45.2.3 repeats before-quit/close in the bounded
reference fixture; this difference is deliberate and tested separately (ADR 0025).

## Historical implementation stages

The following notes preserve the sequence of internal bridge development. Statements
that public preload/contextBridge/IPC are unavailable describe those earlier stages;
the current partial public contracts are in the table above and the
[API reference](../api/index.md). Do not use these historical stages as the current
support inventory.

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

An owned compile adapter now gates non-Promise Report-style WebIDL callbacks.
queueMicrotask ordering, cancellation, in-flight owner lifetime and error isolation
pass SDK/helper tests; ordinary renderer and Firefly/Electron regression gates
also pass (ADR 0034). Other callback conversions and timer/listener resource cleanup
remain incomplete. No public preload/contextBridge/IPC support is implied.

Internal source timeouts/intervals now release their specific native timer/map
entries at physical world detach. SDK native-state/GC tests and a helper test with
a continuing page interval pass (ADR 0035). This is not public preload/timer parity;
string-handler realm policy and other native resources remain incomplete.

Internal MutationObserver delivery now reconstructs record arrays and native DOM
wrappers in the source world, preventing page prototype influence. Specific native
registrations/records are disconnected at physical detach; SDK and actual helper
page-observer controls pass (ADR 0036). Other observers/listeners and callback
conversions remain incomplete; public preload/contextBridge/IPC stay unsupported.

Internal function/object event listener dispatch now checks source lifetime before
native Event/receiver wrapping. Getter/proxy/last-owner detach and source error
privacy have SDK/helper coverage (ADR 0037). Page listeners keep working. Listener
registration removal, handler attributes and script-valued event fields remain
open; this does not enable public preload or a general callback membrane.

Internal source listener registrations and exact signal abort algorithms now release
at physical world detach, including duplicate registration and callbacks registering
after logical detach. Native-state/GC and helper page-listener/signal controls pass
(ADR 0038). Handler attributes, other observers and script-valued event fields remain
open; public preload/contextBridge/IPC remain unsupported.

Internal native on… handler dispatch now checks source lifetime before wrapping,
redacts errors and converts beforeunload results within the retained source frame.
SDK/helper and app regressions pass (ADR 0039). Shared handler maps/raw callback
getters and handler cleanup remain unresolved; direct calls can bypass native
dispatch. Public preload/contextBridge/IPC stay unsupported.

Registered internal worlds now have private native on… handler maps. Their getters
return their own callback/null; page and other-world handlers keep separate identity
and delivery. SDK/helper GC, ordering, inline attribute, erasure/rearm, foreign-
callback and independent detach controls pass (ADR 0040). Other event fields,
observers and callback boundaries remain incomplete; public preload/contextBridge/
IPC remain unsupported.

Internal CustomEvent.detail now keeps same-world identity and uses bounded frozen
snapshots across registered world boundaries, with stable per-event/reader cache
identity. This is an **intentional difference** from raw cross-world mutation
sharing. SDK/helper provenance/trap/GC/reset/revocation controls and app regressions
pass (ADR 0041). Original native payload retention, other event fields and the full
membrane remain incomplete; public preload/contextBridge/IPC stay unsupported.

Internal ErrorEvent.error and window.onerror's fifth argument now share the bounded
frozen snapshot policy across registered worlds (ADR 0042). Same-world identity,
getter/argument cache identity and ordinary native page errors have SDK/helper
coverage. Cross-world Error instances and other non-data values remain unsupported;
rejected owned handler arguments are redacted before invocation. Generic reader
errors have no Source stack frames. Public preload/contextBridge/IPC stay rejected.

Internal MessageEvent.data now uses the same bounded frozen snapshot policy for
registered cross-world reads (ADR 0043). Same-world identity and native page message
deserialization remain unchanged. SDK/helper native postMessage, init/cache/GC and
source detach controls pass. Non-data clone types, real transferred ports and sender
authentication remain incomplete; this does not enable public preload or IPC.

Internal principal listener/handler/report-style callback and event error reporting
entries now hide ambient Source contexts and preserve page Promise/timer ownership
(ADR 0044). SDK/helper stack, job, timer, foreign beforeunload conversion and
last-owner/GC reentry controls pass. Other WebIDL modes, arbitrary native callbacks/
transfers and shared registries remain incomplete. Public preload/contextBridge/IPC
remain unsupported.

Internal NodeIterator/TreeWalker native filter invocation now checks Source lifetime
before wrapping/lookup and retains owners through numeric return conversion
(ADR 0045). Source callback/conversion exceptions intentionally become generic
caller-realm TypeErrors; ordinary page errors stay unchanged. SDK/helper conversion,
last-owner/GC, revocation and page-job controls pass. Cross-world .filter getter
access remains unsafe/unimplemented; public preload/contextBridge/IPC stay rejected.

Internal NodeIterator/TreeWalker .filter getters now preserve own-world identity and
reject cross-world callback references with reader-realm TypeErrors (ADR 0046).
Constructor provenance, nested guard handling and sticky revocation prevent raw
callback release after Source detach/writer GC; getter/proxy traps are not called.
SDK/helper native traversal, identity/null, callback-kind and GC controls pass.
Unknown provenance/unresolved callable realms reject explicitly. Native container
retention and other API boundaries remain incomplete; public preload/contextBridge/
IPC stay unsupported.

The native publication engine now composes nested data/function APIs atomically
(ADR 0067). It freezes copied data, preserves cycles/function aliases, rejects
occupied names and unsupported/trapping values without partial publication, and
revokes exported functions/pending promises on source detach. SDK and actual
sandboxed helper tests cover principal reentry and removed-frame lifecycle. This
is an internal integration step: public preload selection, production helper
staging, navigation policy and main/renderer IPC still need implementation.

Public CommonJS preload selection and a separate production renderer helper now
compose those boundaries (ADR 0068). Application and packaged tests cover native
window identity, first-script installation, navigation/process replacement, restricted
require and fail-closed evaluation. IPC and general callback/DOM parity remain open.
