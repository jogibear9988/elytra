# Current support and tested examples

Reviewed **2026-11-17**. Mite and Firefly are partial implementations. This page
summarizes current evidence; individual API references state signatures, limits
and platform scope. Historical checkpoints remain in [PROGRESS](PROGRESS.md) and
the architecture records. A source check or successful compilation alone does
not establish a working desktop application.

## Platforms and delivery

| Product / platform | Confirmed evidence | Remaining work |
| --- | --- | --- |
| Mite / macOS ARM64 | Release CLI and embedded host build with Ladybird `9803f2d5d888e2afd23d69d1da920f78f4279918`. All 165 local CLI gates have passing evidence: 163 in the full run, then the two repaired fixtures in a targeted rerun. | Full Node parity, broader host/architecture coverage and fresh distribution acceptance. |
| Mite / Linux x64 | Earlier full native CLI runs pass; later review changes and the new Ladybird pin have fresh CI pending. | Current complete native gate and broader distributions. |
| Mite / Windows x64 | Earlier scoped native baselines pass. The `05149fe` run builds but passes only 151/160 suites; follow-up repairs require fresh native acceptance. | Current green CLI gate and distribution. See [Windows bring-up](development/windows-build.md). |
| Firefly / macOS ARM64 | Native windows, isolated preload/IPC and documented desktop subsets. Earlier packaged Designer edit/Monaco/window-control gates pass. Controlled Code-OSS editor/terminal workflows and a visible Welcome launch pass on the preceding SDK. | Rebuild/accept the latest Ladybird SDK, fresh packages and relocated Code-OSS Launcher; broader desktop reliability, signing/notarization and installers. |
| Firefly / Linux x64 | Earlier experimental Ubuntu 24.04 X11 app, screen/clipboard, extracted Designer and kernel-isolation gates pass at `a56d183`. | Current renderer/package CI, wider displays/hosts, Wayland and distribution. |
| Firefly / Windows x64 | Experimental native renderer/window and confinement components are under development. No complete Windows desktop or Designer/Code-OSS executable package is accepted. | Real helper-role confinement, broker flow control, full desktop services and native PE/DLL packaging. |

The new Ladybird pin has **CLI evidence only** at this checkpoint. The visible
Code-OSS app uses the preceding `b872` SDK; do not combine those results into a
claim that the new renderer or packages pass. Ladybird remains unmodified, with
integration changes in Elytra's owned adapters/build files.

[Alpha 2](https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.2)
is the published macOS ARM64 release, including Designer. It predates the current
review work and contains no Code-OSS package or Windows apps. Later optimized
macOS CI artifacts measured 196.21 MiB for the framework and 230.33 MiB for
Designer, uncompressed; those are separate historical artifacts, not current
release measurements. See [package-size evidence](development/package-size.md).

The current release driver builds four product archives: Mite, Firefly, Designer
and partial Code-OSS. Its new sample build and relocated native Launcher gates
must pass before upload/publication. The [packaging guide](guides/packaging.md)
and [Code-OSS packaging contract](guides/code-oss-packaging.md) describe the actual
pipeline and outstanding acceptance. A Windows release remains blocked.

## Code-OSS checkpoint

Controlled macOS offscreen workflows pass real CommonJS and ESM extension
activation, commands and open/edit/save with independent file readback. The latest
controlled CommonJS terminal run also passes a real Bash terminal, `sendText`,
exit status and three timed parent-liveness checks. Main exits 0; all 14 observed
native identities disappear with zero cleanup signals.

A separate Cocoa launch displays the Welcome workbench, sidebar and native menu
bar and was left running for user inspection. That observation does not establish
menu actions, physical editing/input, arbitrary extensions or reliable startup:
preceding Cocoa attempts encountered webRequest policy/navigation refusals whose
cause remains unresolved. Controlled offscreen workflows are separate evidence.

The application still uses explicit temporary source/dependency adaptations.
Their reasons and removal criteria are tracked in the
[adaptation register](development/code-oss-adaptations.md). See the
[Code-OSS compatibility record](compatibility/vscode.md) for exact workflows,
remaining blockers and evidence. Full VS Code compatibility is not claimed.

## Which Electron examples work?

The [catalog](examples/index.md) covers the 21 topics in Electron's examples
overview plus quick-start and lifecycle: **23 original Elytra apps**. These are
adapted examples, not acceptance of every Electron Fiddle unchanged.

