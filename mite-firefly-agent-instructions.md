# AGENTS.md — Mite and Firefly

## Mission

Build two production-oriented, open-source projects:

1. **Mite**: a standalone, Node.js-compatible JavaScript runtime powered by Ladybird's **LibJS**, without embedding Node.js or V8.
2. **Firefly**: a native, Electron-compatible desktop application framework powered by **Mite** for the main process and Ladybird's **LibWeb/LibJS** for rendering.

Both projects must target **Windows, macOS, and Linux natively**. Windows support must not require WSL, and macOS/Linux must not require a virtual machine. Deliver functioning software, not just architecture documents or stubs.

## Primary design principles

- **Compatibility first, but not at any cost.** Preserve existing Node.js and Electron APIs wherever feasible. Favor matching module names, exports, signatures, event names, asynchronous behavior, error shapes, and semantics over inventing new APIs.
- **Easy migration is a primary success criterion.** A simple Electron app should ideally migrate by changing its package dependency/import from `electron` to `firefly`, with minimal other changes.
- **Not 100% compatibility.** Unsupported or deliberately different behavior is allowed when justified by architecture, security, performance, or implementation cost. Document every important difference and provide migration guidance.
- **Independent runtime.** Mite must run CLI and server applications without Firefly, LibWeb, or any graphical environment.
- **Reuse rather than rewrite.** Integrate existing Ladybird/LibJS/LibWeb components and their upstream improvements where possible.
- **Native and secure.** Preserve process isolation, avoid exposing privileged APIs to untrusted renderer content, and make platform-native packaging a core requirement.
- **Working vertical slices.** Prioritize a runnable, tested end-to-end implementation over a large collection of empty API placeholders.

## Language policy: Rust and C++ are both allowed

The coding agent must choose the best language **per subsystem** based on real engineering constraints, not an arbitrary Rust-first or C++-first policy.

- **C++** is appropriate for direct integration with LibJS, LibWeb, Ladybird's existing C++ code, and APIs where avoiding an FFI layer improves maintainability.
- **Rust** is appropriate for systems components, platform services, packaging, tooling, and components where its safety and ecosystem offer concrete benefits.
- Mixed Rust/C++ designs are welcome, but avoid gratuitous FFI boundaries, duplicated ownership models, or excessive build complexity.
- Ladybird contains components written in different languages; inspect the actual upstream repository before deciding how to integrate them.
- Do not rewrite stable upstream C++ or Rust code merely to standardize on one language.
- Record significant language decisions and their tradeoffs in architecture decision records (ADRs).

## Required architecture

```text
Application
├── Main process: Mite
│   ├── LibJS JavaScript engine
│   ├── Node-compatible APIs and module loader
│   ├── Event loop and asynchronous I/O
│   └── Native operating-system services
├── Firefly desktop API
│   ├── app / BrowserWindow / webContents
│   ├── ipcMain / ipcRenderer / contextBridge
│   ├── dialog / Menu / Tray / clipboard / shell
│   └── platform window management and packaging
└── Renderer processes: Ladybird
    ├── LibWeb + LibJS
    ├── HTML / CSS / DOM / Web APIs
    └── constrained IPC bridge to the main process
```

Mite must never depend on Firefly. Firefly can depend on Mite and Ladybird. Keep privileged APIs in the main process and communicate with renderer processes over validated IPC.

## Mite: Node.js compatibility requirements

Implement a practical Node-like runtime on LibJS, not Node.js itself. Use Node's public API documentation and observable behavior as the reference. Initially prioritize:

- ES modules, `package.json`, `node_modules`, package exports/imports, and standard Node module resolution.
- CommonJS `require`, `module.exports`, and ESM/CJS interoperability where practical.
- Globals: `process`, `Buffer`, `console`, timers, `queueMicrotask`, `URL`, `TextEncoder`, `TextDecoder`, and `fetch`.
- Built-ins: `node:path`, `node:url`, `node:os`, `node:events`, `node:util`, `node:buffer`, `node:fs`, `node:fs/promises`, `node:stream`, `node:crypto`, `node:net`, `node:http`, `node:https`, and `node:child_process`.
- Correct event-loop behavior, microtasks, promise rejection handling, and asynchronous I/O.
- Compatibility with ordinary pure-JavaScript npm packages.

Native Node addons, N-API, V8 internals, `process.binding()`, and obscure Node internals are not mandatory initially. Explicitly report unsupported APIs instead of silently pretending they work.

Support existing npm project structures and workflows where possible; do not build a package manager until there is a clear need. A useful initial CLI is `mite app.js`.

## Firefly: Electron compatibility is a PRIMARY requirement

**Treat Electron's documented public API as the preferred API design for Firefly.** This is a central project requirement, not a stretch goal.

