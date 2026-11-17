# Documentation baseline: Node.js and Electron

Reference inventory reviewed 2026-11-07. Behavioral references remain **Node 24.15.0** and **Electron
45.2.3**, matching the repository's test references. Documentation coverage follows
our product specification and measured implementation; a reference chapter does not
make its corresponding API available here.

Reviewed against current code/native evidence on **2026-11-07**. See
[current support](../status.md) for exact platform/example/API status. Historical
requirements below remain a completion checklist, not a claim that all entries work.

## What the reference projects provide

Node separates its [learning material](https://nodejs.org/en/learn/getting-started/introduction-to-nodejs)
from a [versioned API manual](https://nodejs.org/download/release/v24.15.0/docs/api/index.html).
The useful pattern for Mite is a task-oriented learning path plus precise reference
entries, including CLI use, module loading, asynchronous work, errors and diagnostics.

Electron organizes its [documentation](https://www.electronjs.org/docs/latest/) around
an application tutorial, process concepts, best practices, examples, development,
distribution, testing/debugging and API reference. Its [BrowserWindow reference](https://www.electronjs.org/docs/latest/api/browser-window)
provides the level of signature, options, events and process/platform detail that
Firefly should approach as its implementations grow.

## Documentation coverage we should maintain

| Area | What we already have | Required addition / completion rule |
| --- | --- | --- |
| Introduction and installation | Source-build quickstart; renderer and Windows development notes | Separate first-use paths for runtime and desktop; requirements, expected output, supported host and upgrade procedure |
| Learning path | Runnable CLI/server/desktop examples | Explain modules, async/resource lifetime and process boundaries before advanced recipes |
| Runtime reference | One Mite overview and measured compatibility tables | Dedicated CLI/module/class entries with parameter types/defaults, return values, errors, async behavior and examples |
| Desktop reference | Detailed [app](../api/app.md), [BrowserWindow](../api/browser-window.md), [webContents](../api/web-contents.md) and [contextBridge](../api/context-bridge.md) entries | Keep method signatures, errors, cancellation and lifetime synchronized with each implemented change; add native services as they become available |
| Examples | Runtime/Designer examples and [23 desktop topic apps](../examples/index.md), six native shared slices, one manual theme gate and 16 Firefly API/platform blocks | Preserve per-topic source/API links, manual prerequisites and measured scope; reference-only code does not establish feature support |
| Migration | Compatibility matrices | Practical Node and Electron audits, import changes, package/feature checks and known blockers |
| Security | Detailed internal isolation ADRs and test gates | Application-author guidance describing actual controls and remaining gaps, plus the current bounded request/reply IPC contract |
| Distribution | Experimental macOS packager and gated release driver | App-author guide, complete native platform acceptance, dependency notices, signing/notarization and installation/update lifecycle |
| Testing/debugging | Native/differential scripts and build notes | A user-facing troubleshooting route, error lookup, bug-report evidence and supported debugging methods |
| Versions/stability | Pinned upstreams and versioned matrices | Explain support labels; record API changes and migration notes when behavior changes; do not fabricate release history |
| Navigation | Homepage, two API overview pages, flat documentation links | Documentation home, grouped navigation, page outlines and cross-links between guides, reference and evidence |
| Contribution | Specification, progress log, ADRs, upstream update procedure | State documentation acceptance rules and how new APIs update tests, reference, compatibility and examples together |

## Runtime API breadth to account for

The [Node 24 API index](https://nodejs.org/download/release/v24.15.0/docs/api/index.html)
is the inventory baseline; selected current behavior is also checked against
Node 24.22.3 with exact old-reference differences documented per API. Each family
needs an explicit implementation decision and
documentation status. Current module-level detail is in the [Mite matrix](../compatibility/mite-0.1.md).

| Family | Current Mite scope | What remains to document/implement |
| --- | --- | --- |
| CLI, globals, process, console, timers | Partial | Full parameter/error reference; stdio, broader signal support, lifecycle/rejection events and diagnostics. Custom events and restricted signal ownership have a detailed process contract |
| ESM, CommonJS, packages | Partial loader | Per-condition resolution/reference, named CJS exports, require(ESM), module tooling and unsupported loader modes |
| Buffer, text, URL, path, OS | Partial; [Buffer](../api/buffer.md) now documents the implemented byte/numeric surface and 3558-case local differential; [legacy URL](../api/url-legacy.md) has separate parser semantics | Remaining overloads/native platform differences; full util and namespace coverage |
| Events and streams | Partial | Backpressure/lifetime recipes, state/error contracts and unsupported cancellation/Web Streams modes |
| Filesystem | Partial sync/callback/Promise/FileHandle APIs | Per-overload reference; streams/watch, permissions, metadata and remaining options |
| TCP, DNS, HTTP/TLS/Fetch | Partial client/server subsets; genuine [HTTP Agents](../api/http.md), client upgrades and [WebSocket](../api/websocket.md) have bounded contracts | TLS/HTTPS servers, pooling/proxy breadth and wider protocol/timeout/platform parity |
| Crypto | Native randomness, Hash/HMAC, digest enumeration, byte equality, minimal X509 canonicalization and P-256 public-key verification; [current contract](../api/crypto.md) | Further key/signature algorithms, ciphers, KDFs, Web Crypto subtle and complete provider/error contracts |
| Child processes and concurrency | Native spawn/execFile/exec/fork, pipes, bounded IPC and lifetime have a [partial reference](../api/child-process.md); independent-VM [worker threads](../api/worker-threads.md) have focused lifecycle acceptance | Sync APIs, shared-memory/transfer breadth and broader option/error/platform parity |
| Testing/assertion and diagnostics | Partial [assert](../api/assert.md), [async context](../api/async-hooks.md), [diagnostics channels](../api/diagnostics-channel.md), [performance clocks](../api/perf-hooks.md) and inactive [inspector state](../api/inspector.md) | Test runner, active inspector/debugger, timeline/profiling and broader tracing/reporting |
| UDP, HTTP/2, compression, readline/TTY | Partial [zlib](../api/zlib.md), [DecompressionStream](../api/decompression.md), [readline](../api/readline.md), [TTY queries](../api/tty.md) and genuine owned [PTYs](../api/pty.md) | UDP/HTTP2, remaining compression formats, full terminal streams/raw mode and wider provider acceptance |
| Other Node facilities | Unsupported | Explicit decisions for SQLite, WASI, REPL, single-executable packaging and the remaining standard modules |
| V8 internals / native addons | Outside initial required scope | Preserve explicit limitations; no engine-specific success substitutes |

The inventory is broader than the initial compatibility target. It must remain clear
which entries are required by the specification, optional extensions or intentionally
outside the initial scope. See the [delivery roadmap](../roadmap.md).

## Desktop API breadth to account for

Electron's [API navigation](https://www.electronjs.org/docs/latest/api/app) distinguishes
main, renderer and utility-process modules plus classes and structures. Our reference
must label process availability rather than imply that all exports can run anywhere.
The [Firefly matrix](../compatibility/firefly-0.1.md) is the measured status source.

| Family | Current Firefly scope | Next acceptance/reference needs |
| --- | --- | --- |
| app / BrowserWindow / webContents | Partial main-process API with detailed per-member references | Broader API and native OS acceptance remain; document each new implementation with behavior and differential evidence |
| Preload / contextBridge / ipcMain / ipcRenderer | [Preload/publication](../api/context-bridge.md), bounded one-way/request-reply [IPC](../api/ipc.md) and genuine [message ports](../api/message-channel-main.md) have contracts | Sync/subframe IPC, full clone types, shared-memory/transfer breadth and broader callback/value compatibility |
| dialog / Menu / MenuItem / Tray | Partial native [dialogs](../api/dialog.md) and [application/Dock menus](../api/menu.md); Tray remains absent | Broader options/providers/platform acceptance and genuine Tray integration |
| clipboard / screen / nativeTheme | Partial macOS services; scoped X11 clipboard, new Linux screen wiring pending native acceptance | Physical/event/platform coverage and detailed references; [current status](../status.md) |
| shell / nativeImage / session | Scoped native [shell](../api/shell.md), [images](../api/native-image.md) and [session](../api/session.md) providers | Broader native/platform/permission support and full Electron semantics |
| Navigation / frames / protocol / web preferences | Owned loading, [frame identities](../api/web-frame-main.md), [custom protocols](../api/protocol.md), [webRequest](../api/web-request.md) and structured preference validation are partial | Broader navigation/subframe/network behavior and platform acceptance |
| System integration | Scoped native appearance, [notifications](../api/notification.md), [shortcuts](../api/global-shortcut.md), power and storage providers, plus [utility processes](../api/utility-process.md) | Full OS behavior and fresh per-provider Linux/Windows acceptance; exported APIs do not establish platform support |
| Distribution / updates / crash diagnostics | Published macOS Alpha 2; later optimized Actions artifact; experimental Linux archive/extracted Designer gates pass at 197cd2a | Native Windows and wider Linux acceptance, signed distributions, installer/update behavior and diagnostics |
| Chromium-specific or deprecated facilities | Outside initial promise | Record deliberate differences; do not copy deprecated surfaces merely to fill an inventory |

## Reference-entry requirements

Every implemented module/class page should state:

1. Availability and tested platforms; process/realm availability for Firefly.
2. Signature and parameter table with defaults, accepted types and supported options.
3. Return value, callback/Promise behavior and error codes.
4. Events with argument shapes, ordering, cancellation and destruction behavior.
5. Resource ownership, close/shutdown behavior and security restrictions.
6. Runnable example and exact verification source/command.
7. Known differences, unsupported modes and relevant decision record.
8. Version/change notes once a release or behavior change actually exists.

This is the reference-writing standard, not a statement that every current page has
reached it. Existing summaries remain useful navigation while detailed entries grow.
See [documentation maintenance](../development/documentation.md) for review gates.
