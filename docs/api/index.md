# API reference

Mite and Firefly are early implementations. This reference describes the current
public surface, not full Node.js or Electron compatibility. Native validation is
macOS ARM64 and scoped native Mite gates: Linux passes 78 suites at f2e887b;
Windows passes 76 at a56d183. Firefly has tested macOS subsets and an experimental X11 app;
Windows desktop remains unfinished. Experimental Ubuntu 24.04 x64 X11 stripped
archive/Designer, screen integration and portable CPU-command gates pass at 197cd2a;
wider Linux distribution support and release publication remain unfinished.
[Current support and tested examples](../status.md) records exact source/run
references; use the matrices and per-API contracts for individual limits.

For tutorials, concepts and migration, start at the [documentation home](../index.md).
The [Firefly CLI/import policy](firefly-cli.md) documents --electron-compat,
restricted preload aliases and recorded native launcher options.
The [documentation baseline](../reference/documentation-baseline.md) tracks reference
depth still required alongside the implementation.

## Mite

[Mite runtime API](mite.md) covers entry points, modules, globals and Node-style
built-ins. The [CLI entry](cli.md) defines flags and exit behavior.
Mite runs independently of Firefly, Qt and LibWeb.
The [node:util reference](util.md) covers promisify, custom/named results and encoding classes.
The [node:module reference](module.md) covers ESM-owned require functions, shared
cache identity and the available builtin inventory.
The [node:perf_hooks reference](perf-hooks.md) covers native monotonic now and the
fixed process runtime timeOrigin; timeline/profiling APIs remain absent.
The [events reference](events.md) covers synchronous EventEmitter dispatch,
error delivery and cancellable Promise once waits.
The [node:crypto reference](crypto.md) details the supported random/hash APIs,
parameters, errors, resource lifetime and Node differences.
The [node:net reference](net.md) covers TCP sockets/listeners, stream ownership,
AbortSignal cancellation and native shutdown.
The [node:tls reference](tls.md) covers native client records, trust, verification,
stream ownership and unfinished server/context modes.
The [node:http reference](http.md) covers client streams, options, errors and the
current connection policy alongside the existing server API.
The [node:https reference](https.md) describes verified native HTTPS clients using
those same HTTP request/response classes.
The [cancellation reference](abort.md) covers EventTarget/AbortController/AbortSignal
and native Promise timer consumers.
The [timers reference](timers.md) covers native timeout/immediate callbacks,
checkpoints and handle lifetime.
The [Web Streams reference](web-streams.md) covers readable/writable/transform
streams, byte ownership, piping, pressure and current interoperability limits.
The [Node/Web stream bridge reference](stream.md) describes conversion, endpoint
ownership, errors and native HTTP integration.
The [Fetch reference](fetch.md) covers Headers, Request, Response, incremental
HTTP/HTTPS bodies, redirects, cancellation and unsupported web-platform modes.
The [Blob/File/FormData reference](blob-formdata.md) covers immutable bytes, file
metadata, multipart uploads and body consumers.
The [decompression reference](decompression.md) describes native Web Stream gzip/
deflate decoding, automatic Fetch content handling, pressure and integrity limits.
The [child-process reference](child-process.md) describes native spawn, pipes,
process lifetime, bounded execFile/exec capture, genuine OS shells and custom
promisified child/output results. The new exec slice has local macOS acceptance;
fresh Linux/Windows shell execution remains unverified.
The [process reference](process.md) covers identity/environment, clocks, CPU usage,
custom events and the explicitly owned native signal subset. Local macOS passes
77 runtime/component suites including actual Code-OSS source bootstrap, platform/process,
the application-only cache policy, pure startup/launcher checks, real CLI parser
synchronous resolve chains and promisify/native callback services with actual host GC.
At adc00d0 native Linux passes all 75 suites and Windows all 74. Native resolve-hook
acceptance passes Linux at 6c613ec (76 suites) after its inline-definition link fix;
promisify passes Linux/Windows at a56d183; new native protocol components
require separate gates.
Full desktop startup remains unfinished; see the [Code-OSS inventory](../compatibility/vscode.md).

```js
import {createHash} from 'node:crypto';
console.log(createHash('sha256').update('hello').digest('hex'));
```

## Firefly

The partial [protocol API](protocol.md) now publishes application scheme origin
registration through the [custom-origin foundation](../architecture/0111-custom-protocol-origins.md)
and native owned-helper configuration. Default-session public file handlers now load
real windows/resources; broader protocol/session compatibility remains partial. The shared native/Rust DOM origin
gate now passes locally on macOS; see [ADR 0113](../architecture/0113-shared-rust-origin-policy.md).

The [original-fs reference](original-fs.md) describes raw main-process filesystem
access, separate facades and the remaining ASAR limitation.

[Firefly desktop API](firefly.md) covers app lifecycle, native BrowserWindow,
webContents, events and errors. Firefly embeds Mite in its main process and uses
separate Ladybird renderer processes. Detailed entries: [app](app.md) and [native locale/languages](app-locale.md),
[BrowserWindow](browser-window.md), [webContents](web-contents.md) and
[preload/contextBridge](context-bridge.md), [ipcMain/ipcRenderer](ipc.md) and
[screen](screen.md), [clipboard/ClipboardItem](clipboard.md), [nativeTheme](native-theme.md)
and [protocol](protocol.md), [session](session.md), [crashReporter](crash-reporter.md), [Menu/MenuItem](menu.md), [contentTracing](content-tracing.md), [app.commandLine](command-line.md), [app paths](app-paths.md).

