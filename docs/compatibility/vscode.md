# VS Code / Code-OSS compatibility target

Status: **not supported**. Requested as a priority compatibility target on
2026-11-07. This is a source-based blocker audit and implementation order, not a
successful launch, complete dependency inventory or percentage-complete estimate.
Running Monaco in Designer does not establish Code-OSS desktop compatibility.

## Current priority: first real workbench, then developer workflows

Updated 2026-11-08. The real prepared entry now passes the simple custom-protocol
CORS privilege and stops at allowServiceWorkers. This is not just a missing flag:
Ladybird at Elytra's pinned revision refuses non-HTTP(S) service-worker registration
in ServiceWorkerContainer::start_register; Job.cpp also rejects the unimplemented
Run Service Worker step after fetching a new script. Firefly's current file channel
intentionally refuses worker/frame clients. Both engine execution and secure worker
resource routing would need implementation before advertising this privilege.

The Code-OSS main registration requests allowServiceWorkers specifically for
vscode-webview, codeCache for vscode-webview and vscode-file, and secure/fetch/CORS
on its nonstandard remote resource schemes. These are separate unresolved contracts.
The engine overlay currently removes none of them. Do not treat the current simple
CORS slice as support for all these scheme declarations.

Prioritize these concrete outcomes:

1. **Unblock main startup honestly.** Audit whether the first local workbench can
   explicitly defer webviews/remote resources and engine-specific caches. Any such
   mode must be a separately documented opt-in application adaptation with visible
   limitations, preserved ordinary Electron behavior and refusal of unavailable
   features. It is planned, not implemented. Otherwise implement the actual demanded
   contract; do not silently remove privileges or report a fake worker/cache.
2. **Build independent startup prerequisites now.** The pinned mainImpl next uses
   getPreferredSystemLanguages before readiness, getSystemLocale after readiness
   and getLocale for NLS. The [native locale slice](../api/app-locale.md) now passes
   local macOS native/public-host acceptance, including independent regional/UI/
   application preferences. Fresh Linux and full NLS execution remain unverified.
   Continue other demanded main/IPC prerequisites while the worker architecture is assessed.
   Continue the actual entry after each resolved blocker instead of implementing
   unrelated Electron examples.
3. **Connect the real workbench.** Prioritize secure preload/event IPC and public
   binary MessagePorts, file protocol/interception and demanded window services.
   Preserve native sender/document identity, revocation and bounded file streaming.
   Require a real editor opening/editing/saving a local file before declaring the
   first workbench milestone accepted.
4. **Complete developer workflows.** Add extension/shared/PTY hosts, watching,
   terminal/Git/tasks and webviews with their actual worker/resource requirements.
   A limited first workbench must keep these missing features explicit. Require
   the same workflow evidence separately for native packaged platforms.

