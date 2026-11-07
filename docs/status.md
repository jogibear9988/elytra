# Current support and tested examples

Reviewed **2026-11-07** against the implementation, example catalog and completed
native CI logs. Mite and Firefly remain partial implementations. A checked-in
example, a successful unsupported-API rejection or a green formatting job does
not establish working feature support. Newer master commits need their own native
gates; the source snapshots below are deliberately explicit.

Local macOS Release now passes **77** runtime/component suites, adding custom process events
and restricted native signal subscriptions with real owned-child delivery and
embedded-host ownership/GC checks. That process slice also passes native
Linux/Windows at 94acc0c; see the [process contract](api/process.md). Code-OSS desktop still
does not run. A newer source-mode gate also evaluates the actual pinned Code-OSS
bootstrap-node.ts and its portable configuration under Mite after type erasure;
Desktop startup remains open. The
[source overlay](guides/code-oss-adaptation.md) makes actual Code-OSS platform/process
modules recognize truthful Mite identity; OS/arch/env/cwd/locales/timer and the
unadapted negative control pass locally and on native Linux at 10f6d70. An explicit
[cache application policy](architecture/0106-code-oss-libjs-cache-policy.md) now
passes local actual upstream development/missing/disabled/refusal branches. The
real full-source main.ts probe initializes the cache policy and reaches mainImpl,
then exits 1 on standalone Mite's missing Firefly-only original-fs module. It is
not a desktop launch. The cache slice now also passes native Linux at 790235c;
Windows acceptance of that slice passes at 5735b91.