- Preserve familiar APIs and names: `app`, `BrowserWindow`, `webContents`, `ipcMain`, `ipcRenderer`, `contextBridge`, `dialog`, `Menu`, `MenuItem`, `Tray`, `clipboard`, `shell`, `screen`, `nativeImage`, `session`, and others as justified by demand.
- Match Electron's method signatures, options, return values, events, error behavior, and promise behavior where practical.
- Support Electron-style application entry points and lifecycle (`app.whenReady()`, `app.on('ready', ...)`, `window-all-closed`, etc.).
- Support `BrowserWindow.loadFile()`, `loadURL()`, window controls, preload scripts, secure IPC, and renderer isolation early.
- Prefer Electron-compatible preload and `contextBridge` patterns. Do not expose filesystem/process capabilities to web content by default.
- Design an import compatibility strategy so applications can ideally replace `electron` with `firefly` (or use a documented compatibility alias) without rewriting their architecture.
- Do not claim that an Electron feature works until tested. Maintain a versioned compatibility matrix with `compatible`, `partial`, `unsupported`, and `intentional difference` statuses.
- Avoid inventing replacement APIs when an existing Electron API can reasonably be implemented.
- Full Electron compatibility is **not** required: Chromium-specific features, Chrome extensions, V8-specific features, and expensive edge cases may be postponed or omitted with explicit documentation.

Example migration target:

```diff
-import { app, BrowserWindow } from 'electron';
+import { app, BrowserWindow } from 'firefly';
 import fs from 'node:fs';

 app.whenReady().then(() => {
   const win = new BrowserWindow({ width: 1200, height: 800 });
   win.loadFile('index.html');
 });
```

The unchanged `node:fs` import should work in Mite, while the desktop API should behave as closely to Electron as feasible.

## Cross-platform support

Native builds, windows, menus, dialogs, process spawning, filesystem access, networking, IPC, and distributable application packages are required on:

- Windows (native Windows executable; no WSL runtime requirement)
- macOS (native application bundle)
- Linux (native executable and appropriate distributable packaging)

Investigate Ladybird's actual platform support and embedding limitations at the beginning. Do not assume its existing browser UI can simply be embedded on all three platforms. If upstream lacks a Windows backend, define and implement a realistic Windows integration strategy. Use conditional compilation and small platform adapters rather than scattering platform checks throughout the codebase.

## Repository structure (suggested, not mandatory)

```text
/
  AGENTS.md
  docs/
    architecture/
    compatibility/
    migration/
  mite/
    runtime/
    modules/
    cli/
    tests/
  firefly/
    core/
    native/
    renderer-bridge/
    packaging/
    tests/
  examples/
    mite-cli/
    mite-http/
    firefly-hello-world/
    electron-migration/
  tooling/
  .github/workflows/
```

Use a monorepo initially if that makes coordinated development easier. Maintain strict architectural separation so Mite can later be distributed independently.

## Engineering workflow for AI coding agents

1. Inspect the current upstream Ladybird, LibJS, and LibWeb source, build system, licenses, embedding interfaces, and OS support. Verify facts rather than relying on assumptions.
2. Write a concise architecture and ADRs covering language choices, ownership/FFI, event loop, module loading, renderer isolation, IPC, and cross-platform integration.
3. Implement the smallest running Mite executable using LibJS, then add ESM loading, timers, filesystem, and Node-like globals.
4. Create behavioral tests that run against both Node.js and Mite; compare results, exceptions, and async ordering.
5. Validate real pure-JavaScript npm packages and record compatibility honestly.
6. Establish a Ladybird renderer embedding/protocol proof of concept, including Windows feasibility, before committing to a large Firefly API surface.
7. Implement a working Firefly `app` + `BrowserWindow` + `loadFile` vertical slice, followed by IPC/preload/context isolation.
8. Expand Node and Electron API coverage according to real-world migration examples and test results.
9. Add native build, CI, packaging, and smoke tests for all three platforms.
10. Continuously update compatibility matrices, limitations, architecture notes, and migration examples.

## Testing and acceptance criteria

- Automated unit, integration, and end-to-end tests; CI on Windows, macOS, and Linux.
- Node differential tests for supported Mite APIs, including async ordering and error behavior.
- Electron migration fixtures that run in Electron and Firefly with as few changes as possible.
- A working Mite CLI example, a small HTTP server example, and a Firefly desktop application.
- Explicit coverage for security boundaries, malicious renderer inputs, and IPC validation.
- No fake green checkmarks: incomplete implementations must be marked incomplete, with failing/skipped tests tracked.
- Keep upstream dependencies pinned/reproducible and preserve applicable third-party license notices.

## Execution expectations

Make progress autonomously and implement code rather than only proposing designs. If a component is blocked by upstream limitations, isolate the blocker, document a concrete workaround, and continue on independent work. Do not replace Ladybird/LibJS with Chromium, V8, or Node.js as a shortcut; that would violate the project's core purpose. Prioritize compatibility, correctness, maintainability, security, and native cross-platform behavior over premature optimization or superficial API coverage.