The pinned webview HTML has a disableServiceWorker option, but that alone is not a
resource-loading fallback: it also skips the worker's host-resource messaging path.
No usable webview fallback or adapted workbench mode has been established. Keep
this an investigation item rather than a compatibility claim.

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
| Main process startup | mainImpl imports original-fs, node:perf_hooks and Electron protocol/Menu/crashReporter/contentTracing | Real Firefly --electron-compat host probe now resolves raw FS and the implemented Electron alias, with the actual frozen root production dependencies, passes registerHooks/promisify and, with explicit --libjs-engine, passes the audited implicit Chromium defaults and fails at unsupported protocol privilege allowServiceWorkers (exit 1). Without that opt-in it still rejects the enable-features append. Implement the required real protocol contract next; protocol now supports origin registration and default-session file callbacks; command-line reads now work; incompatible Chromium feature switches and other app methods remain missing or partial; contentTracing has a tested native main-only subset; application menus now have a tested local native subset. No workbench/startup acceptance |
| Locale/NLS prerequisites | mainImpl reads preferred system languages before ready, then regional/app locale | Native getPreferredSystemLanguages/getLocaleCountryCode and ready-guarded getSystemLocale/getLocale now pass local macOS no-window host/native tests with distinct Foundation/UI/application settings. Actual full Code-OSS still stops earlier at allowServiceWorkers; its NLS workflow and fresh Linux acceptance remain unverified. [Contract](../api/app-locale.md) |
| Custom resource origins | mainImpl registers vscode-webview/vscode-file and remote resource schemes; protocolMainService serves authorized files and blocks direct file access | Internal actual LibURL/LibWebCommon custom origin/trust foundation passes local native tests; helpers remain empty by default; native parent/owned-helper configuration before bootstrap now passes local headless tests. Rust/native/DOM origin extraction now shares the canonical native policy, with strict local macOS and 1d033fe hosted Linux headless acceptance. Public [origin registration](../api/protocol.md) now passes local main/LibJS gates, and internal production-helper ESM/CSS/file loading passes. Public default-session file callbacks and automatic grants now load real custom windows/resources locally. Complete parsing, general handlers, partitions and additional privileges remain required. [ADR 0113](../architecture/0113-shared-rust-origin-policy.md) |
| Preload and messaging | Preload uses send/invoke/on/once/removeListener, webFrame zoom, webUtils and port handoff | Bounded invoke/handle and renderer-to-main send/on events now have local macOS acceptance; main factory policy also passes Node/Mite and a real Electron positive send reference passes. Main-to-preload send/listener methods now also pass local macOS acceptance and the positive Electron reference, with real stalled-peer timeout retirement/reload. The bounded document-callback bridge now passes local native/production-helper tests (data payloads, identity, reentry and Promises); native IPC events now cross with the exact private sender module replaced by an empty object, and individual raw methods stay blocked. The 64 KiB contextBridge binary copy subset now passes local native and production-helper acceptance; the native raw process path now passes local early invoke/binary reply/bidirectional send, view geometry and reload acceptance, with bounded admission and history ordering. Native handle handoff checks epoch/document/PID and rejects unknown-realm/wrong-document/duplicate handoffs. Own trusted source publication getters now pass native and production-helper tests, including the metadata getter shape used here; restricted native sandbox process metadata and additionalArguments now have local helper/reference acceptance, including the argv/config IPC/userEnv merge shape. Need the remaining full preload graph, process events/metrics, port transfer and file access semantics (a bounded native HTML file-picker now passes local native-view/helper acceptance, but native File path provenance and File bridge passage remain missing); native webFrame factor/level zoom now has local helper/layout/reference acceptance, with revocation/sender checks |
| Binary channel protocol | Code-OSS MessagePort Protocol sends Uint8Array-backed VSBuffer messages | Firefly raw binary invoke/reply/send now passes local native/public acceptance with up to 64 KiB of distinct backing storage and a 128 KiB frame budget. MessageChannelMain/MessagePortMain and renderer port handoff remain missing; current copied buffers do not establish port/stream support |
| Workspace files | Disk provider channel includes open/read/write/readFile/readFileStream/watch and cancellation | Broaden filesystem/stream/watch APIs and native watcher lifecycle; actual open/edit/save/rename/external-change tests must pass |
| Native services | Manifest includes node-pty, @parcel/watcher, @vscode/sqlite3, native-keymap and other native packages | Root production graph is now pinned and installed without lifecycle scripts on macOS ARM64; 27 packages have native/WASM evidence. Other install roots remain unaudited. No Node addon loader/N-API. Establish each required package/ABI; implement engine-independent bindings or compatible service adapters as required. A Node/V8 host is not an allowed shortcut |
| Auxiliary/extension processes | Bootstrap names extension-host, shared-process, pty-host and agent-host roles | Need utility/process messaging and extension-host runtime compatibility, shutdown/crash recovery and explicit extension limitations; spawning an executable alone is insufficient |
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
General Response handlers and broader session support remain open.
This is not Code-OSS startup.

The pinned ProtocolMainService uses defaultSession.protocol.registerFileProtocol for
its authorized workbench files. The [native async authorization service](../architecture/0125-asynchronous-file-authorization.md)
now accepts delayed/out-of-order completions with quota/deadline/cancel ownership;
actual helper ESM/CSS/file loading passes with deferred native authorization. Default-session Main-VM
callback delivery and automatic grants now pass actual public-window tests. General
handlers, required privilege flags and the real Code-OSS main services remain open.

Immediate execution order (each step needs real native acceptance):

| Priority | Deliverable | Exit criterion |
| --- | --- | --- |
| 1 | Required protocol privileges and handlers | Build on the tested origin/file/simple-CORS path; inspect the real allowServiceWorkers demand, actual upstream worker support and required file interception. Never accept a flag without its native behavior |
| 2 | Actual main-process bootstrap | Re-run the pinned entry after each blocker; implement required protocol, lifecycle and desktop services until real startup advances |
| 3 | Workbench bridge | Required preload/events and bounded binary MessagePort delivery connect the real workbench to main services |
| 4 | First usable desktop workspace | Real Code-OSS window opens, edits and saves files; file changes, settings and shutdown work |
| 5 | Development workflows and platform gates | Extension host/LSP, terminal/PTY and native services pass defined workflows, followed by native packaged acceptance on each OS |

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
