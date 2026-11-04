# Implementation progress

The specification is `mite-firefly-agent-instructions.md`. Work proceeds on
`codex/mite-firefly` in resumable commits. This is a large implementation project;
unchecked steps are requirements still outstanding, not supported features.

## Milestones

- [x] 1. Inspect and pin Ladybird; record integration decisions and native Windows gaps.
- [x] 2. Build an actual LibJS-powered Mite executable and differential CLI tests.
- [ ] 3. Add modules (ESM/CJS/package resolution), globals, timers and filesystem.
- [ ] 4. Add asynchronous networking, HTTP, streams, crypto and process services.
- [ ] 5. Validate pure-JavaScript npm packages and publish measured compatibility.
- [ ] 6. Prove LibWeb rendering/process integration; implement native Windows adapters.
- [ ] 7. Implement Firefly lifecycle, native windows and file/URL loading.
- [ ] 8. Implement validated IPC, preload and context isolation with security tests.
- [ ] 9. Expand desktop APIs using Electron migration fixtures.
- [ ] 10. Native CI, distributable packages and smoke tests on all three platforms.

## Current work

Mite is a working, statically linked LibJS script host on macOS ARM64. It has a
CLI, basic console, process.argv, a rooted FIFO promise/microtask queue, rejection
reporting, and native libuv timers with cancellation and ref/unref support. ESM file loading is implemented; CJS/package resolution, built-ins and Firefly remain unimplemented.

Validation: native Debug build with Apple Clang 21 / Rust 1.98.0; all 41 runtime/module
checks pass (34 shared Node fixtures and 7 CLI checks). CLI example executed.
`otool -L` shows platform libraries only, with no Node, V8, LibWeb or Qt.
The upstream submodule remains unmodified. Windows/Linux remain unverified.

Local build used `build/debug` with the same settings as the documented `dev`
preset. Logs are in ignored `build/{configure,compile,test}.log`.

Milestone 3 partial progress: extracted EventLoop, installed queueMicrotask,
setTimeout/setInterval and Timeout handles; added ordering, cancellation, ref/unref,
invalid callbacks, exception and liveness differential fixtures. ADR 0003 records
ownership and the libuv choice. No full Node event-loop compatibility is claimed.

Milestone 3 ESM progress: native LibJS module graph with canonical URL cache,
relative/file URL resolution, package type selection, import.meta, live bindings,
cycles, dynamic import and top-level await. Unsettled top-level await exits 13.
Handled await rejections are filtered by the engine's authoritative handled bit.
14 module graphs and an async rejection regression fixture are compared with Node.

JSON modules now reuse LibJS synthetic module parsing and enforce type=json
attributes before consulting the module cache. Mutation/cache isolation, malformed
JSON and missing/incompatible type attributes are covered by differential tests.
`examples/mite-cli/modules.mjs` demonstrates JSON imports and timer-driven await.
Unknown import attribute names still lack Node's error code due to upstream's
pre-host validation. The static Rust archive duplicate-symbol warning recorded
in ADR 0004 also remains open before packaging.

Next: CJS and package resolution, followed by filesystem modules and globals.
Timer refresh, timeout warnings, immediates, nextTick and process events remain
tracked as unsupported in the compatibility matrix.

## Resume

1. Read this file, the specification, and `git status` / recent commits.
2. Initialize dependencies with `git submodule update --init --recursive`.
3. Follow the README build/test instructions, then take the next unchecked slice.
