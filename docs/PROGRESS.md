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

## Upstream constraint

Ladybird must remain unmodified and regularly updatable. Integration changes stay
in our runtime/build/platform adapters. The manifest and gitlink are checked at
configure/build/test time, including rejection of local upstream source changes.
Runtime LibJS version metadata is generated from the manifest. See
`docs/development/updating-ladybird.md`. Seven isolated guard tests cover clean,
modified/staged/untracked, mismatched and uninitialized states. No newer upstream
candidate or native Windows/Linux update was validated in this step.

## Current work

Mite is a working, statically linked LibJS script host on macOS ARM64. It has a
CLI, basic console, process.argv, a rooted FIFO promise/microtask queue, rejection
reporting, and native libuv timers with cancellation and ref/unref support. ESM, JSON and CommonJS file loading are implemented; Node-style package resolution is implemented in part; the events, process, path and buffer builtins are implemented in part; other built-ins and Firefly remain unimplemented.

Validation: native Debug build with Apple Clang 21 / Rust 1.98.0; all 91 runtime/module/npm
checks pass, plus 3,968 lexical path comparisons and a path identity/live-cwd scenario (80 shared Node fixtures, 7 CLI checks, 3 explicit unsupported-API
checks and 1 Mite runtime identity check). Behavioral reference: Node v24.15.0. CLI example executed.
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

CommonJS file execution now implements the wrapper, require/resolve/cache/main,
metadata, cycles, directory main/index resolution and cache rollback after errors.
14 additional Node fixtures cover these behaviors, BOM/shebangs, alias replacement,
ordering, lazy ESM interoperation and shared JSON values. GC-owned source metadata
replaces permanently rooted wrapper records, allowing cache invalidation without
retaining all previous scripts. ADR 0005 records ownership and remaining gaps.

Package resolution now supports bare/scoped node_modules lookup, self references,
exports/imports maps, insertion-ordered conditions, exact/pattern priority, arrays,
legacy main/index and module.paths values. Temporary package trees compare both
loaders against Node, including error names/codes for invalid/blocked targets,
encoded traversal/separators, malformed configs and nested package shadowing.
The 2 built-in reservation checks are explicit unsupported behavior, not claims of
working fs APIs. ADR 0006 records condition/recursion limits and remaining gaps.

The builtin registry now shares CommonJS exports with fixed named ESM shims,
canonical node: identities and prefix cache bypass. node:events implements core
EventEmitter behavior and Promise once; 12 new differential fixtures exercise it.
An additional fixture checks explicit unsupported captureRejections/AbortSignal
modes. Warnings currently have simplified formatting/microtask scheduling;
EventTarget, async event iterators/hooks and syncBuiltinESMExports remain absent.
ADR 0007 records the JS/C++ boundary and limits.

Milestone 5 partial progress: standard npm ci installs four locked published
packages (once 1.4.0, wrappy 1.0.2, escape-string-regexp 5.0.0, eventemitter3 5.0.1)
with scripts disabled and integrity/license metadata retained. Six real-package
fixtures pass in Mite and Node, including transitive dependencies and conditional
ESM/CJS entry identity. Missing/wrong versions or license files fail the suite.
The package matrix records exact measured scope; no full upstream test-suite or
general npm compatibility claim is made. The published once.proto absence is
observed under both runtimes and documented rather than patched.

Native process basics now provide cwd/chdir, live OS environment variables, IDs,
platform/architecture, executable path/argv0, exitCode and immediate exit. Global,
CommonJS and ESM process identity is shared even if the global alias is replaced.
Eight new Node fixtures and an explicit Mite identity check pass. ADR 0008 records
native ownership, environment behavior, exit semantics and unsupported lifecycle
APIs. Native Linux/Windows validation remains open.

Lexical node:path now provides POSIX/Windows operations and shared path/posix and
path/win32 aliases. All 3,968 differential call comparisons and an ESM identity /
live-cwd scenario pass. ADR 0009 records scope; matchesGlob remains absent and
native Windows drive/cwd behavior is unverified. Nine CTest suites pass, including the upstream integrity gate and its seven isolated regression tests.

Buffer foundation now uses LibJS Uint8Array/ArrayBuffer storage with global/builtin
identity, shared views, encodings and memory/search operations. The initial 2,170 Buffer
comparisons pass, including malformed UTF-8 boundaries and overlapping copies.
Ten CTest suites pass. ADR 0010 records the zeroed unsafe-allocation policy, size
limit and pending Blob/File APIs. Ladybird remains unmodified.

Buffer numeric access now covers fixed/variable-width signed and unsigned
integers, floating point and 64-bit BigInt with both endian orders. The Buffer
corpus now has 3,557 shared comparisons plus an explicit allocation-policy check.
Encoded bytes, error codes, numeric limits and subview offsets match Node.
A follow-up regression covers base64 decoding of UTF-16 surrogate pairs: the
forgiving decoder now processes every code unit, matching Node low-byte behavior.

Next: filesystem, then networking/server and renderer
vertical slices. Broaden the real-package
corpus as those APIs become available. Filesystem, further Buffer and process services precede the
networking and renderer slices. CommonJS named-export detection, synchronous
require(ESM), eval CJS globals and require.resolve options remain unsupported.
Timer refresh, timeout warnings, immediates, nextTick and process events remain
tracked as unsupported in the compatibility matrix.

## Resume

1. Read this file, the specification, and `git status` / recent commits.
2. Initialize dependencies with `git submodule update --init --recursive`.
3. Follow the README build/test instructions, then take the next unchecked slice.