| Result | Count | Evidence scope |
| --- | ---: | --- |
| Shared Firefly/Electron native smoke assertions pass | 6 | Quick-start, browser module worker, initial online/offline state, iframe messaging, frameless startup and lifecycle/cancellation/exit; earlier macOS ARM64 package gate. |
| Partial API coverage; catalog interaction outstanding | 1 | Dark mode has direct nativeTheme/CSS tests; the catalog visual/button flow remains manual. |
| Missing requirements or unaccepted application workflow | 16 | See each catalog topic's current API links and blockers. Implementing part of an API does not automatically validate its example. |

No complete Linux/Windows acceptance of all 23 apps is claimed. A browser worker
and an iframe have different contracts from Mite worker_threads and native
WebContentsView. Initial network state does not prove network-change delivery.

## Supported API families

Use the [Mite matrix](compatibility/mite-0.1.md),
[Firefly matrix](compatibility/firefly-0.1.md) and [API index](api/index.md) for
member-level scope. Every family below is a subset.

| Mite family | Detailed contracts |
| --- | --- |
| Runtime and modules | LibJS CLI, ESM/CommonJS, package resolution and supported module hooks: [Mite](api/mite.md), [modules](api/module.md), [CLI](api/cli.md). |
| Events and scheduling | [EventEmitter](api/events.md), [timers/checkpoints](api/timers.md), [process errors](api/process-errors.md) and [AbortSignal](api/abort.md). |
| Files, data and OS | Selected filesystem, Buffer, URL/path, encoding and native process services: [Mite overview](api/mite.md), [Buffer](api/buffer.md), [process](api/process.md). |
| Streams and web data | [Node streams](api/stream.md), [Web Streams](api/web-streams.md), [Blob/File/FormData](api/blob-formdata.md) and [compression](api/decompression.md). |
| Networking | [TCP](api/net.md), [HTTP](api/http.md), [TLS](api/tls.md), [HTTPS](api/https.md) and [Fetch](api/fetch.md); exact supported client/server modes differ. |
| Native concurrency and processes | Selected [child process](api/child-process.md) and [worker_threads](api/worker-threads.md) operations, with independent VM and lifetime boundaries. |
| Crypto and utilities | Selected [crypto](api/crypto.md), [util](api/util.md) and [console](api/console.md) services. |

HTTP/2 and HTTP/3 are planned, not implemented. Full Node API coverage, V8 internals,
arbitrary native addons/N-API and many native options remain outside the accepted
surface. Unsupported operations must fail explicitly; importing a module is not
proof of every member's support.

| Firefly family | Detailed contracts |
| --- | --- |
| Application and views | [app](api/app.md), [BrowserWindow](api/browser-window.md), [webContents](api/web-contents.md) and [WebContentsView](api/web-contents-view.md). |
| Isolation and communication | [contextBridge](api/context-bridge.md), [IPC](api/ipc.md), [main ports](api/message-channel-main.md) and [utility processes](api/utility-process.md). Native renderer/utility port handoff has scoped macOS evidence; full clone/transfer/platform parity remains open. |
| Session and requests | [Document-bound permission policy](api/session.md), [webRequest](api/web-request.md), [custom protocols](api/protocol.md) and [main/utility net](api/desktop-net.md). Custom sessions, complete browser storage/auth/proxy behavior and broader interception remain unfinished. |
| Native desktop services | Partial [menus](api/menu.md), [dialogs](api/dialog.md), [clipboard](api/clipboard.md), [screen](api/screen.md), [shortcuts](api/global-shortcut.md), [theme](api/native-theme.md), [safeStorage](api/safe-storage.md) and other indexed services. Platform and actual OS/UI acceptance vary per API. |

Renderer Node integration remains rejected. Main-process Mite capabilities stay
privileged; preload/IPC publication is bounded. Real notification delivery,
nonempty NativeImage, Tray, complete menu/accelerator behavior and auto-updates
remain unavailable or unaccepted. See [security guidance](guides/security.md).

## Documentation currency

This review replaces an accumulated status narrative that still called webRequest,
worker_threads, dialogs and the workbench entirely absent after those slices had
advanced. It also corrects permission and process-error statements in guides.
Historical ADR/progress entries retain the evidence of their original source/run;
newer native changes need their own platform results.

The [documentation baseline](reference/documentation-baseline.md) tracks remaining
reference depth. [PROGRESS](PROGRESS.md) records logical commits and validation;
the [roadmap](roadmap.md) tracks unfinished work. Current docs do not imply complete
Node/Electron compatibility or that every example and platform passes.
