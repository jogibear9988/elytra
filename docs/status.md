# Current support and tested examples

Reviewed **2026-11-07** against the implementation, example catalog and completed
native CI logs. Mite and Firefly remain partial implementations. A checked-in
example, a successful unsupported-API rejection or a green formatting job does
not establish working feature support. Newer master commits need their own native
gates; the source snapshots below are deliberately explicit.

## Platforms and delivery

| Product / platform | Confirmed evidence | Remaining work |
| --- | --- | --- |
| Mite / macOS ARM64 | 65 CLI suites and native package gate at 85557be; independent LibJS runtime | Full Node API coverage, older hosts and production distribution |
| Mite / native Linux x64 | All 65 enabled suites at da5e3e8; [run 42349719739](https://github.com/jogibear9988/elytra/actions/runs/42349719739) | Wider hosts/architectures; new portable-CPU profile needs its own acceptance |
| Mite / native Windows x64 | All 65 enabled suites at da5e3e8; [run 42349719866](https://github.com/jogibear9988/elytra/actions/runs/42349719866) | Full Windows API/error parity, other architectures and standalone distribution |
| Firefly / macOS ARM64 | Native windows, documented preload/IPC and desktop-service subsets; packaged Designer edit/Monaco/window controls pass at 85557be in [run 42345882824](https://github.com/jogibear9988/elytra/actions/runs/42345882824) | Full Electron parity, signing/notarization, installers and older hosts |
| Firefly / Linux x64 | Experimental X11 app/renderer/preload/IPC/clipboard and live kernel checks pass at 8dc5830 in [run 42348529660](https://github.com/jogibear9988/elytra/actions/runs/42348529660) | New screen integration, portable CPU build, complete archive and Designer gates pending; newer da5e3e8 app run failed with SIGILL |
| Firefly / native Windows | Desktop not implemented/accepted; [headless renderer bring-up](development/windows-build.md#experimental-native-libweb-renderer-gate) now has a native CI gate, execution pending | Native renderer/window/process/sandbox services and app packaging; no WSL or alternate-engine fallback |

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