The current [explicit Electron import option](api/firefly-cli.md) passes a local
macOS native batch: real main/preload alias, default npm resolution, argument
stripping, frozen restricted imports and no page privileges. Native argument and
real macOS package-launcher relocation/option/PID tests also pass. The Linux native alias gate also passes at 5735b91; whole flagged archive acceptance
remains pending. With the actual frozen root production dependencies, the Code-OSS
Firefly-host probe passes the registerHooks/promisify imports and reaches missing
Electron protocol (exit 1).
The workbench still does not run. The [dependency guide](guides/code-oss-adaptation.md#prepare-actual-root-production-dependencies)
records the 71 direct / 258 frozen snapshot graph, native/WASM candidates and
the real 14-case CLI parser test. The frozen dependency-install matrix passes all three native platforms at adc00d0
in [run 42389926763](https://github.com/jogibear9988/elytra/actions/runs/42389926763).
The new synchronous resolve-hook slice passes local macOS Node/Mite/host-GC checks;
its native Linux/Windows acceptance passes at a56d183. Linux 98866c3 fails linking before
CTest in [run 42390960317](https://github.com/jogibear9988/elytra/actions/runs/42390960317);
6c613ec adds the missing LibJS inline header and passes the new full Linux run below.
The new [promisify contract](api/util.md) passes local actual callback-service and
host-GC gates and native Linux/Windows at a56d183.

The native [protocol policy delivery](architecture/0112-native-protocol-configuration.md)
now passes local typed-codec/argv/lifecycle tests and actual headless diagnostic/
production helper tests. Each selected helper receives the exact immutable parent
policy before its VM starts; no page/preload setter is added. The first helper
launch seals even an empty policy and late registration rejects.

The [shared Rust/native origin entry](architecture/0113-shared-rust-origin-policy.md)
now passes the strict local macOS headless DOM gate for diagnostic and production
helpers, including IDNA/IPv6, ports, relative origins and default denial. The C++
URL::origin override is removed. Cargo source selection, explicit freshness and
actual dependency outputs are checked. Public protocol, standard parsing,
registration timing/worker propagation and resource transport remain unfinished.
Code-OSS still fails at the protocol import; no desktop launch is claimed.

The [protocol file-body component](../firefly/native/protocol/files/README.md)
passes a local macOS real-file/separate-child streaming gate with bounded chunks,
HEAD, cancellation and explicit mutation/type errors. Versioned response metadata
now transfers checked MIME/length and ordered HTTP header bytes before the body.
Native request owners now bound pending jobs, preserve epoch/ID/offset correlation
and revoke their files/credit on terminal paths. The framed native-child gate and
concurrent quota/deadline checks pass locally. Actual renderer request/document
binding, response application, authorized handler integration and public protocol
remain unfinished.

The [LibWeb protocol response adapter](architecture/0116-libweb-protocol-responses.md)
passes local actual pending-response/body-stream tests with real checked files,
GC, credit and controller/reader cancellation. The next
[scheme-fetch seam](architecture/0117-native-scheme-fetch-dispatch.md) also passes
actual JavaScript fetch/arrayBuffer, HEAD and AbortController against real files
in the same-process native fixture. Product helpers install no provider yet.
Live helper/document admission, navigation and public protocol remain open; this
is not a Code-OSS launch or new platform acceptance.

The [native resource request service](architecture/0118-native-resource-request-service.md)
now passes a local macOS real parent/child URL-request, credit and file-response gate.
Only the main-side native authorization callback selects a file path. Parallel
GET/HEAD, refusal, cancel/peer loss/forceful termination, quotas and a real fixed
30-second deadline release pending files. This completes the native request
component, not live WebContent/document binding or the public protocol API.

The [document-bound fetch client](architecture/0119-document-fetch-channel-client.md)
now connects that native service to actual global JavaScript fetch in a separate
LibWeb child fixture. Local macOS tests cover byte-exact file bodies, HEAD,
forbidden-header filtering, same-origin document isolation, eight accepted/ninth
refused requests, abort/reader cancellation, client destruction, malformed replies
and a silent-peer deadline. Only the parent opens files. This is still a native
fixture, not a live WebContent helper or a public protocol/session implementation.

The [shared fetch-scheme classification](architecture/0120-custom-fetch-scheme-classification.md)
now also includes explicitly registered standard schemes in the actual engine and
native UI predicates. Local native routing and existing headless helper regressions
pass. Navigation responses still require a separate admitted lifecycle: the current
document-bound client cannot bootstrap the first custom-origin document. No custom
navigation, public protocol handler or new platform support is accepted here.

The CPU-corrected **1d033fe** Linux renderer passes
[run 42399004586](https://github.com/jogibear9988/elytra/actions/runs/42399004586):
strict actual Rust DOM origins, native helper policy delivery, real Cargo dependency
checks, stripped archive and extracted Designer gates. This accepts the shared
origin composition on Ubuntu 24.04 x64 X11; it predates the native file-response
components and does not accept those or broader desktop platforms.

The later standalone **614ebf1** Windows run accepts all **78** tests in 140.07s
([run 42398340794](https://github.com/jogibear9988/elytra/actions/runs/42398340794)),
including the native compiler-driver argument/exit gate in 1.67s. This is CLI/build
tooling acceptance, not renderer Rust origins or the new file-response component.

The previous **bd83480** snapshot is accepted on native Linux (all **78**, 100.86s,
[run 42396266883](https://github.com/jogibear9988/elytra/actions/runs/42396266883))
and Windows (all **77**, 134.45s,
[run 42396266975](https://github.com/jogibear9988/elytra/actions/runs/42396266975)).
Its Linux renderer/default archive/Designer and native helper policy delivery pass
[run 42396266897](https://github.com/jogibear9988/elytra/actions/runs/42396266897).
That snapshot still reports the DOM origin gap; it does not accept the new Rust
composition, which requires fresh platform CI.

Native **a56d183** accepts promisify/resolve hooks on Linux (all **77**, 68.73s,
[run 42391944791](https://github.com/jogibear9988/elytra/actions/runs/42391944791))
and Windows (all **76**, 130.36s,
[run 42391944810](https://github.com/jogibear9988/elytra/actions/runs/42391944810)).
The later **f2e887b** Linux run passes all **78** including the origin source guard
(97.65s, [run 42393201428](https://github.com/jogibear9988/elytra/actions/runs/42393201428));
that standalone guard does not prove native renderer origin behavior.
The full Linux renderer/default archive/Designer **a56d183**
[run 42391944775](https://github.com/jogibear9988/elytra/actions/runs/42391944775)
passes, including actual opt-in host/preload aliases; candidate artifact
13582388416 is a 122713450-byte ZIP. It predates native protocol policy delivery.
New protocol component native CI results remain pending.

Linux **6c613ec** passes all **76** runtime suites in 96.57 seconds
([run 42391732431](https://github.com/jogibear9988/elytra/actions/runs/42391732431)),
including actual resolve hooks. It accepts the inline-header link correction;
new protocol changes need their own native results; later promisify acceptance is above.

The newer **adc00d0** Linux dependency/parser baseline passes all **75** suites
in 66.05 seconds ([run 42389926816](https://github.com/jogibear9988/elytra/actions/runs/42389926816)).
Windows adc00d0 passes all **74** in 128.09 seconds
([run 42389926814](https://github.com/jogibear9988/elytra/actions/runs/42389926814)).
The adc00d0 Linux renderer/default archive/Designer gate also passes
([run 42389926764](https://github.com/jogibear9988/elytra/actions/runs/42389926764)),
retaining candidate artifact 13581920595 (122684408-byte ZIP).
These runs do not include the new resolve hooks or promisify. The earlier snapshots below retain
separate platform/package evidence.

Native **5735b91** Linux passes all **74** suites in 78.51 seconds
([run 42386340937](https://github.com/jogibear9988/elytra/actions/runs/42386340937));
Windows passes all **73** in 130.32 seconds
([run 42386340970](https://github.com/jogibear9988/elytra/actions/runs/42386340970)).
Windows omits the POSIX launcher fixture. These include the actual cache/platform/
bootstrap and startup option gates. Later success does not diagnose or establish
a fix for earlier Node reference crashes below.
The full Linux renderer/archive/Designer
[run 42386340939](https://github.com/jogibear9988/elytra/actions/runs/42386340939)
also passes at **5735b91**, including actual native default/opt-in host/preload
alias assertions. The complete app archive uses the default launcher mode;
whole --electron-compat archives still require their own acceptance.
Native package launcher opt-in/default relocation/PID checks also pass in
[run 42386340998](https://github.com/jogibear9988/elytra/actions/runs/42386340998).

Newer native **Linux 10f6d70** passes all **71** suites in 77.23 seconds
([run 42381776025](https://github.com/jogibear9988/elytra/actions/runs/42381776025)).
Native **Windows 2bf956d** passes all **70** in 102.74 seconds
([run 42380750929](https://github.com/jogibear9988/elytra/actions/runs/42380750929)).
Windows 10f6d70 passes the new actual platform gate, but its overall
[run 42381775982](https://github.com/jogibear9988/elytra/actions/runs/42381775982)
fails because the Node 24.15 reference exits 0xC0000409 in HTTP/HTTPS fixtures.
Mite's HTTP fixture completes; no complete 71-suite Windows acceptance or crash
diagnosis is claimed. Keep the failing reference checks intact.

Native **94acc0c** passes all **69** suites on
[Linux](https://github.com/jogibear9988/elytra/actions/runs/42380475178) (76.84 seconds)
and [Windows](https://github.com/jogibear9988/elytra/actions/runs/42380475051)
(102.93 seconds). This includes process events: actual owned-child POSIX signals
on Linux, registration/unref/policy on Windows. No Windows POSIX delivery claim.

Native Linux and Windows x64 at **678c85e** pass all **68** Mite suites:
[run 42376107084](https://github.com/jogibear9988/elytra/actions/runs/42376107084)
(Linux, 83.24 seconds) and
[run 42376107074](https://github.com/jogibear9988/elytra/actions/runs/42376107074)
(Windows, 124.04 seconds), including module utilities, the collecting raw-FS host gate and
the native performance clock/pinned Code-OSS module. This is standalone/host API
acceptance, not a Code-OSS desktop launch, full Linux app archive or Windows desktop proof.

The [node:module utilities](api/module.md) at 8f617c0 pass local macOS ARM64 Node
differential/embedded-host GC checks and the full **66-suite** Mite run. This adds
ESM-owned createRequire, not Code-OSS startup or compile-cache/V8 support. The
newer native Linux/Windows gates above include these utilities.

The newer [node:perf_hooks clock](api/perf-hooks.md) passes local native clock/GC
checks and executes the pinned Code-OSS performance module after type erasure,
using that module's mark fallback. It supplies now/timeOrigin only; it does not
establish full performance API or desktop bootstrap support. The newer native
Linux/Windows gates above include the clock and actual pinned module checks.

Latest completed macOS Release/package verification:
[run 42348529659](https://github.com/jogibear9988/elytra/actions/runs/42348529659)
at **8dc5830**, including all six shared example smoke slices, explicit gates for
the other 17 examples, native preload/IPC checks and the packaged Designer's
edit/Monaco/HTML window controls. Its Actions artifacts are separate from Alpha 2.
The baseline Linux renderer run
[42351420391](https://github.com/jogibear9988/elytra/actions/runs/42351420391) and
native Windows renderer run
[42352483937](https://github.com/jogibear9988/elytra/actions/runs/42352483937)
both completed with failure; neither establishes complete platform acceptance.
That Linux archive rejected conflicting libdbus providers; the Windows renderer
compile rejected an adapter include path. The newer Linux run below passes after
provider/ELF fixes; no newer Windows desktop acceptance follows from it.

The full Linux renderer/package [run 42379262065](https://github.com/jogibear9988/elytra/actions/runs/42379262065)
at **197cd2a** now passes: portable CPU-command checks, renderer/helper/isolation,
trusted byte handoff, live X11 app/screen/clipboard/kernel checks, stripped archive
extraction/ELF provenance and the extracted bundled Designer's edit/Monaco/HTML
controls. Its [linux-designer-package-candidate artifact](https://github.com/jogibear9988/elytra/actions/runs/42379262065/artifacts/13577811546)
is an experimental Ubuntu 24.04 x64 X11 candidate, separate from published releases
and later master changes.

## Platforms and delivery

| Product / platform | Confirmed evidence | Remaining work |
| --- | --- | --- |
| Mite / macOS ARM64 | Current local 77 runtime/component suites including actual Code-OSS shared bootstrap/platform/cache-policy, pure native startup/launcher checks and synchronous resolve-hook/host-GC checks; earlier native package gate at 85557be | Full Node API coverage, older hosts and fresh production distribution |
| Mite / native Linux x64 | All 78 enabled suites at f2e887b; [run 42393201428](https://github.com/jogibear9988/elytra/actions/runs/42393201428) | New native protocol component, wider hosts/architectures and unsupported API coverage; different profiles need separate acceptance |
| Mite / native Windows x64 | All 76 enabled suites at a56d183; [run 42391944810](https://github.com/jogibear9988/elytra/actions/runs/42391944810). Earlier oracle crashes remain undiagnosed | New protocol component, oracle crash diagnosis, full Windows API/error parity and distribution |
| Firefly / macOS ARM64 | Native windows, documented preload/IPC and desktop-service subsets; packaged Designer edit/Monaco/window controls pass at 85557be in [run 42345882824](https://github.com/jogibear9988/elytra/actions/runs/42345882824) | Full Electron parity, signing/notarization, installers and older hosts |
| Firefly / Linux x64 | Experimental Ubuntu 24.04 X11 app/screen/clipboard/kernel, portable CPU profile, stripped archive and extracted Designer edit/Monaco/HTML controls plus native host/preload alias pass at a56d183 in [run 42391944775](https://github.com/jogibear9988/elytra/actions/runs/42391944775) | Wider hosts/physical displays, Wayland, installer/update/signing and published Linux release; earlier SIGILL cause is not diagnosed |
| Firefly / native Windows | Production desktop not implemented/accepted; [renderer](development/windows-build.md#experimental-native-libweb-renderer-gate) and [Qt window prototype](development/windows-build.md#native-qt-window-prototype) have native CI gates, execution pending | Native renderer/window/process/sandbox services and app packaging; no WSL or alternate-engine fallback |

[Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2) contains
tested macOS ARM64 source 19ad113, with original artifact version 0.1.0-dev.118.
It includes the Designer. It does not contain subsequent master changes.
The later optimized macOS Actions artifact at 85557be measures **196.21 MiB** for
the framework and **230.33 MiB** for Designer, uncompressed. It is not a newly
published release; [size evidence](development/package-size.md) records hashes,
profiles and limitations. Linux Designer archives are candidate workflows whose
actual app gates must pass before upload/publication.

## Which Electron examples work?

Our [catalog](examples/index.md) covers all 21 topics currently listed in the
[Electron overview](https://www.electronjs.org/docs/latest/tutorial/examples),
plus quick-start and lifecycle: **23 original Elytra apps**. This does not mean
every Electron Fiddle/subexample runs unchanged.

| Result | Count | Scope |
| --- | ---: | --- |
| Shared Firefly/Electron native smoke assertions pass | **6** | Quick-start; browser module worker; initial online/offline state; iframe messaging; frameless startup; lifecycle/cancellation/exit |
| Partial API coverage, manual example acceptance outstanding | **1** | Dark mode: direct nativeTheme/CSS tests exist; the catalog button/visual flow remains manual |
| Blocked by missing API/platform requirements | **16** | Device access, purchases, shortcuts, deep links, desktop actions, local AI handler, menus, native file drag/drop, navigation history, notifications, offscreen rendering, progress bars, recent documents, represented files, spellchecker, taskbar integration |

The six shared example IDs are quick-start, multithreading, online-offline,
web-embeds, window-customization and lifecycle. This is scoped **macOS ARM64**
evidence, confirmed in the completed 85557be run above. A DOM worker does not mean
Mite node:worker_threads works; an iframe does not mean WebContentsView works;
initial connectivity does not prove native network-change events. See each topic's
[contract and blockers](examples/index.md). No Linux/Windows acceptance of all
23 desktop apps is claimed.

## Supported API families

Every family below is a subset with documented limits, not a general Node/Electron
compatibility promise. The [Mite matrix](compatibility/mite-0.1.md) and
[Firefly matrix](compatibility/firefly-0.1.md) define finer-grained status.

| Mite family | Implemented subset / detailed contract |
| --- | --- |
| Runtime/modules | LibJS CLI, ESM/CommonJS and package resolution; [runtime overview](api/mite.md), [CLI](api/cli.md) |
| Events/async | EventEmitter, nextTick/microtasks, timers/immediates and selected cancellation; [events](api/events.md), [timers](api/timers.md), [AbortSignal](api/abort.md) |
| Files/data/OS | Selected fs/Promise/FileHandle, Buffer, URL/path/encoding and OS/process APIs; [runtime overview](api/mite.md), [process](api/process.md) |
| Streams | Selected Node streams, Web Streams and bridges; [streams](api/stream.md), [Web Streams](api/web-streams.md) |
| Network/web | TCP, DNS, HTTP clients/server, TLS/HTTPS clients and Fetch bodies/redirects/cancellation; [net](api/net.md), [HTTP](api/http.md), [TLS](api/tls.md), [HTTPS](api/https.md), [Fetch](api/fetch.md) |
| Crypto/processes | Random/hash/HMAC/equality; native spawn/execFile and pipes; [crypto](api/crypto.md), [child processes](api/child-process.md) |
| Body data/decompression | Blob/File/FormData and selected gzip/deflate decoding; [body APIs](api/blob-formdata.md), [decompression](api/decompression.md) |

Mite still lacks full fs APIs, general util, worker_threads, exec/fork/synchronous
child processes, TLS/HTTPS servers, Agents, HTTP/2, UDP, Web Crypto, native Node
addons/N-API/V8 internals and many options. The per-API references describe exact
errors, overloads and intentional differences.

| Firefly family | Implemented subset / important limits |
| --- | --- |
| app/window/webContents | Lifecycle/readiness, native windows, loading, geometry and selected controls/events; [app](api/app.md), [BrowserWindow](api/browser-window.md), [webContents](api/web-contents.md) |
| Secure preload/contextBridge | Bounded isolated top-level preload publication; authored ESM is bundled to supported CommonJS payloads; [contextBridge](api/context-bridge.md) |
| IPC | Asynchronous invoke/handle/handleOnce/removeHandler with sender checks; one-way/sync/ports/subframes unfinished; [IPC](api/ipc.md) |
| Desktop services | Partial macOS clipboard, screen and nativeTheme; scoped X11 clipboard; new Linux screen wiring awaits native acceptance; [clipboard](api/clipboard.md), [screen](api/screen.md), [nativeTheme](api/native-theme.md) |

Menu/MenuItem, dialog, Tray, shell, nativeImage, session/permission/protocol control,
notifications, global shortcuts, auto-updates and most Electron APIs remain absent.
Renderer Node integration is rejected; privileged Mite APIs stay in the main process.

## Documentation currency

This review corrects stale overview/migration claims that HTTP clients, TLS/HTTPS,
Fetch, child processes and clipboard were entirely unavailable. It updates the
current baseline from older 64-suite notes to the confirmed 65-suite CLI runs and
keeps experimental Linux implementation separate from completed acceptance.
Historical ADR/progress entries retain their original source-specific results.
Per-feature tests do not certify all options or platforms; newer changes still
require native results before their status advances. Use [PROGRESS](PROGRESS.md)
for individual changes and [roadmap](roadmap.md) for unfinished delivery scope.

The latest review also corrects the runtime overview's obsolete execFile rejection
and separates the Firefly matrix's historical bridge stages from current support.
API reference depth is still incomplete: the
[documentation baseline](reference/documentation-baseline.md) tracks the remaining
member-by-member descriptions and guides. Documentation currency does not imply
that every Node/Electron member has been implemented or documented in full.
