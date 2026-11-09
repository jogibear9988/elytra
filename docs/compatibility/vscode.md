# VS Code / Code-OSS compatibility target

Status: **not supported**. Requested as a priority compatibility target on
2026-11-07. This is a source-based blocker audit and implementation order, not a
successful launch, complete dependency inventory or percentage-complete estimate.
Running Monaco in Designer does not establish Code-OSS desktop compatibility.

## Current priority: first real workbench, then developer workflows

The latest actual local-desktop/NLS main-ready diagnostic passes GPUProcessMainService
and ProtocolMainService. Public modern Response handlers and the real file-scheme
denial path now pass native/public offscreen acceptance. The unchanged pinned
protocol consumer passes genuine file denial, remote 404/500, binary/MIME responses,
IPC registration and cleanup, with 49 compiled dependencies verified.

With the explicit --mite-policy-watcher build adaptation, the unchanged pinned
NativePolicyService and NativeManagedSettingsService now initialize and dispose
through Mite's [native macOS policy extension](../api/policy-watcher.md). The probe
verifies 46 compiled dependencies and exact package/adapter provenance; it reads a
unique unused domain without preference writes or injected initial snapshots.
Without this opt-in, the ordinary package still requires its Node/N-API addon.

Native local streams now pass the unchanged pinned Code-OSS IPC channel in separate
Mite/Mite, Mite/Node and Node/Mite processes, with binary request/reply, disposal and
41 compiled dependencies verified. Node differential tests and independent native
socket peers pass locally on macOS. The actual adapted main-ready startup now
passes main.claimInstance and both session permission setters in
CodeApplication.configureSession. Genuine native/renderer acceptance now covers
main/same/cross-origin query ownership, real geolocation request-handler denial,
handler replacement/reset, same-origin window isolation and stale pending replies.
A separate exact-object native frontend passes twelve identity/grant/revocation
cases without invoking a device provider. The shared query/request-denial path
passes Electron 45.2.3 as well.

The explicit local desktop mode defers optional display capture and rejects
actual recording, capture authorization and capture-settings attempts. The
11,536-source tree completes entry/app readiness and now passes both the original
webRequest registrations and ErrorTelemetry's process event subscriptions. The
latest real trace reaches CodeApplication.initChannels and fails subscribing to
powerMonitor speed-limit-change. No workbench window is accepted.

The logged ESM require error remains a handled device-ID fallback: @vscode/deviceid
requires its nested ESM-only uuid entry. Error.captureStackTrace is also missing.
[Native error events](../api/process-errors.md) now have genuine event-loop and
entry-point delivery, with local Node/Mite/embedded-host ordering, fatal/default
and GC acceptance. Fresh platform and broader lifecycle support remain open.

The [request policy](../api/web-request.md) passes local macOS native offscreen
HTTP endpoint-zero cancellation, header replacement before CORS, canonical frame
metadata, stable IDs, main net.fetch/net.request and custom-provider cancellation.
The shared hidden Electron fixture passes. Utility transport is temporarily
excluded while filters are active; real parent-mediated utility decisions remain
necessary for Code-OSS. Worker/unbound navigation, other stages and broader browser
coverage are unfinished. The earlier dependency rows retain their checkpoint
context; webRequest is no longer the first missing registration.

Full display capture and complete session-wide request coverage remain open. Media grants, separate worker permission queries, notification request/delivery
and native Linux/Windows permission acceptance are not established. No workbench,
workspace open/edit/save, actual MDM-change or physical GPU acceptance is claimed.

Updated 2026-11-09. Without the new local desktop opt-in, the real prepared entry
passes the simple custom-protocol CORS privilege and stops at allowServiceWorkers. This is not just a missing flag:
Ladybird at Elytra's pinned revision refuses non-HTTP(S) service-worker registration
in ServiceWorkerContainer::start_register; Job.cpp also rejects the unimplemented
Run Service Worker step after fetching a new script. Firefly's current file channel
intentionally refuses worker/frame clients. Both engine execution and secure worker
resource routing would need implementation before advertising this privilege.

The Code-OSS main registration requests allowServiceWorkers specifically for
vscode-webview, codeCache for vscode-webview and vscode-file, and secure/fetch/CORS
on its nonstandard remote resource schemes. These are separate unresolved contracts.
The engine overlay alone removes none of them. Do not treat the current simple
CORS slice as support for all these scheme declarations.

