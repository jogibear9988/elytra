# Documentation baseline: Node.js and Electron

Reviewed 2026-11-06. Behavioral references remain **Node 24.15.0** and **Electron
45.2.3**, matching the repository's test references. Documentation coverage follows
our product specification and measured implementation; a reference chapter does not
make its corresponding API available here.

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
is the inventory baseline. Each family needs an explicit implementation decision and
documentation status. Current module-level detail is in the [Mite matrix](../compatibility/mite-0.1.md).

| Family | Current Mite scope | What remains to document/implement |
| --- | --- | --- |
| CLI, globals, process, console, timers | Partial | Full parameter/error reference; stdio, process events, signals, immediates, lifecycle and diagnostics |
| ESM, CommonJS, packages | Partial loader | Per-condition resolution/reference, named CJS exports, require(ESM), module tooling and unsupported loader modes |
| Buffer, text, URL, path, OS | Partial | Per-method contracts and native platform differences; general util and legacy URL gaps |
| Events and streams | Partial | Backpressure/lifetime recipes, state/error contracts and unsupported cancellation/Web Streams modes |
| Filesystem | Partial sync/callback/Promise/FileHandle APIs | Per-overload reference; streams/watch, permissions, metadata and remaining options |
| TCP, DNS, HTTP | Partial | HTTP clients, HTTPS/TLS, fetch, upgrades and wider protocol/timeout coverage |
| Crypto | Random, incremental Hash/HMAC and byte equality; [detailed current-subset reference](../api/crypto.md) | Ciphers, KeyObject/CryptoKey, asymmetric signatures, KDFs, Web Crypto and complete provider/error contracts |
| Child processes and concurrency | Unsupported | child_process, workers and necessary native lifetime/IPC behavior before documenting runnable examples |
| Testing/assertion and diagnostics | Unsupported built-ins | Decide practical assert/test, inspector, performance, tracing/reporting and async-context priorities |
| UDP, HTTP/2, compression, readline/TTY | Unsupported | Demand-led implementation and migration guidance |
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
| Preload / contextBridge / ipcMain / ipcRenderer | [Preload/publication](../api/context-bridge.md) and [bounded request/reply IPC](../api/ipc.md) references | One-way/sync/port/subframe IPC, full clone types and broader callback/value compatibility remain |
| dialog / Menu / MenuItem / Tray | Unsupported | Native implementations and practical application examples |
| clipboard / shell / screen / nativeImage / session | Unsupported | Native services, permission policies, platform differences and migration fixtures |
| Navigation / frames / protocol / web preferences | Narrow loading/defaults only | Frame/navigation policy, session/network controls and structured option reference |
| System integration | Unsupported | Prioritize notifications, shortcuts, themes, power/storage and utility services by application demand |
| Distribution / updates / crash diagnostics | Experimental macOS bundles only | Native Windows/Linux, signed distributions, installer/update behavior and supported diagnostics |
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