```js
import {app, BrowserWindow} from 'firefly';
await app.whenReady();
const window = new BrowserWindow({width: 1000, height: 700});
await window.loadFile('index.html');
app.on('window-all-closed', () => app.quit());
```

## Compatibility and examples

- [Desktop feature examples and API dependencies](../examples/index.md)
- [Getting started](../getting-started.md)
- [Mite compatibility](../compatibility/mite-0.1.md)
- [Firefly compatibility](../compatibility/firefly-0.1.md)
- [Measured npm packages](../compatibility/npm-packages-0.1.md)
- [Implementation progress and remaining requirements](../PROGRESS.md)

Absent methods fail with ordinary JavaScript API/import errors. Implemented APIs
reject unsupported options rather than returning success placeholders. Error
messages and stack formatting are not generally identical to Node or Electron.

The [sandbox preload process reference](sandbox-process.md) describes renderer identity,
window argument/environment snapshots, limits and unsupported process capabilities.

The [webFrame reference](web-frame.md) covers native isolated-preload browser zoom,
factor/level conversion, rendering evidence and unsupported frame services.

The [native file input reference](file-input.md) describes the bounded chooser,
browser FileList data, cancellation and the still-missing native path association.

The [node:zlib reference](zlib.md) covers bounded native raw/zlib/gzip Transform streams,
ordered flush and convenience functions, with explicit unsupported features.

The [assert reference](assert.md) covers callable/strict assertions, supported deep
comparisons, error metadata and synchronous/asynchronous exception checks.

The [readline reference](readline.md) describes nonterminal UTF-8 line events and
async iteration; [FileHandle read streams](file-read-stream.md) document bounded
native file reads and descriptor ownership. The separate [file write-stream
reference](file-write-stream.md) documents native output, pressure/close/handle
ownership and explicit abort limitations; all seven local Node/Mite differential
groups and owned policy checks pass.

The [MessageChannelMain reference](message-channel-main.md) covers connected main
ports, copied binary data, queued ownership transfer and GC/resource bounds. Native
renderer and utility-process port delivery remains unavailable. The separate
[utilityProcess reference](utility-process.md) documents the real LibJS child/payload
implementation, six passing native boundary tests, local Firefly/Electron public lifecycle/payload acceptance and
explicitly unsupported native port transfers/authentication forwarding.

The [safeStorage reference](safe-storage.md) documents locally accepted macOS
OS-keychain/AES-256-GCM storage, Buffer serialization, isolated-test evidence,
Firefly-specific ciphertext and unavailable async/Linux/Windows providers.

The [WebContentsView reference](web-contents-view.md) covers native flat child-view
composition, stable contents identity, ownership/reparenting, explicit cleanup and
the webContents registry. Shared offscreen Firefly/Electron functional acceptance
and the same complete hidden production Cocoa fixture pass; on-screen/GPU/AX
acceptance and native Linux/XCB/Windows execution remain unverified.

The [desktopCapturer reference](desktop-capturer.md) describes asynchronous source
metadata, explicit zero-thumbnail options, permission boundaries and request
limits. Its macOS provider is implemented, while valid OS enumeration remains
untested; policy/controlled-dispatcher and no-prompt preflight evidence are scoped
separately. The [nativeImage reference](native-image.md) covers the verified
empty-image subset; nonempty images and media capture remain unavailable.

The [Notification reference](notification.md) covers owned generations, mutable
text/button metadata, events, replacement/close policy and native scheduling
limits. Node/Mite policy and bare-CLI read-only preflight evidence do not establish
OS permission or delivery acceptance; valid notifications remain untested.

The [powerMonitor reference](power-monitor.md) documents native read-only queries,
capability-gated passive events, listener/epoch ownership and conservative unknown
states. Node/Mite policy and local macOS query/start-stop evidence do not certify
physical power/session transitions; shutdown cancellation remains unsupported.

The [powerSaveBlocker reference](power-save-blocker.md) documents owned native
sleep/display inhibition requests, precedence, transactional failures, bounded
IDs and shutdown cleanup, separately from passive powerMonitor observations.

The [shell reference](shell.md) covers native URL/path dispatch, file reveal,
reversible OS Trash, promise/error differences and bounded asynchronous ownership.
Actual macOS Trash/restore is accepted locally; visible launch/reveal remains
unaccepted and native Windows/Linux providers are unavailable.

The [systemPreferences reference](system-preferences.md) covers native typed scalar
defaults, passive notifications, callback ownership and read-only media status.
It separates controlled tests, genuine read-only observations and isolated-suite/
local-notification acceptance from physical events and permission requests.

The [fork/JSON IPC contract](child-process.md#forkmodulepath-args-options) now
covers real self-children, native write completion/backpressure, child process APIs,
reference/disconnect/teardown rules and the runtime-only Firefly boundary. The
[exit notification contract](process.md#exit-notification) describes synchronous
cleanup and its remaining Node differences. Both have local actual runtime checks;
platform/package and full Code-OSS acceptance remain separately scoped.