Prioritize these concrete outcomes:

1. **Unblock main startup honestly.** Audit whether the first local workbench can
   explicitly defer webviews/remote resources and engine-specific caches. Any such
   mode must be a separately documented opt-in application adaptation with visible
   limitations, preserved ordinary Electron behavior and refusal of unavailable
   features. The [local desktop experiment](../guides/code-oss-adaptation.md#experimental-local-desktop)
   is now implemented and locally policy-tested. With both build and runtime opt-ins,
   the actual entry import completes and app ready is observed; asynchronous startup
   loads the generated English catalog and passes actual zlib/assert imports, then
   passes readline, MessageChannelMain, utilityProcess, safeStorage, WebContentsView and webContents imports, then reaches
   webRequest.onBeforeRequest after native policy adaptation, real local IPC, document-bound permission registration and explicit optional display-capture deferral. The local real-file tests consume 30,000 lines; the actual OS-release
   macOS branch passes, while native Linux file-branch acceptance remains open.
   Compression, CLI assertions and the actual MessagePort Protocol now pass component
   probes. The real LibJS utility fork/parentPort payload implementation has six native
   boundary tests; local public Firefly/Electron acceptance now passes. Actual Code-OSS utility execution
   remains untested. Native port handoff and authentication forwarding remain
   unsupported; this is not a workbench launch.
2. **Build independent startup prerequisites now.** The pinned mainImpl next uses
   getPreferredSystemLanguages before readiness, getSystemLocale after readiness
   and getLocale for NLS. The [native locale slice](../api/app-locale.md) now passes
   local macOS native/public-host acceptance, including independent regional/UI/
   application preferences. Fresh Linux and full NLS execution remain unverified.
   Continue other demanded main/IPC prerequisites while the worker architecture is assessed.
   Continue the actual entry after each resolved blocker instead of implementing
   unrelated Electron examples.
3. **Connect the real workbench.** Build on the locally tested main-realm
   [MessageChannelMain ownership core](../api/message-channel-main.md). Code-OSS next
   requires native utility/renderer port transfer, including event.ports, beyond the
   new [utility payload channel](../api/utility-process.md). Local Firefly/Electron public lifecycle and payload gates now pass;
   Code-OSS utility execution/authentication forwarding remain unaccepted.
   Main-only pairs and copied payloads cannot supply those endpoint handoffs.
   Preserve native endpoint/process/document identity, revocation and bounded binary
   data, then complete file protocol/interception and demanded window services.
   Require a real editor opening/editing/saving a local file before declaring the
   first workbench milestone accepted.
4. **Complete developer workflows.** Add extension/shared/PTY hosts, watching,
   terminal/Git/tasks and webviews with their actual worker/resource requirements.
   A limited first workbench must keep these missing features explicit. Require
   the same workflow evidence separately for native packaged platforms.

The pinned webview HTML has a disableServiceWorker option, but that alone is not a
resource-loading fallback: it also skips the worker's host-resource messaging path.
No usable webview fallback or working adapted workbench has been established. The
local desktop experiment rejects webview creation instead of claiming a fallback.

Source evidence is the pinned Code-OSS mainImpl.ts, webviewElement.ts,
pre/index.html and pre/service-worker.js, and the pinned Ladybird
Libraries/LibWeb/ServiceWorker/{ServiceWorkerContainer,Job}.cpp. This paragraph is
source inspection; no new service-worker execution test or workbench launch is
claimed. HTTP/2/3, website polish and unrelated example breadth remain lower priority.

## Audited reference

Inspect Microsoft Code-OSS commit
[6531648f5ace8b77ae799262bdc97bf048461938](https://github.com/microsoft/vscode/tree/6531648f5ace8b77ae799262bdc97bf048461938),
whose package manifest identifies development version 1.146.0 and Electron 43.8.6.
Keep this application reference separate from Elytra's Electron 45.2.3 API oracle.
Do not silently move the application baseline when comparing results.

The target is the desktop Code-OSS application running on Firefly/Mite/LibWeb,
including practical editor, filesystem, extension and terminal workflows. A web
editor demonstration, Node-based server or Electron subprocess is not acceptance.
Mite must still embed LibJS without Node/V8, and remain independent of LibWeb.
Any required application/runtime adaptation must be isolated and recorded; no
claim that Microsoft's distributed VS Code binary or every extension runs follows
from a future Code-OSS test.

## Confirmed blockers and next acceptance gates

| Area | Observed upstream dependency | Elytra gap and required proof |
| --- | --- | --- |
| Native environment identity | common/platform and common/process recognize native execution through versions.node | Explicit pinned source overlay also recognizes truthful versions.mite. Actual upstream modules, locale/env/cwd/timer checks and unadapted negative control pass locally on macOS; desktop/Electron detection remains open. [Adaptation guide](../guides/code-oss-adaptation.md) |
| Earliest runtime bootstrap | main imports nodeCompileCache, whose static imports include node:module compile-cache APIs and node:v8 | Explicit --libjs-cache application overlay preserves ordinary upstream startup logic while disabling V8 caches and rejecting generation/measurements/waits. Actual cache branches pass locally; real full-source main.ts reaches mainImpl then standalone-Mite original-fs host-boundary failure. No desktop launch; public Node cache/V8 APIs remain absent. [Policy](../architecture/0106-code-oss-libjs-cache-policy.md) |
| Shared Node bootstrap | bootstrap-node imports createRequire, registers SIGPIPE and sets cwd; exports portable configuration | Actual pinned module evaluates locally on macOS after type erasure in Node and Mite, including SIGPIPE/cwd and portable configuration. Source-mode gate only; exported loader hooks/global-search and packaged ASAR customization remain unverified/unsupported |
| Main process startup | Actual pinned desktop entry and production dependencies | Default engine adaptation still rejects allowServiceWorkers. The explicit local desktop mode with --nls completes the entry import, observes app ready, loads matching English catalogs and passes zlib/assert/readline/MessageChannelMain/utilityProcess/safeStorage/WebContentsView/webContents imports. With the explicit policy package adapter, it passes local IPC and document-bound permission registration, then reaches webRequest.onBeforeRequest after explicit optional display-capture deferral; both unchanged policy consumers have genuine macOS initialization acceptance. Actual component gates pass locally; the real utility fork/parentPort payload source has six native boundary tests with local public Firefly/Electron acceptance passing. Code-OSS utility execution and workbench/open-edit-save remain unaccepted; native utility/renderer port handoff and authentication forwarding are still unsupported |
| Locale/NLS prerequisites | mainImpl reads preferred system languages before ready, then regional/app locale | Native getPreferredSystemLanguages/getLocaleCountryCode and ready-guarded getSystemLocale/getLocale now pass local macOS no-window host/native tests with distinct Foundation/UI/application settings. The explicit local desktop/NLS adaptation now loads its generated English catalog and passes protocol and native policy package resolution with the explicit adapter and passes local IPC and permission registration before webRequest.onBeforeRequest with optional capture explicitly disabled; the default engine-only path still refuses allowServiceWorkers. Translated/full NLS workflow and fresh Linux acceptance remain unverified. [Contract](../api/app-locale.md) |
| Custom resource origins | mainImpl registers vscode-webview/vscode-file and remote resource schemes; protocolMainService serves authorized files and blocks direct file access | Internal actual LibURL/LibWebCommon custom origin/trust foundation passes local native tests; helpers remain empty by default; native parent/owned-helper configuration before bootstrap now passes local headless tests. Rust/native/DOM origin extraction now shares the canonical native policy, with strict local macOS and 1d033fe hosted Linux headless acceptance. Public [origin registration](../api/protocol.md) now passes local main/LibJS gates, and internal production-helper ESM/CSS/file loading passes. Public default-session file callbacks and automatic grants now load real custom windows/resources locally. Modern Response handlers, main net.fetch dispatch and genuine file interception now pass locally; broader parsing, partitions and additional privileges remain required. [ADR 0113](../architecture/0113-shared-rust-origin-policy.md) |
| Preload and messaging | Preload uses send/invoke/on/once/removeListener, webFrame zoom, webUtils and port handoff | Bounded invoke/handle and renderer-to-main send/on events now have local macOS acceptance; main factory policy also passes Node/Mite and a real Electron positive send reference passes. Main-to-preload send/listener methods now also pass local macOS acceptance and the positive Electron reference, with real stalled-peer timeout retirement/reload. The bounded document-callback bridge now passes local native/production-helper tests (data payloads, identity, reentry and Promises); native IPC events now cross with the exact private sender module replaced by an empty object, and individual raw methods stay blocked. The 64 KiB contextBridge binary copy subset now passes local native and production-helper acceptance; the native raw process path now passes local early invoke/binary reply/bidirectional send, view geometry and reload acceptance, with bounded admission and history ordering. Native handle handoff checks epoch/document/PID and rejects unknown-realm/wrong-document/duplicate handoffs. Own trusted source publication getters now pass native and production-helper tests, including the metadata getter shape used here; restricted native sandbox process metadata and additionalArguments now have local helper/reference acceptance, including the argv/config IPC/userEnv merge shape. Need the remaining full preload graph, process events/metrics, native renderer/utility port transfer and file access semantics (a bounded native HTML file-picker now passes local native-view/helper acceptance, but actual native path delivery and File bridge passage remain missing; a weak File/document registry, input hook and one-shot Page/input request guard now pass local native component tests); native webFrame factor/level zoom now has local helper/layout/reference acceptance, with revocation/sender checks |
| Binary channel protocol | Code-OSS MessagePort Protocol sends Uint8Array-backed VSBuffer messages | The [main-realm MessageChannelMain core](../api/message-channel-main.md) now passes real Firefly/Electron start/FIFO/snapshot/binary/ownership-transfer/close tests and native codec/GC bounds. The unchanged Code-OSS Protocol and once helper pass locally on public ports. Both this core and existing native invoke/reply/send preserve up to 64 KiB distinct backing storage within bounded frames. Utility-process/renderer endpoint handoff, larger-file transport and full clone types remain missing; main-only ports do not establish a connected workbench |
| Workspace files | Disk provider channel includes open/read/write/readFile/readFileStream/watch and cancellation | Broaden filesystem/stream/watch APIs and native watcher lifecycle; actual open/edit/save/rename/external-change tests must pass |
| Native services | Manifest includes node-pty, @parcel/watcher, @vscode/sqlite3, native-keymap and other native packages | Root production graph is now pinned and installed without lifecycle scripts on macOS ARM64; 27 packages have native/WASM evidence. Other install roots remain unaudited. No Node addon loader/N-API. Establish each required package/ABI; implement engine-independent bindings or compatible service adapters as required. A Node/V8 host is not an allowed shortcut |
| Native child composition | Optional integrated browser imports WebContentsView and webContents; main workbench uses BrowserWindow | [Native child-view composition](../api/web-contents-view.md) passes shared offscreen Firefly/Electron and the complete hidden production Cocoa fixture, including stable page/process/channel identity, reparenting, viewport/ownership/registry and binary IPC cleanup. The real local desktop/NLS entry now passes these imports and passes the protocol service and, with the explicit native policy adapter, passes local IPC and permission registration and reaches webRequest.onBeforeRequest with optional capture explicitly disabled. Actual integrated BrowserView session/subframe/CDP requirements remain unsupported; on-screen/GPU/AX/performance and native Linux/XCB/Windows acceptance remain unverified |
| OS-backed secret storage | Actual EncryptionMainService uses Buffer JSON plus synchronous encrypt/decrypt | [safeStorage](../api/safe-storage.md) passes native/public isolated macOS keychain tests and the unchanged compiled service write/read across two OS processes with 39 checked dependencies. Real Electron reference covers pre-ready behavior only. App-profile OS-held AES-256-GCM keys and Firefly ciphertext; Electron migration, async API and Linux/Windows providers remain unavailable. Component readiness is not production/workbench acceptance |
| Auxiliary/extension processes | Bootstrap names extension-host, shared-process, pty-host and agent-host roles | The [real LibJS utility fork/parentPort payload slice](../api/utility-process.md) has six native boundary tests; local public Firefly/Electron lifecycle, binary messaging and parent-teardown acceptance now passes. This does not accept actual Code-OSS utility execution. Native transferred endpoints, authentication forwarding, nonempty engine flags and unsigned-library loading remain explicitly unsupported. Then accept extension-host runtime, shutdown/crash recovery and declared extension limits; spawning an executable alone is insufficient |
| Renderer and webviews | Complete workbench and extension UI require actual browser behavior | Monaco proof is only a subset. Run the real workbench; verify DOM/CSS/input/clipboard/workers/webviews, then required WebAssembly/storage/security features. Unmeasured features remain unverified |
| Platforms/distribution | A usable desktop app requires the native host and packaged helpers | Windows desktop/sandbox/package and complete Linux packages remain open. Keep macOS/Linux/Windows acceptance separate |

Primary source evidence at the pinned commit:
[bootstrap](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/main.ts),
[compile cache](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/vs/base/node/nodeCompileCache.ts),
[main implementation](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/mainImpl.ts),
[preload](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/vs/base/parts/sandbox/electron-browser/preload.ts),
[port acquisition](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/vs/base/parts/ipc/electron-browser/ipc.mp.ts),
[binary port protocol](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/vs/base/parts/ipc/common/ipc.mp.ts),
[file provider client](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/vs/platform/files/common/diskFileSystemProviderClient.ts),
[file provider server](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/vs/platform/files/node/diskFileSystemProviderServer.ts),
and [dependency manifest](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/package.json).

A prior targeted local non-GUI probe with the then-existing standalone Mite executable
reports explicit missing built-ins for node:module/node:v8/node:perf_hooks and a
missing original-fs package. That confirms those import blockers, not a launch of
Code-OSS or a current-master full runtime validation.

The new [node:module utility slice](../api/module.md) implements createRequire over
the existing native loader, with shared CJS/JSON instances, real package require
conditions and actual GC acceptance locally on macOS. This targets the pinned
[bootstrap-node createRequire call](https://github.com/microsoft/vscode/blob/6531648f5ace8b77ae799262bdc97bf048461938/src/bootstrap-node.ts#L7).
It does not implement the separate compile-cache imports, node:v8 or execute the
whole desktop bootstrap. The explicit application cache adaptation below addresses
its optional engine imports without changing Mite's public API inventory.

The newer non-GUI **mite-code-oss-bootstrap** gate executes the actual pinned
bootstrap-node.ts after type erasure, using VSCODE_DEV=1. It proves module
evaluation, its SIGPIPE registration, original cwd capture and portable-mode
existing/missing directory/temp configuration locally on macOS. It leaves all
source algorithms intact. It does not call the exported developer loader-hook or
global-search customization, and does not activate packaged ASAR support. Those
paths, transitive mainImpl and desktop launch remain open. Node is
only the development oracle/type stripper; Mite still executes with LibJS alone.

The [performance clock slice](../api/perf-hooks.md) supplies native global/module
now/timeOrigin. Its gate runs the actual pinned common/performance.ts after
development-only type erasure in both Node and Mite: origin-backed startup marks,
explicit timestamps and clearing pass using Code-OSS's own fallback. The unmodified
source/hash manifest and MIT license live in vendor/code-oss-tests. This is one
real upstream module, not mainImpl or the desktop bootstrap. Full performance
timeline/profiling APIs remain open.

The [original-fs slice](../api/original-fs.md) passes real raw file IO, descriptor
interoperability and public-fs patch isolation in the native macOS Firefly starter
and Electron console reference. It registers only in the trusted main process;
standalone Mite keeps ordinary npm resolution. This removes that import dependency
for unpacked trees, not ASAR support or the remaining Code-OSS startup blockers.

The first [internal binary snapshot/wire slice](../architecture/0099-bounded-native-binary-wire.md)
passes local macOS native/separate-process tests. It now preserves typed-array/
DataView offsets, repeated-view identity and shared backing with raw attachments.
Later native document-channel and public binary gates now prove bounded Buffer/Uint8Array
invoke/reply/send locally on macOS. MessagePorts, larger-file streaming and fresh
Linux/Windows execution remain required.

The [native byte channel](../architecture/0100-native-byte-channel.md) also passes
its local macOS component/owned-child gate. This supplies a real byte transport,
not yet a document-bound Code-OSS or public Firefly IPC path.

The next [trusted endpoint handoff](../architecture/0101-trusted-byte-endpoint-handoff.md)
adds a real headless WebContent integration gate: native handle delivery, PID/cookie
correlation, raw byte echo and default/invalid/duplicate denial controls. The later document-bound handoff and raw application channel now install the actual
product helper receiver with epoch/document/PID validation and revocation. Public
binary/event IPC has local acceptance; port handoff remains required for Code-OSS.

## Implementation order

An [audited source overlay](../architecture/0105-code-oss-native-identity.md) now
handles the first native identity checks without fabricating Node metadata.
This preserves actual native environment/cwd instead of Code-OSS's web fallback;
it does not supply desktop identity/API behavior. The separate
[LibJS cache policy](../architecture/0106-code-oss-libjs-cache-policy.md) now removes
the optional cache's static import barrier. Actual full-source main.ts records
cache initialization and reaches mainImpl under a nonzero standalone-Mite probe.
The real Firefly host now supplies raw FS and an explicit import strategy through
[--electron-compat](../api/firefly-cli.md). With the actual [root production dependencies](../guides/code-oss-adaptation.md#prepare-actual-root-production-dependencies),
its diagnostic passes the registerHooks import and advances past util.promisify to unsupported Chromium enable-features without the engine overlay, or unsupported protocol privilege allowServiceWorkers with explicit --libjs-engine (exit 1).
The [resolve-hook API](../api/module.md#registerhooksoptions) has real generic tests;
Code-OSS’ Electron-only ASAR path remains untested/unimplemented. The [promisify API](../api/util.md) is now implemented and tested with real callback services.
The simple custom-protocol CORS subset now has actual native and public-renderer
acceptance; the prepared real entry passes corsEnabled and fails at allowServiceWorkers.
Implement required worker/handler behavior next, then follow each real startup failure.
Broader exports/options, workbench startup and other dependency roots remain unimplemented. No fake Electron objects or mainImpl replacements are used.

Choose general compatibility work against this application path first. Keep
security and the full native platform specification; deprioritize unrelated
Electron example breadth and new HTTP protocols.

The next internal [file-body component](../../firefly/native/protocol/files/README.md)
now passes real local macOS checked-file reads and 4 MiB streaming to a separate
native child, including HEAD/cancellation and file mutation errors. Versioned
response metadata now carries validated MIME/length and ordered HTTP headers to
the native consumer before the body. Native request ownership now adds bounded
jobs, expiry/cancellation and epoch/ID/offset frames, with real separate-child and
concurrent-budget tests. Renderer request admission/binding, response application
and general handler dispatch remain partial; this
does not yet load Code-OSS resources or resolve its protocol import.

The [LibWeb response adapter](../architecture/0116-libweb-protocol-responses.md)
now passes a local non-GUI real-file-to-ReadableStream gate with bounded credit,
GC and actual FetchController/reader cancellation. The
[native scheme-fetch seam](../architecture/0117-native-scheme-fetch-dispatch.md)
now passes actual JavaScript fetch/arrayBuffer, HEAD and AbortController over
real files in a same-process native fixture. Live helper/document binding and
the later public default-session file callbacks now cover a subset; broader handler/session APIs remain open.
This does not yet load workbench resources or establish general fetch compatibility.

The [native resource service](../architecture/0118-native-resource-request-service.md)
now transfers URL requests, consumer credit, cancellation and real file responses
across actual native parent/child processes, with local macOS acceptance. The main
side alone selects authorized paths. The next
[document-bound client](../architecture/0119-document-fetch-channel-client.md)
connects it to the real LibWeb response owner and global JavaScript fetch in a
separate native child, with local macOS acceptance. Exact document identity,
parallel quotas, real abort/reader cancellation and silent-peer expiry are checked.
The internal live helper/document routing is covered below; the public
broader protocol/session contract remains open. Code-OSS resources still do not load through
the product.

Inspection of the real navigation path identifies another prerequisite for the
first workbench URL: the [shared fetch classifier](../architecture/0120-custom-fetch-scheme-classification.md)
now routes registered standard schemes internally in engine/native UI checks.
The next [one-shot navigation scope](../architecture/0121-native-navigation-population.md)
now passes actual upstream population through the native resource channel, with
returned body/origin/CSP and refusal/cancellation checks. Navigation is still refused
without its explicit native handler and intent. Initial descriptor buffering is
limited to 16 MiB. The [native live helper path](../architecture/0122-live-helper-resource-navigation.md)
now loads and activates a real app-origin DOM through the production helper in a
local headless probe, with exact endpoint/navigation admission and absent page
privileges. The [exact-document handoff](../architecture/0123-live-document-resource-handoff.md)
now also loads external ESM, CSS and binary GET/HEAD in the real helper; wrong-ID,
missing/duplicate grants and revocation on navigation pass locally. The normal main
bridge now exposes [default-session public file callbacks](../api/protocol.md) and
automatic native grants, accepted with actual app HTML/ESM/CSS/binary loading locally.
Modern Response handlers and genuine file interception now also pass the public
round-trip gate. Broader session support and actual workbench startup remain open.

The pinned ProtocolMainService uses defaultSession.protocol.registerFileProtocol for
its authorized workbench files. The [native async authorization service](../architecture/0125-asynchronous-file-authorization.md)
now accepts delayed/out-of-order completions with quota/deadline/cancel ownership;
actual helper ESM/CSS/file loading passes with deferred native authorization. Default-session Main-VM
callback delivery and automatic grants now pass actual public-window tests. Modern
Response/file handlers and the unchanged protocol consumer now pass too. Required
privileges, native policy integration and the remaining Code-OSS services stay open.

Immediate execution order (each step needs real native acceptance):

| Priority | Deliverable | Exit criterion |
| --- | --- | --- |
| 1 | Main continuation, utility forwarding and complete session policy | The real adapted entry now passes both permission setters and unchanged webRequest registrations. Real document/main HTTP/custom request enforcement passes locally; worker/unbound navigation and WebSocket scopes are deliberately limited, and active filters temporarily exclude utility processes. Native error/rejection delivery now passes real Node/Mite/host tests; startup passes ErrorTelemetry and next fails at powerMonitor speed-limit-change during initChannels. Implement a genuine native event source, plus remaining runtime gaps and real utility parent forwarding, then validate a workbench/open-edit-save run. Display capture and fresh platform permission/interception acceptance remain open. |
| 2 | Utility-process and native port handoff | Build on the tested real utility fork/parentPort payload slice, then implement native utility/renderer event.ports transfer and demanded authentication forwarding. Preserve native endpoint/process/document binding and revocation; pass a real Code-OSS connection beyond the main-only Protocol gate |
| 3 | Actual main-process bootstrap | Re-run the pinned entry after each blocker; implement required protocol, lifecycle and desktop services until real startup advances |
| 4 | Workbench bridge | Required preload/events and bounded binary MessagePort delivery connect the real workbench to main services |
| 5 | First usable desktop workspace | Real Code-OSS window opens, edits and saves files; file changes, settings and shutdown work |
| 6 | Development workflows and platform gates | Extension host/LSP, terminal/PTY, full required protocol/worker/webview/remote and native services pass defined workflows, followed by native packaged acceptance on each OS |

HTTP/2/3, additional example breadth and visual website work remain behind this
path unless a real Code-OSS trace makes one of them necessary. The sequence below
retains the larger acceptance milestones; a successful import alone is insufficient.

1. **Reproducible bootstrap and transport slices:** preserve the pinned application
   baseline, executable/source versions and real failing startup trace. Implement
   binary IPC and the required event/port/preload contracts in separately tested
   native slices. Resolve first startup imports and distinguish legitimate
   engine-specific adaptation from missing general Node APIs.
2. **Actual workbench:** launch the pinned desktop application using Firefly, not a
   look-alike shell. Require native window, real workbench/Monaco editing, required
   resource protocols, configuration delivery and clean shutdown. Select the first
   working native platform without claiming the others are complete.
3. **Usable workspace:** open/edit/save files and folders, handle external file
   changes, search, settings and persistence. Implement efficient bounded
   [file and binary transport](../guides/file-transfer.md); test large files and
   sustained small edits rather than just a startup screenshot.
4. **Developer workflows:** real utility/extension host, a controlled extension/LSP
   corpus, integrated terminal/PTY, Git/tasks and webviews. Test required native
   dependency adapters, process failure and workspace-trust boundaries.
5. **Cross-platform acceptance and delivery:** run those same workflows in native
   macOS/Linux/Windows packages, including upgrade/relaunch/security/resource
   measurements. Expand extension compatibility with recorded exact versions.

Several subsystems are missing outright, so a credible date or completion
percentage needs a runnable pinned bootstrap and transitive API/package inventory.
A startup-only milestone must not be described as full VS Code support. Neither
HTTP/2 nor HTTP/3 is a prerequisite established by this initial audit.
