# Implementation progress

The specification is `mite-firefly-agent-instructions.md`. Work proceeds on
`master` in resumable commits. This is a large implementation project;
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

## Current scope and acceptance

The detailed dated entries below supersede the initial historical baseline.
Mite is a working native LibJS runtime. The dated entries record exact source
revisions, build types and suite totals for accepted and candidate validation.
Native Linux/Windows CLI baselines have
passed hosted CI; this does not validate each subsequent change on those platforms.
Each new change still needs its own fresh platform acceptance. Mite remains
independent of Firefly, LibWeb and graphical libraries.

Firefly has a partial working macOS desktop runtime and an existing macOS ARM64
alpha release. At 197cd2a the experimental native Linux renderer/helper/window,
screen/clipboard/kernel, stripped archive/ELF provenance and extracted bundled
Designer gates pass. This is Ubuntu 24.04 x64 X11 Xvfb/Openbox acceptance;
broader Linux distribution/theme/installer support and native Windows desktop
remain required and unfinished.
The milestone checklist above remains open; scoped component success does not
complete those cross-platform requirements. See the latest entries for exact
source revisions, evidence, limits and live candidates.

## Initial runtime baseline (historical)

Mite is a working, statically linked LibJS script host on macOS ARM64. It has a
CLI, basic console, process.argv, a rooted FIFO promise/microtask queue, rejection
reporting, and native libuv timers with cancellation and ref/unref support. ESM, JSON and CommonJS file loading are implemented; Node-style package resolution is implemented in part; the events, process, path, buffer, filesystem, stream, string_decoder, net, dns, os, crypto and HTTP server builtins are implemented in part; other built-ins and remaining Firefly services are unimplemented.

Validation: native Debug build with Apple Clang 21 / Rust 1.98.0; all 35 CTest
suites pass. This includes 110 runtime/module/npm/tick checks (101 shared Node
fixtures, 7 CLI, 1 unsupported-API fixture and 1 Mite identity check), 3,968 path
comparisons plus identity/live-cwd, 3,558 Buffer comparisons plus allocation
policy, 16 filesystem workflows plus 3 explicit policy fixtures, 1,013 URL comparisons
and 342 URL helper comparisons, 1,657 encoding comparisons, 449 StringDecoder
comparisons, 7 shared stream workflows and 1 recorded stream ordering difference,
14 shared TCP workflows, 63 net validation comparisons and an independent TCP peer,
6 shared DNS/TCP workflows, 119 DNS validation comparisons and a recorded DNS NUL
policy difference, 11 shared HTTP workflows, 135 HTTP header/constant comparisons,
2 recorded outgoing HTTP policy differences, independent HTTP-client requests,
the checked-in runtime-library integrity gate, and the upstream
integrity gate with 7 isolated guard tests. Behavioral reference: Node v24.15.0.
CLI and filesystem-copy examples executed.
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
The 2 built-in reservation checks now verify implemented fs wins over a local
npm shadow; they previously covered explicit unsupported behavior. ADR 0006 records condition/recursion limits and remaining gaps.

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
corpus now has 3,558 shared comparisons plus an explicit allocation-policy check.
Encoded bytes, error codes, numeric limits and subview offsets match Node.
A follow-up regression covers base64 decoding of UTF-16 surrogate pairs: the
forgiving decoder now processes every code unit, matching Node low-byte behavior.

Filesystem now provides native whole-file I/O, access/exists, stat/lstat,
mkdir/rmdir, unlink/rename and readdir, with synchronous/callback/Promise forms.
Native workers own their byte snapshots; JS completions are rooted and run on the
VM thread with microtask checkpoints. Nine differential workflows, two explicit
fatal-FIFO exit policy tests and one unsupported-options fixture pass. A FIFO /
timer test proves nonblocking native I/O; inherited descriptor tests verify
borrowed ownership and positions. TypedArray copies use the native engine path.
Eleven CTest suites pass. ADR 0011 records intentional differences (including fatal
termination with pending work), limitations and the exact tested platform.

Scalar descriptor APIs now add open/close/read/write, fstat/ftruncate and
fsync/fdatasync in synchronous/callback forms. Three new differential workflows
check byte views, optional arguments, exact BigInt read positions, stream versus
positioned access, zero-length I/O, native errors and final bytes. The filesystem
corpus now has 12 shared workflows plus the existing three explicit policy cases.
All eleven CTest suites pass. Numeric write offsets beyond the safe-integer range
are explicitly unsupported.

FileHandle now implements Promise open, scalar/whole-file operations, metadata,
sync/truncate, close events and async disposal. Per-handle active references defer
native close until pending operations finish; a private native guard owns the fd
until release. Three new Node workflows verify lifecycle timing, pending/new
operations during close, repeated close and descriptor reuse. The filesystem
corpus now has 15 shared workflows plus 3 explicit policy fixtures; all eleven
CTest suites pass. ADR 0012 records the unverified forced-GC fallback and remaining
FileHandle APIs. Ladybird remains unchanged.

The locked real-package corpus now includes @nodelib/fs.stat 2.0.5 (MIT). Two
additional published-code workflows pass for sync/callback filesystem adapters,
Settings reuse, symlink follow/mark behavior and broken/missing entries. The
package suite now has 8 scenarios across 5 locked packages; integrity and license
checks remain enforced. No dependency code or Ladybird source was modified.

URL and URLSearchParams globals now reuse unmodified LibURL parsing, origin,
setters and form encoding/decoding through our own adapter. A 1,013-comparison
Node suite covers opaque/file/blob/custom URLs, IDNA/IP hosts, live query lists,
iterator behavior and synchronization with URL setters. All twelve CTest suites
pass. ADR 0013 records scope and the still-missing Node URL helpers; no LibWeb
or upstream patch was introduced.

Node URL helpers now add string/byte file paths, pathToFileURL, IDNA conversion
and HTTP options. All 342 helper comparisons pass, including lexical Windows
UNC/namespace cases. File URLs work through fs synchronous, Promise and FileHandle
APIs with scheme/separator/NUL checks before native I/O. A real filesystem URL
workflow passes; the filesystem suite now has 16 shared workflows and 3 policy
fixtures. All thirteen CTest suites pass. Native Windows/Linux remains unverified;
no upstream changes were needed.

TextEncoder/TextDecoder now share global and node:util identity. Native UTF-8
encoding/encodeInto and streaming decoding use LibTextCodec, with the existing ICU
backend for five legacy encodings whose mappings/error timing differ in Node.
All 1,657 differential comparisons pass, including fatal recovery, BOMs, byte-view
boundaries and detached/shared/resizable buffers. Fourteen CTest suites pass.
ADR 0014 records the backend policy, native ownership and remaining stream/util
APIs. Ladybird source remains unmodified.

The real npm corpus now also includes @msgpack/msgpack 3.1.2 (ISC), with two new
published-code workflows. Long Unicode strings demonstrably call the native
encodeInto/decode methods; binary, BigInt, timestamp, async chunk/multi/array
formats and invalid input guards match Node. The corpus now has 10 scenarios
across 6 locked packages, with integrity/license checks and no dependency patches.

process.nextTick now uses a separate rooted FIFO that drains before microtasks
at CJS/native boundaries and after the in-flight microtask queue during ESM/job
execution. Fifteen new Node fixtures cover CJS/ESM order, await optimizations,
nested queues, arguments, native callbacks, handled rejections and thrown ticks.
All fifteen CTest suites pass. ADR 0015 records the queue policy and remaining
Node event-loop phases.

Portable streams now embed pinned readable-stream 4.7.0 and string_decoder 1.3.0
sources through our adapters, without a Node/V8 runtime or upstream edits. Seven
Node workflows cover buffering, decoding, backpressure, writev, Promise pipelines,
isolated errors, async iteration and identity; 449 StringDecoder comparisons pass.
One separate concurrent-error fixture records different relative error ordering
from Node 24, also reproduced by the portable library under Node's scheduler.
All eighteen CTest suites pass. Exact bundle regeneration and the pipeline example
were validated. License texts and source/integrity metadata are retained; every
native build checks artifact/input hashes without requiring Node/npm. ADR 0016
and tooling/runtime-js/README.md describe limits and updates. Filesystem/stdio
streams, WHATWG adapters and broad helper coverage remain open.

Native TCP now provides numeric IPv4/IPv6 node:net Socket/Duplex and Server
facades over the shared libuv loop. Fourteen Node workflows cover byte transport,
backpressure, half-close, buffering, errors, close waiting, cancellation, ref/unref,
timeouts and callback queue checkpoints; 63 address/validation comparisons pass.
An independent Python socket peer verifies both runtimes; fatal callback exits
and nine unsupported API cases are checked explicitly. All twenty CTest suites
pass. The TCP echo example was tested with an external UTF-8 client. Console lines
now flush when piped so long-running servers can publish readiness immediately.
ADR 0017 records native ownership/shutdown, copied writes, queued byte-count limits,
root retention and numeric-host-only scope. Ladybird remains unmodified; Windows
and Linux remain unverified. DNS, IPC transports, local bind/family selection and
several advanced socket policies remain open.

OS-backed asynchronous dns.lookup now has callback/Promise forms, family/all/hints,
IDNA normalization, ordering controls and shared builtin identity. TCP resolves
hostnames (including default localhost), supports custom lookup and bounded
alternating-family attempts with retry events and aggregate errors; listen can
resolve hostnames and close safely during lookup. Six shared workflows and 119
validation comparisons pass, along with fatal/pending resolver checks and a
POSIX FIFO scenario proving a queued OS lookup leaves timers responsive. A
separate fixture records deliberate NUL rejection instead of Node's prefix
resolution. All twenty-two CTest suites pass. ADR 0018 documents request ownership,
fatal-exit accounting, partial retry coverage, absent record-query services and
unverified remote DNS/native Windows/Linux behavior. The existing TCP suite now
has eight unsupported-option cases, since hostname/custom lookup/family support
replaces earlier exclusions. Ladybird remains unmodified.

A working HTTP/1 server now layers IncomingMessage/response streams and Server
on native TCP with pinned llhttp 9.3.1. Eleven shared workflows cover HTTP/1.0/1.1,
GET/POST, fixed/chunked bodies and trailers, HEAD/204, continue, keep-alive,
pipelined response order, malformed framing, large delayed uploads, client FIN
and default errors. All 135 header/constant comparisons pass; two separate
fixtures retain stricter outgoing header/framing outcomes than Node. Independent
Python clients complete six requests on one connection under both runtimes; fatal
request callbacks and Mite deadline/unsupported policies are tested explicitly.
All twenty-five CTest suites pass. examples/mite-http/server.mjs was exercised
with GET and a piped binary POST. The llhttp source/version pin and MIT notice are
retained, versions.llhttp identifies it, and otool still shows platform libraries
only. ADR 0019 records ownership, deadlines and remaining gaps: HTTP clients,
Agent/TLS/fetch/upgrades, aggregate pipeline pressure, chunk-extension limits and
full lifecycle/header parity. Native Windows/Linux remain unverified. Ladybird
remains unmodified.

The first isolated renderer embedding probe builds and runs on native macOS
ARM64 (macOS 27.0.1, Apple Clang 21, Rust 1.98.0). All 76 pinned browser dependency
packages and the complete 3,224-step upstream/helper/probe build completed. Our
CMake project/deferred-include hook leaves Ladybird and the Mite graph unmodified.
The adapter skips development install rules, directly links its IPC/URL symbols,
and passes the companion executable's directory for helper/resource lookup.
These fixes are all outside the submodule.

`tooling/tests/test_renderer.py` passes all three real runtime checks: LibWeb text with
LibJS DOM mutation, absent process/require/Buffer/Mite hooks and rejected node:fs;
a 640x400 compositor PNG; and distinct WebContent/RequestServer/Compositor OS
children, including the navigated renderer PID. The image was visually reviewed:
heading, paragraph and both successful DOM/capability markers paint correctly.
The process observer covers the full session because upstream can launch a spare
WebContent before the navigated renderer. Temporary profiles and default helper
sandbox flags are retained; these selected checks do not prove the full sandbox
or IPC attack surface. Logs: build/renderer-build.log (initial missing-link failure),
build/renderer-rebuild.log (successful final build), build/renderer-test.log
(passing checks). Screenshot: build/renderer-artifacts/page.png. Generated artifacts
are not committed. The unchanged standalone Mite still links only platform
libraries, with no LibWeb/Qt/Node/V8 dependency. Its existing 25 suites passed in
the preceding HTTP commit; renderer checks are separate, not counted among them.

This headless probe is not a Mite-driven Firefly application, preload bridge
or public Electron API. Native Windows/Linux remain unverified, and the pinned
Windows renderer sandbox is a no-op requiring an actual platform implementation.
The browser dependency link emits newer macOS deployment-target warnings; running
on this host establishes no older-macOS support or distributable packaging claim.
ADR 0020 and docs/development/renderer-build.md record boundaries and reproduction.

The next native macOS gate now builds a Firefly-owned QWidget window using
Ladybird's actual, unchanged WebContentView and Qt event-loop sources. Native
IOSurface/Core Animation presentation, input translation, resize and accessibility
code are reused directly from the pin. A small context-menu adapter avoids the
upstream menu callback's dependency on its tabbed browser Application. Context
menus currently take a snapshot with weak action references; live menu updates,
shortcuts/icons and public Electron Menu behavior remain unverified.

`check-firefly-window` passes the native fixture: Qt-injected mouse/key events reach
LibWeb, DOM confirms CLICK_OK/KEY_OK and an 800x500 CSS viewport, a second local
navigation executes JS, and closing the real window ends the loop. The native
QScreen capture was visually reviewed at 1600x1000 pixels on this HiDPI host and
shows the button, typed a and all three success markers. A separate run identifies
the loaded WebContent as an OS child, kills it and requires an explicit crash
error and exit code 1. This is real native presentation with toolkit-injected
input, not proof of all OS input/IME/accessibility/beforeunload behavior. Logs:
build/window-build.log and build/window-target-test.log; image:
build/renderer-artifacts/window.png. CLI help and missing-binary failure are also
checked. The existing headless renderer gate still passes. All generated outputs
remain ignored. Deployment-target linker warnings and nonfatal teardown/host task
policy messages remain; no older-macOS or distribution claim follows.

The window target is enabled only on macOS, with Windows/Linux dependency wiring
and runtime/security gates outstanding. No Mite code changed in this slice; its
25 suites were not rerun. Public app/BrowserWindow, Mite GUI event-loop integration,
preload and contextBridge remain unsupported. ADR 0021 records the adapter seam,
verification scope and next integration steps. Ladybird remains unmodified.

The runtime is now extracted into a reusable static mite-runtime target and
Mite::run. The CLI and external hosts use the exact same initialization, module
loader and native services. HostHooks initialize bindings before entry execution,
let a host drive the loop, and disconnect callbacks before realm teardown. The
nonblocking pump_once keeps existing tick/promise/rejection checkpoints; host-owned
window lifetime is deliberately separate from libuv liveness. A shared CMake
function generates the runtime sources/metadata and accepts the appropriate
upstream JS target, avoiding an accidental second runtime in a future LibWeb host.
No Firefly/LibWeb/Qt dependency was added to standalone Mite; otool still lists
only platform libraries. Existing static Rust archive duplicate-symbol linker
warnings remain (ADR 0004).

The external embedding fixture tests real CommonJS streams, ticks/promises,
timers and exit behavior against the CLI; ESM async filesystem completion; and
an external rooted callback settling top-level await while libuv is idle. It
also verifies exactly-once shutdown after partial initialization failure, native
callback exceptions, unhandled rejections and a fatal callback while an actual
POSIX FIFO worker is blocked, and after initialization itself queues blocking
work then fails before entry execution. Initial test errors using require in eval mode were
corrected to actual CommonJS files, with explicit expected exit/output assertions.
The embedding gate and all 26 CTest suites pass in the final native regression;
logs are build/test.log and build/embedding-test.log. The native Qt executable has not
yet been connected to this hosting API: public app/BrowserWindow integration remains required. ADR 0022 records ownership and remaining integration.
Windows/Linux hosting remains unverified; Ladybird is still unmodified.

Native hosts can now register actual realm-owned module exports in the loader,
without adding desktop names to Mite's builtins. Exact bare names and package
imports aliases resolve to shared CJS/ESM objects; ESM provides default plus a fixed
named-export list and cached namespace. Node builtins remain reserved, and host
registrations do not create node: aliases. Internal host URLs/bridge access from
application modules are rejected. Registered objects are rooted until loader
teardown, and require.cache cannot replace them. Registration validation rejects
invalid/reserved names, duplicates and unsafe export names atomically; reserved
word exports use quoted aliases. ADR 0023 records the native API's ASCII/unscoped
name limits and named-property snapshot semantics.

The new host-module integration gate passes with real CJS/ESM files, conflicting
node_modules packages, aliases, shared identities, callbacks, default-only and
reserved-word exports, cache poisoning attempts, explicit GC rounds, invalid
attributes, missing exports, private imports and node: alias rejection. It also
proves host-only modules are absent in standalone Mite, where ordinary packages
still load. The existing embedding gate passes. All 27 CTest suites pass in the final native
regression; logs are build/host-modules-test.log and
build/test.log. Standalone linkage still contains only platform libraries; no
Firefly/Qt/LibWeb dependency was added. Ladybird remains unmodified. These test
host modules are internal fixtures, not an implemented firefly API or renderer
bridge. Native Windows/Linux remain unverified.

The first actual Firefly application now runs on the shared Mite VM with native
macOS Qt/IOSurface windows and the real Ladybird WebContent/RequestServer/
Compositor processes. The firefly host module provides app and BrowserWindow to
both CJS and ESM. It uses Mite's same events/path/url builtins, not a second JS
runtime or another web engine. The API covers asynchronous readiness, basic
window registry/controls, loadFile/loadURL promises, cancellable close and explicit
quit. Unknown options and nondefault security/preload requests fail explicitly.
The current subset and remaining gaps are in docs/compatibility/firefly-0.1.md.
examples/firefly-hello-world/main.cjs is a runnable native application.

A shared lifecycle/renderer fixture passes in native Electron 45.2.3 and Firefly:
ready, did-finish-load before load Promise resolution, cached webContents identity,
renderer capability absence and process PID, native size/title/show/hide, a first
cancelled close and the complete closed/window-all-closed/quit event sequence.
The fixture caches webContents before closed because Electron does not allow
retrieving it through the destroyed window. A separate static-ESM entry uses
actual Mite fs/HTTP services to serve and load its page. Additional native tests
pass for missing files, refused connections without a deadline fallback, unsupported
security/options, an owned WebContent OS process killed by the harness, fatal main
callbacks and normal quit with a blocked FIFO worker. Electron is a pinned test
reference under ignored build/, not a linked/runtime dependency. Its installer and
npm lockfile are checked in under tooling. Logs: build/firefly-target-test.log,
build/firefly-test.log and build/electron-bootstrap.log.

Core GUI callbacks enter Mite's dispatch/checkpoint boundary. Nested pumps are
blocked, and external GUI callbacks defer while a Mite turn is active; the added
embedding regression verifies ticks/promises do not run inside a nested pump.
All 27 Mite CTest suites pass (build/test.log). Standalone Mite remains independent
of Firefly/Qt/LibWeb; Firefly links the appropriate single LibJS copy alongside
LibWeb. The initial GUI driver polls every millisecond and needs an idle-efficient
backend. A host-requested stop with pending native work uses _Exit after host
shutdown; later C++/temporary-profile cleanup on that forced path remains a gap.

A generated dependency manifest adds libuv and pinned llhttp outside the upstream
checkout. Configure/build guards reject stale or modified generated manifests,
requiring regeneration after input/pin changes. Qt view sources are compiled once
in our shared adapter. Ladybird and both upstream manifests stay unmodified.
No newer upstream candidate was validated. Existing Rust duplicate-symbol and
newer-macOS deployment-target warnings remain; no older-system or distribution
support follows from this development build.

The headless gate passes after integration. The native window gate currently
fails at capture, and visual review of the new example is still open: authoritative
IORegistry state reports IOConsoleLocked=true and CGSSessionScreenIsLocked=true.
WindowServer lists the Mite-driven window but not as onscreen. No replacement
screenshot or false pass is recorded. The earlier native window/input proof remains
historical; rerun the visible gate and review a fresh image after unlocking.
ADR 0024 records ownership, API references and these limits. Public preload/IPC,
Windows/Linux adapters/security, OS lifecycle parity, most desktop APIs and
packaging remain outstanding; the overall milestones are not complete.

Firefly lifecycle coverage now includes matching Electron 45.2.3 fixtures for
cancelling before-quit, close and will-quit, retrying after cancellation and creating
a replacement window after cancelled will-quit. app.exit(code) is implemented with
synchronous native destruction and closed/quit notification, bypassing cancellable
quit hooks. Shared runs verify signed codes 0, 7, 13, -1, 255, 256 and INT32_MAX,
POSIX exit-status truncation, and explicit status precedence over process.exitCode.
The reference returns to the current JS call after app.exit; Firefly does likewise.
Invalid codes are rejected without closing a live window. Exit before readiness
with an unsettled ESM await ends with the selected code, without a false unsettled
await diagnostic. A generic optional host exit code in Mite preserves this policy;
standalone CLI behavior and linkage remain unchanged.

A quit attempt is marked active before calling before-quit listeners, coalescing
nested calls rather than recursively repeating hooks. This is a documented
intentional difference: the bounded Electron reference repeats before-quit and
close twice; Firefly emits each once. Both outcomes have separate explicit test
expectations. All 27 Mite suites and the expanded actual native/Electron app gate
pass (build/test.log and build/firefly-target-test.log). No Ladybird code changed.
The locked-desktop screenshot gate remains open; IORegistry still reports both
IOConsoleLocked and CGSSessionScreenIsLocked true. Windows/Linux and post-exit
async timing remain unverified; beforeunload and OS quit still need work.

The preload source investigation found actual WrapperWorld/HostDefined and
WindowProxy primitives, but BiDi's named sandbox-realm path rejects the request,
and its preload runner skips sandbox scripts. PageClient::run_javascript uses the
page's own realm. None is an already working secure preload adapter. ADR 0025
records those exact boundaries and a concrete next renderer-helper prototype:
reuse the per-world wrappers and synchronous before-script EventSink boundary,
then prove helper/session routing, lifecycle, detach and membrane safety without
changing vendored source. That prototype, contextBridge and bounded renderer IPC
are still unimplemented. Do not replace them with a main-world closure.

An internal renderer-side IsolatedWorld owner is implemented using the unmodified
LibWeb Extension WrapperWorld/HostDefined/Intrinsics/WindowProxy APIs. It creates
separate realms around one document's actual DOM, propagates the principal's
agent-cluster mode and installs no Mite bindings. The owner roots its realm/world,
restores execution contexts on errors and rejects recursive/detached evaluation.
Detach is idempotent; a request during evaluation is logically immediate but
physically deferred until context pop. A strong evaluation self-reference permits
the last external owner to be released safely inside the callback.

The actual native check-firefly-isolation gate passes: page prototype poisoning,
separate globals and DOM expandos, shared native DOM attributes, independent worlds,
a harmless native marker restricted to one world, selected Window/Document/element
constructor and WindowProxy paths, thrown/syntax error context restoration, GC,
repeated/explicit/destructor detach, direct SDK detach and callback-driven
last-owner release. A new
world created after page prototype pollution also remains separate. Log:
build/isolation-test.log. The target checks the synchronized, unmodified Ladybird
pin and Skia confinement. Mite and the public Firefly app API were not changed;
the existing 27-suite/app results from the preceding step were not rerun here.

This is a single-process synchronous SDK gate, not an installed renderer helper or
preload/contextBridge API. Raw internal VM values must not cross a future membrane.
DOM listeners, promise jobs, timers, retained functions and document-generation
revocation are not handled automatically. They remain required before privileges
or public preload can be enabled. Native Windows/Linux, nondefault COI pages and
the locked-desktop screenshot gate remain unverified/open. ADR 0026 records these
limits and the next actual renderer lifecycle/membrane integration. Ladybird stays
unmodified.

A separate macOS development WebContent helper now attaches the isolated-world
owner to the actual before-author-script lifecycle. It reuses the original
upstream bootstrap, sandbox and BiDi service translation units through two small
compile adapters. No engine source was copied, edited or patched. Three token
seams are checked during configure/build; API changes also require compilation
and the runtime gates. This is adapter maintenance, not a stable upstream ABI.
The regular Firefly application still selects the original helper.

The actual check-firefly-renderer-helper gate passes with an unchanged helper as
a negative control: only our helper installs the harmless compiled ordering marker
before the first author script. Top-level and srcdoc first scripts verify separate
globals/prototypes and shared DOM; helper log PIDs match the reported WebContent
PID. Seventy sequential frame cycles create at least 141 worlds and release at
least 140 while the top-level document remains live. The initial test exposed
141 creations but only 71 destruction events: replaced initial documents did not
all produce realmDestroyed events. An active-document sweep now detaches those
owners, both before creation and on a 10 ms timer. This only proves the current
synchronous bootstrap lifetime, not async callback revocation or BFCache support.

Log: build/helper-test.log. The helper retains upstream macOS sandbox setup,
helper entitlements/signing and Skia confinement; codesign --verify --strict
passes. Resources are physically staged inside its development bundle: symlinking
them caused real sandbox font-access failures. No sandbox relaxation was added.
These checks are not a sandbox audit. Nonfatal Metal sandbox and compositor/profile
shutdown diagnostics still occur. ADR 0027 records the process/session coupling,
resource layout and remaining risks. Upstream integrity passes at the same pin;
no newer candidate, Windows/Linux helper, distribution, current screenshot or
nondefault COI support is claimed. Existing Mite/app suites were unchanged and
not rerun for this slice.

The internal BridgeData adapter now captures plain data into an owned C++ graph
with no retained JS/VM pointers and reconstructs frozen containers in the target
realm. It supports primitive values, ordinary/null-prototype objects and arrays,
including cycles, aliases, sparse slots, special numbers and exact UTF-16 code
units. Source containers must belong to the declared source realm and have the
expected intrinsic prototype. Host/exotic objects, proxies, accessors, symbols,
functions and promises are explicitly rejected without running their traps.

Default limits cover 4096 values/properties, 65536 cumulative UTF-16 code units,
depth 64 and array length 4096. Captures fail before publishing partial data.
The SDK's own-key enumeration allocates a temporary vector before our property
limit check: snapshot/output bounds do not establish hard scratch-memory or CPU
containment. This remains a prerequisite for accepting arbitrary untrusted IPC;
no engine-layout shortcut or upstream modification was introduced.

Both native gates pass: check-firefly-isolation exercises data copies in both
directions, source deletion/GC, independent frozen target identities, poisoned
prototypes and inherited setters, unsupported/foreign-realm values, zero getter/
proxy-trap calls, all budgets and recovery after rejection. The actual sandboxed
helper additionally copies compiled harmless data before author scripts and
mutates its source afterward. Top-level/srcdoc first scripts still see the original
frozen data and their own intrinsics; source globals remain private. The unchanged
helper negative control and 70-frame cleanup gate still pass. Logs:
build/bridge-isolation-test.log and build/bridge-test.log. ADR 0028 records policy
and remaining resource limits. Mite/public Firefly APIs were unchanged; their
previous suites were not rerun. Public preload/contextBridge/IPC remain unsupported,
including function/Promise proxies and generation-based async revocation. No new
platform, packaging, COI, screenshot or full sandbox-audit claim is made.

The internal FunctionBridge now exports one synchronous data-only endpoint per
exact isolated world/document. Arguments (up to 64) are copied together to preserve
aliases, and results are reconstructed/frozen in the target realm. Callers cannot
supply the source receiver or obtain raw source objects. Callable proxy/bound
source functions, unsupported arguments/results and nested entry are rejected.
Source exceptions are never inspected; callers receive a fixed target TypeError.

Native tests exposed an SDK integration trap: NativeFunction::create's first
realm argument only chooses the allocation VM. The adapter now sets and checks
the optional function realm explicitly; constructor-chain isolation passes.
IsolatedWorld shares its rooted context boundary for evaluate/call, preserving
reentry rejection and deferred detach. Explicit endpoint revoke releases roots
and the source owner. The helper performs revoke before world detach on every
cleanup path; activity/detach checks deny removed documents immediately, and a
post-call check denies output after revocation during a running callback.

Both check-firefly-isolation and check-firefly-renderer-helper pass, including
alias/cycle/freeze/receiver/constructor checks, rejected argument traps, arity and
output bounds, generic errors for thrown proxies and function/Promise results,
native reentry, GC, repeated revoke, in-flight revoke/detach with restored context,
and retained child function references denied immediately after frame removal.
The separate sandboxed helper exercises compiled arithmetic probes before first
author scripts in top-level/srcdoc documents and through 70 cleanup cycles, with
an unchanged helper negative control. Log: build/function-test.log. ADR 0029
records ownership requirements and limits. Ladybird is still entirely unmodified.

This is synchronous endpoint revocation, not full async/document-generation
safety. Promise jobs, timers/listeners, BFCache and cross-process navigation still
need implementation and adversarial tests; returning a rejected Promise may have
already scheduled source work. Public preload/contextBridge/IPC remain unsupported;
the regular Firefly app is unchanged. Hard resource containment, native
Windows/Linux, packaging, nondefault COI, sandbox audit and the visible desktop
gate remain open. Existing Mite/public-app suites were not rerun for this slice.

FunctionBridge now observes actual source-realm engine Promise results through
native perform_then reactions and returns a separate intrinsic target Promise.
Fulfillment uses the bounded snapshot/frozen reconstruction path; source rejection
values are never inspected or forwarded. Then/constructor getters on source
promises are bypassed. Unsupported fulfillment values reject with a fixed target
TypeError. Copied roots whose target prototype chain contains a then accessor,
callable or exotic object are rejected without invoking it, preventing resolution
hijacking into an untracked pending thenable. Promise/function arguments and arbitrary thenables remain unsupported.

Each endpoint owns at most 64 pending capabilities; a full endpoint rejects calls
before source invocation. Entries are removed before settlement. Revoke clears
source ownership and pending roots, rejects all outstanding target promises and
makes late source reactions inert. Both synchronous and Promise errors are now
created with saved contexts and a synthetic target frame, preventing accidental
source traceback capture during in-source revocation.

The actual helper test exposed a lifecycle issue invisible in synchronous SDK
calls: the inactive-document timer has no active JS context, and calling a native
Promise reject function panicked. An owned temporary native-call context now
supports that path without running a checkpoint inside revoke. No engine patch or
sandbox relaxation was added. Native check-firefly-isolation and
check-firefly-renderer-helper pass, including target Promise identity/async order,
copied frozen fulfillment, trap-free rejection, pending limits, GC, 64 simultaneous
revocations, late fulfillment, in-source revoke/detach with no source stack frames,
and a parent handler observing a removed child document's Promise rejection.
The existing unchanged-helper control and 70 frame cycles remain passing. Logs:
build/promise-test.log and build/promise-isolation-test.log. ADR 0030 records details.

This revokes Promise forwarding, not all source execution: previously queued jobs,
listeners/timers and retained closures can still run. The inspected SDK cannot
unregister native Promise reactions, and source promises/reactions may retain
realms until settlement/collection. Clearing our roots does not prove full realm
reclamation. Public preload/contextBridge/IPC stay unsupported; normal Firefly
and Mite were unchanged and their earlier app/runtime suites were not rerun.
Hard resource containment, BFCache/cross-process navigation, callbacks, native
Windows/Linux, COI, packaging, full security audit and the locked-desktop visible
gate remain open. Ladybird remains unmodified at the same pin.

WorldJobGuard now guards Promise job execution for registered owned worlds using
existing VM hooks. The callback hook unwraps bound/proxy callbacks to their actual
realm without traps; the enqueue hook checks owned job realms both before queuing
and just before execution. Inactive documents permanently detach the world. A weak
realm registry and VM-retained native state avoid rooting dead worlds or accessing
a destroyed renderer session. Work without an owned realm/origin keeps the upstream path. The nearest owned
execution-stack realm is also checked, covering Proxy apply traps whose targets
belong to foreign realms; cross-realm scheduling through an active owned caller is
conservatively tied to that caller's world lifetime.
Owned jobs use public HTML microtask/TemporaryExecutionContext APIs with preserved
ScriptOrModule and exception reporting. No extra upstream source seam or patch
was added.

The callback hook alone failed a real SDK assertion: a direct async await-resume
job bypassed it and accessed detached wrappers. The enqueue/execution guard now
passes reactions, bound/proxy callbacks, thenables, async/await, async generators,
GC and late resolution. Page jobs and another live protected world continue;
an unregistered detached-world negative control still executes its harmless native
counter callback, proving upstream detach alone does not enforce this policy.
The helper fixture constructs its argument in the child's realm, retaining the
strict bridge policy. Both native gates pass (build/world-job-final-test.log and
build/world-job-isolation-test.log): six queued source categories are blocked after
frame removal, with unchanged retained DOM and matching PID counters. Existing
ordering control, Promise results and 70 frame-cycle checks remain passing.
ADR 0031 records the two-hook boundary and future upstream-review requirements.

This is pre-execution Promise-job revocation, not universal async cancellation.
Already running async frames/detach during those frames, queueMicrotask,
timers, DOM/WebIDL listeners, direct retained calls, finalization/worker behavior,
BFCache/cross-process navigation and new engine job types still need implementation
and coverage. Source promises may remain unsettled; target endpoint revocation is
still mandatory. No full realm collection or hard queue/CPU/memory bound is proven.
Public preload/contextBridge/IPC remain unsupported, normal Firefly/Mite are
unchanged, and their earlier app/runtime suites were not rerun. Ladybird remains
unmodified; native Windows/Linux, COI, packaging, full security audit and the
locked-desktop visible gate are still open.

Guarded Promise jobs now retain their native world owners for the duration of
execution. IsolatedWorld is weak-referenceable, and the job registry maps weak
realms to weak native owners instead of retaining them between jobs. Origin/job
and callback owners enter the same rooted execution boundary used by evaluate/call;
nested JS callbacks reuse an already active frame. Logical detach and missing
owners deny further jobs immediately, while physical wrapper/context teardown waits
for frame pop. Inactivity requests go through owner.detach rather than raw SDK
detach. Dropping the last external owner inside a running job is safe.

Native target Promise settlement now runs under a synthetic target context with
source execution frames saved/rooted. This keeps sanitized rejection handlers and
already approved result continuations outside the source job's cancellation scope.
A specific SDK regression publishes a copied/frozen result, revokes the source
before its target consumer runs and requires that consumer to receive the result.

Both native gates pass: build/async-frame-final-test.log; the additional SDK
regression also passes in build/async-frame-isolation-test.log. SDK reaction/await/generator jobs request
detach, release their last external owner, continue current-turn DOM access and
attempt another job. The new job is denied; afterward the native owner weak link
is expired, SDK wrappers are detached and context depth is restored. GC and the
existing pre-start cancellation/live/foreign controls pass. Actual helper probes
remove their own iframe during each of those three running job types: current DOM
access completes, further source work is blocked and the live parent receives the
endpoint Promise rejection. Existing ordering/Promise/70-cycle gates still pass.
ADR 0032 records the lifetime and target-settlement policies. Ladybird is unchanged.

Public preload/contextBridge/IPC remain unsupported. queueMicrotask, timer and
DOM/WebIDL callbacks, direct/cross-world function transfers, finalization/worker
paths, source error-event isolation, BFCache/cross-process navigation and resource
containment still need work. Running JS is not interrupted mid-turn; every future
privileged endpoint must enforce logical revocation. No total realm collection,
new platform/package/security-audit or visible-desktop claim is made. Normal
Firefly/Mite were unchanged and their previous app/runtime suites were not rerun.

Source Promise rejection isolation now has actual exploit/control evidence.
Before filtering, the separate sandboxed helper exposed at least three source
rejection objects through principal page events, including a private function
reference; later handling exposed a source handled event as well. The fixture
also required normal page reject/handle events. The functions were harmless
compiled probes. Baseline evidence: build/error-baseline.log.

WorldJobGuard now wraps the existing rejection tracker. Registered Promise realms
and Reject operations originating in owned source frames are kept out of the
principal tracker without reading/coercing reasons. Weak Promise identities cover
later Handle operations outside the source, including foreign-realm Promises.
Page promises handled by source code still delegate their original Handle
operations. Guarded abrupt job completions no longer use the ordinary reporter,
which can dispatch raw error objects; only native failure counters are recorded.
These are Reject-operation/abrupt-job counts, not unhandled-error diagnostics.

Both native gates pass (build/error-isolation-test.log). SDK delegation probes
cover source/foreign/derived-foreign rejections, later native handling outside the
source, a page promise handled by source code, zero reason-proxy getter calls and
GC. The actual helper fixture now sees zero source rejection/handled events while
its page controls each occur once. Matching PID counters prove source rejections
were generated. All existing ordering/Promise/active-frame/70-cycle gates pass.
ADR 0033 records the measured boundary, policy and limits. The abrupt engine-job
reporting branch is payload-free but not separately triggered by this fixture.
Ladybird remains unmodified, at the same pin.

This does not cover every callback/error route. queueMicrotask, timer and DOM/WebIDL
callbacks, direct/cross-world function transfers, finalization/workers,
navigation/BFCache, validated IPC and hard resource containment remain open.
Native counters are not a finished preload error API. Public preload/contextBridge/
IPC remain unsupported; normal Firefly/Mite were unchanged and their previous
app/runtime suites were not rerun. Windows/Linux, COI, packaging, full security
audit and the visible desktop gate remain unverified/open.

The common WebIDL Report callback path is now adapted through our own LibWeb
translation unit. queueMicrotask bypasses Promise hooks, so replacing only its
global binding would miss borrowed/native entry paths. CallbackAdapter includes
the complete original AbstractOperations.cpp with a narrow token rename and
reinstates both public overloads. CMake substitutes exactly that one source entry,
preserving generated sources and checking pin/token counts at configure/build.
Declarations (including PerformanceObserver's same-named member) are preincluded
to preserve class definitions. No vendored source changed or was copied.

A per-thread/VM native gate routes registered Report/non-Promise callbacks through
world/document checks and rooted owner frames. It checks callback shape and actual
function realms, preserving proxy/bound handling. Owned errors are captured via
the original Rethrow path and counted without getters/raw page ErrorEvents.
Unregistered callbacks and other conversion modes retain the original route.
Gate state remains VM-owned across renderer-session destruction; native table
entries do not root JS values or introduce IPC/JavaScript APIs. ADR 0034 records
the compile seam, source-list handling and remaining conversion boundaries.

All current gates pass in build/callback-final-test.log: SDK queueMicrotask
ordering/receiver/arity, cancellation, GC, bound/proxy/foreign-target controls,
in-flight last-owner release and raw Proxy exception redaction; actual helper
removed/self-removing frames now include queueMicrotask, source ErrorEvents stay
private and normal page timer ErrorEvents still fire. Existing Promise/rejection/
70-cycle checks pass. Because the shared LibWeb build changed, ordinary headless
renderer/compositor/process-isolation and actual Firefly/Electron fixtures were
rerun and also pass. The upstream-helper control uses the same adapted LibWeb with
no registered worlds, not a bit-identical upstream library. No newer pin was tried.
Existing linker deployment-target/Rust warnings remain; the locked-desktop visible
gate remains open despite the passing headless compositor image check.

Public preload/contextBridge/IPC remain unsupported. Promise-returning/rethrowing
WebIDL variants, call_user_object_operation, typed callback results, timer/observer
resource cancellation, direct/cross-world transfers, workers/finalization,
navigation/BFCache and hard queue/CPU/memory bounds still need work. Timer/listener
coverage is not claimed merely because some share this callback entry. Mite stays
independent; its previous standalone suites were unchanged and not rerun. Native
Windows/Linux, COI, packaging and full security audit remain open.

Native source timer resources are now released with their world. A second owned
LibWeb compile adapter views the original WindowOrWorkerGlobalScopeMixin timer map
without copying it or changing its class layout. It forwards GC/iteration and
observes entry set/remove/clear. Configure requires both exact source substitutions;
the new timer token guard checks 15 occurrences. Callback/Timer APIs and algorithms
stay upstream, and the vendored checkout remains unmodified.

Weak metadata records source realm, actual Window/map, ID and exact Timer object.
Repeated interval rearming preserves that ownership. Explicit clear/completion/map
clear removes it. IsolatedWorld physical-detach hooks stop the specific native
Timer and clear only an ID still matching that object; they never clear a whole
Window's map. Timers created after logical detach during a running frame are
included at physical cleanup. Already invalid owners use a rooted cancellation
microtask after upstream insertion/start to avoid an armed orphan. ADR 0035 records
the retained algorithms, GC contract and native release ordering.

All gates pass: build/timer-acceptance-test.log and the additional SDK assertions
in build/timer-isolation-final-test.log. SDK GC runs before taking timer snapshots,
then verifies armed native timers, cross-kind clear semantics, metadata removal
and actual inactive timers after detach. In-flight job/callback tests verify cleanup
of timers created after logical detach and then force GC. The actual helper keeps
a document and its existing page interval alive while detaching only the source
world: its interval stops, timeout payload never runs and matching PID counters
show two native cancellations. The page interval continues. Existing SDK/helper,
ordinary renderer/compositor/process and Firefly/Electron reference gates pass.
Additional reentrancy/last-owner cleanup checks pass in build/timer-cleanup-test.log:
cleanup is logically detached, holds its live owner through native hooks, tolerates
repeated detach and denies new evaluation. No zero-refcount owner is resurrected.
Linker/deployment warnings and the separate locked-desktop visible gate remain.

Public preload/contextBridge/IPC remain unsupported. String-timer realm semantics
need explicit policy; current upstream compiles them in the global settings realm.
Observer/listener resources, other callback conversions, typed results and direct
transfers, workers/finalization, navigation/BFCache and queue/CPU/memory containment
still need work. No complete realm reclamation, new platform/package/security-audit
or visible-desktop support is claimed. Standalone Mite is unchanged and its earlier
suites were not rerun. No newer Ladybird pin was evaluated.

MutationObserver registration cleanup and source argument wrapping now form the
next internal slice (ADR 0036). A third owned LibWeb TU adapter includes the full
original MutationObserver.cpp and views its weak node list by reference. It tracks
successful append/disconnect-clear without class-layout, vendor or algorithm
changes. Exact source-substitution and three-token guards join the existing
callback/timer checks. Weak source metadata deduplicates multi-node registrations;
explicit disconnect removes it, re-observe restores it, and physical world detach
roots affected observers while invoking upstream targeted disconnect. Native node
registrations, transient registrations and pending records are cleared.

Upstream currently constructs mutation callback arguments in the settings object's
principal realm. The source callback gate now rebuilds only recognized native
mutation deliveries in the source wrapper world: record Array, MutationRecord
wrappers, observer and receiver. It reads own native descriptors and preserves
identity/order/shared DOM without invoking page getters. SDK and helper fixtures
poison page Array/Observer/Record prototypes to verify actual source isolation.

All gates pass in build/observer-acceptance-test.log: SDK isolation, actual helper,
ordinary renderer/compositor/process and Firefly/Electron reference tests. Additional
SDK repeated/multi-node observe, explicit disconnect and re-observe checks pass in
build/observer-isolation-final-test.log. GC runs before native snapshots. Pending
record detach requires empty native node lists/records; source callbacks stop while
page observers and the document remain live. The helper additionally requires a
matching-PID native cancellation counter. No newer pin was tried; Ladybird remains
unmodified. Standalone Mite is unchanged and its earlier suites were not rerun.

This does not prove complete realm reclamation: an upstream queued notification
can retain the observer until a checkpoint. Other observer/listener resources,
other callback conversions/results, direct cross-world function transfers,
workers/finalization, navigation/BFCache and hard resource bounds remain open.
Public preload/contextBridge/IPC remain rejected. Native Windows/Linux, packaging,
full audit and the locked-desktop visible gate remain unverified/open. Existing
linker/deployment warnings remain.

Event listener dispatch now has a native lifetime/error gate (ADR 0037), before
upstream currentTarget/Event wrapper creation. A gate only on the generic WebIDL
object operation proved too late: SDK detached delivery reproduced an assertion
in WrapperWorld::set_wrapper. A fourth owned TU adapter includes complete original
EventTarget.cpp with only its free invoke_event_listener entry renamed. Original
public dispatch routes through our existing per-VM registry. Exact TU/token guards
and unregistered-world delegation remain; the vendor checkout is unchanged.

Source associated/actual function realms are checked before wrapper construction.
Original handleEvent lookup/getter/proxy invocation runs inside retained native
owners, including deferred physical detach and last-owner release. Use original
CallbackType::associated_realm through a small in-LibWeb wrapper to retain its
non-function origin rules. Source abrupt completions are counted without examining
payloads and suppressed before page exception reporting. Normal void listener,
receiver/Event identity and cancelable-dispatch behavior stay upstream.

SDK isolation passes in build/listener-isolation-test.log. New controls include
source Event prototypes despite page poisoning, function/object receiver and getter
counts, source function/getter error privacy, detached getter suppression and
continued page listeners. In-flight function/getter/proxy tests drop the last owner,
finish DOM access, create timers after logical detach and deny continuations; timers
are released and contexts restored. Full acceptance is recorded in
build/listener-acceptance-test.log, including the sandboxed helper's explicit
source detach, page errors/listeners and self-removing frame callbacks, ordinary
renderer/compositor/process and Firefly/Electron reference gates.

Native listener registrations/signal abort algorithms still need targeted removal.
Handler attributes and script-valued event fields (CustomEvent.detail, ErrorEvent
error, MessageEvent data) need policies; source native Event wrapping alone is not
a general membrane. Generic object operations, other observers/callback results,
direct transfers, workers/finalization, navigation/BFCache and hard resource bounds
remain open. Public preload/contextBridge/IPC stay rejected. No complete reclamation,
new platform/package/audit or visible-desktop support is claimed. Standalone Mite
is unchanged; its previous suites were not rerun. No newer upstream pin was tried.

Native listener/signal resources now release with their owning world (ADR 0038).
The existing EventTarget adapter preserves the original virtual list-changed call,
then observes its two successful add/remove sites. Preincluded original virtual
headers preserve class declarations. The original abort algorithm expression maps
its actual returned Optional ID and records exact signal/ID without replacing its
closure. This also catches duplicate registrations that create an abort algorithm
without inserting a native listener. No additional TU seam or vendor changes.

Weak registries track source listener and signal ownership independently. Explicit
remove/once/abort update listener metadata; exact normal signal removal releases
its recorded closure too. Physical detach erases metadata before rooted native
cleanup, removes exact signal IDs and calls original targeted listener removal.
Never clear all listeners or abort a shared signal. Aborted signals finish clearing
their own algorithm map, avoiding reentrant iterator mutation. Counters describe
cleanup requests; aborted duplicate bookkeeping may remain weak until pruning.
Late registrations inside logically detached but executing frames are included at
physical teardown, with invalid owners cleaned at the native boundary.

SDK tests pass in build/listener-resources-test.log. Native SDK, ordinary renderer/
compositor/process and Firefly/Electron reference pass in
build/listener-resources-acceptance-test.log. Its helper behavior passed, but the
final synthetic guard test wrongly treated the expected two notification tokens
as invalid. The test now derives valid/missing/extra counts from the same named
count table as the guard. The complete helper gate passes in
build/listener-resources-helper-final-test.log. Tests cover duplicate/
capture/once/abort/explicit removal, actual target list absence and native removed
flags, retained dispatch refs denied by the earlier guard, and GC before snapshots.
A native listener is collected after detach even while its signal is strongly
rooted. In-flight job/listener/getter/proxy cases create listeners/signals after
logical detach and require complete targeted cleanup. Helper matching-PID counters
require four source entries/two signal algorithms removed; source results stay
stable while page listeners and later page signal abort continue. No newer pin was
tried; Ladybird remains unchanged. Existing linker/deployment warnings persist.

Public preload/contextBridge/IPC remain rejected. Handler attributes, generic
WebIDL object operations, script-valued event fields, other observer kinds/results,
direct transfers, workers/finalization, navigation/BFCache and hard bounds remain
open. No complete reclamation, new platform/package/audit or visible-desktop support
is claimed. Standalone Mite is unchanged and its earlier suites were not rerun.

Native handler dispatch and return conversion now have their own lifetime/error
gate (ADR 0039). Replacing a handler can reuse a native listener originally created
in the page; its principal callback cannot stand in for the source handler owner.
The new handler entry checks source realms before receiver/Event wrapping, retains
the owners and sanitizes abrupt completions before native page error reporting.
Ordinary/error returns are reduced to strict boolean/undefined. BeforeUnloadEvent
conversion happens inside the retained frame, preserving cancellation-before-
ToString, nullish returns and skipping conversion for nonempty returnValue.

The existing EventTarget adapter uses a build-generated include. It renames only
two free definitions, preserving bodies, internal calls, licenses and every other
byte. Plain token renaming would also rename the internal handler call and bypass
the guard. The generator requires exact single definitions, emits source #line,
has explicit build dependencies and no committed vendor-source copy. Its regression
test checks source preservation, internal public call and missing/duplicate rejection.
No new TU substitution or vendor edits; header/layout/algorithm boundaries remain.

All acceptance gates pass in build/handler-acceptance-test.log: SDK isolation,
sandboxed helper, ordinary renderer/compositor/process and Firefly/Electron reference.
SDK tests create principal native listeners first, replace callbacks from source,
verify receiver/Event/strict-false and special error semantics, ignored object
results without traps, redacted throw and detached rejection. Handler and native
beforeunload ToString cases detach/drop the last owner yet safely finish DOM work,
unwind contexts and physically detach. Throwing conversion leaves canceled=true
and empty returnValue; an existing value skips hostile conversion. Helper tests
repeat source-only detach, stable DOM, principal errors/listeners and self-removing
frame handlers; normal close retains a detached beforeunload replacement safely.

Handler storage isolation is still open and now explicitly identified from source:
native EventTarget handler maps are shared, and generated getters return callback
objects directly (e.g. HTMLElement.onclick). Raw references could bypass native
entry guards on direct calls, and removing a listener need not release its stored
handler value. Implement world-specific handler storage/getter/setter semantics
and targeted cleanup before public preload. These passing dispatch tests do not
prove that boundary. Script-valued event fields, generic operations, other observers,
direct transfers, workers/finalization, navigation/BFCache and hard bounds remain
open. Public preload/contextBridge/IPC remain rejected. No complete reclamation,
new platform/package/audit or visible-desktop support is claimed. Existing warnings
remain; standalone Mite is unchanged and no newer Ladybird pin was tried.

Registered isolated worlds now use private native handler storage (ADR 0040),
closing ordinary handler getter sharing identified in ADR 0039. The existing
EventTarget adapter routes four map addresses through a per-realm/target selector,
adds native target tracing, selects native listener realm from storage ownership
and scopes principal document attribute/full-erasure algorithms. The generator
requires exact seam counts and its regression reverses all edits to recover every
upstream byte. Native algorithms/class layouts/vendor files remain unchanged;
compiled engine behavior is adapted, not a bit-identical upstream binary.

Native map metadata holds weak realm/target keys. Actual native EventTargets trace
private handler values, so dead node/callback cycles do not become registry roots.
Private property getters return own callbacks/null, preserving identity and
upstream replacement order; principal and other-world getters remain separate.
HTML text event attributes keep document/principal compilation and storage even
when Source sets/removes them. A handler listener uses its storage owner's realm,
including when its assigned callback belongs to a different realm. Actual callback
invocation still uses the associated callback realm.

Physical detach removes native listeners and map metadata, roots targets/handler
cells and calls original null setters under exact-map overrides. It preserves page
and other-world maps. Full native target erasure clears every private map and then
principal state, including when invoked from source; subsequent handlers can rearm.
Logical-detach late writes are included at physical cleanup. Revoked direct calls
get private storage with rooted native microtask cleanup, never principal fallback.

All gates pass in build/handler-storage-acceptance-test.log: native SDK, sandboxed
helper, ordinary renderer/compositor/process and Firefly/Electron reference. SDK
checks page/two-world getter isolation, function identity/prototypes, replacement
order, inline attributes, independent clear/detach and GC before dispatch. A detached
node/callback cycle collects while its world remains live. Native erase/rearm and
foreign callback owner-map selection pass. Retained native handler probes still
verify denial before detached wrapping. Helper source/page getters stay separate,
source cancellation/error behavior remains correct, page handlers/listeners continue
after source detach, matching-PID map counters advance, and self-removing frame
handler/lifetime controls pass. Existing beforeunload and other isolation gates pass.
Additional SDK checks pass in build/handler-storage-late-write-test.log. All active
reaction/await/generator/microtask/listener/getter/proxy modes create handler maps
after logical detach and require release at frame pop. An explicitly retained raw
C++ harness call after physical detach cannot reach principal storage: its native
registration is removed immediately and late private map clears at the native
checkpoint. No raw-call JavaScript API is enabled.

Counts include empty getter-created maps, and dead records prune on lookup/teardown.
Additional map memory is not yet charged to upstream external-memory accounting;
hard resource bounds remain open. Other script-valued event fields, generic object
operations, observers/results, direct transfers, workers/finalization and navigation/
BFCache still need work. Public preload/contextBridge/IPC remain rejected. No full
reclamation/audit, new platform/package or visible-desktop support is claimed.
Ladybird stays unmodified; no newer pin was tried. Standalone Mite is unchanged and
its earlier suites were not rerun. Existing deployment/linker warnings remain.

CustomEvent.detail now has an internal value membrane (ADR 0041). Two additional
owned native/generated compile adapters route constructor/successful init origin,
native GC tracing and the detail getter return through the existing per-VM registry.
Original checkout/generated binding/class layout and other bytes remain unchanged;
source preservation/drift and exact source-substitution checks guard the seams,
including bindgen ordering. No newer Ladybird pin was evaluated.

Same-world values retain identity. Registered cross-world reads use bounded
BridgeData capture/frozen reader-realm materialization, reject functions/proxies/
accessors/exotics/unsupported primitives without executing traps and throw only a
generic reader TypeError. Weak writer provenance must agree before same-world
bypass; foreign prototype shapes cannot launder source values. Global guard-chain
activity and sticky revocation cover pre-registration events and expired weak
writers. Events trace at most 64 per-reader copied values for stable identity.
Snapshots stay independent of later source mutations; successful reinit invalidates
caches, while init during dispatch keeps the original generation. Source detach
denies fresh transfers but preserves published copies in other worlds; target
detach releases its caches through every guard layer.

All gates pass in build/custom-event-acceptance-test.log: SDK, sandboxed helper,
ordinary renderer/compositor/process and Firefly/Electron reference. Follow-up
SDK/helper provenance/revocation passes are in build/custom-event-revoke-test.log
and build/custom-event-provenance-test.log. Explicit primitive writer collection
passes in build/custom-event-origin-gc-test.log and the final provenance run.
SDK checks graph alias/cycles/holes, target prototypes/freeze, identity/cache/reset,
native GC tracing, dispatch-time init no-op, rejected values with zero traps, page
identity and source rejection of page getters, foreign-prototype ownership,
published-vs-fresh readers after revoke, target cache collection and expired-writer
revocation. Helper dispatches a source payload twice around mutation, checks stable
frozen page copies, raw-function rejection, page-to-source copies/zero getter calls,
source revoke and a fresh iframe reader's TypeError. Existing gates remain passing.

This is an intentional cross-world snapshot difference, not public preload or full
Electron parity. The native event still holds its original raw payload until it is
reinitialized/collected, so object payloads can retain a source realm after detach.
No complete reclamation/audit is claimed. Other event fields, arbitrary borrowed
getter/direct function transfers, observers/results, workers/finalization, BFCache/
navigation and aggregate CPU/queue/memory bounds/accounting remain open. Public
preload/contextBridge/IPC stay rejected. Standalone Mite is unchanged, with no new
platform/package or visible-desktop validation. Existing warnings remain.

ErrorEvent.error and the special onerror fifth argument now share the event value
membrane (ADR 0042). EventValues handles a single payload slot on each supported
native Event subtype, reusing cache/provenance/GC/revocation policy from CustomEvent.
Two additional native/generated ErrorEvent TUs adapt registration/tracing and the
constructor/getter, while the existing EventTarget adapter routes its raw error
argument through the reader in the callback realm. Exact source counts, original
bindgen dependencies and full reverse-edit source preservation tests guard the seams.
Vendor/generated originals and native layout remain unchanged; no newer pin tested.

Writer provenance is now explicit in both JS event constructor bindings; native
constructors use unknown origin. CustomEvent successful init records its JS entry
realm. A native page error reported beneath a Source dispatch must keep ordinary
page identity, not inherit that ambient Source caller. Unsupported field/argument
transfers still reject generically. Reader TypeErrors now use a synthetic target
context with the original stack saved, preventing Source dispatch/callback frames
from appearing in their stacks. Getter/onerror agree on the same cached data copy.

All gates pass in build/error-event-acceptance-test.log: SDK, sandboxed helper,
ordinary renderer/compositor/process and Firefly/Electron reference. SDK/helper
checks cover same-world data/identity, Source-to-page frozen cycles/snapshots and
exact getter/fifth-argument identity including error metadata. Reverse page input
keeps Page identity but uses Source copies/prototypes. Functions and hostile page
getters cannot cross either route; traps remain zero. Reader errors have no Source
frames. Native page exceptions inside Source-triggered page callbacks still deliver
ordinary page Error objects to page onerror. Published data remains after source
detach. All earlier CustomEvent provenance/GC/init/revoke and lifecycle/resource
controls continue to pass.

Cross-world Error instances and other non-data values remain unsupported; Source
onerror's unsupported arguments are rejected/redacted before handler invocation.
This is not full error/Electron compatibility or public preload. Native raw payloads
may retain origin realms until event collection. MessageEvent and other event fields,
arbitrary borrowed function/getter transfers, general callback frame/stack and
job-origin isolation, other observers, workers/finalization, BFCache/navigation and
hard resource bounds remain open. Public preload/contextBridge/IPC stay rejected.
Standalone Mite is unchanged; no new platform/package/audit or visible validation.
Existing linker/deployment warnings persist.

MessageEvent.data now uses the event value membrane (ADR 0043). Native window
postMessage already deserializes in the principal receiver realm; its MessageEvent
getter previously returned that value directly to Source. Two owned native/generated
adapters add data-cache tracing, successful legacy init invalidation, explicit JS
constructor origin and getter routing. Exact source counts, original bindgen ordering
and source preservation/drift tests guard the seams. Native constructors/serialization/
delivery/origin/source/ports algorithms are unchanged; vendor files remain clean.

Owned reads now get the existing bounded frozen graph/cache/revocation behavior.
Same-world/page identity stays unchanged. MessageEvent.initMessageEvent during
dispatch remains a no-op; successful reinit resets snapshots and the original ports
cache. Source-owned synthetic event data cannot leak functions/getters into another
world. Serialized native posts remain approved principal-realm publications and can
arrive after Source detach without retaining an active Source listener.

All gates pass in build/message-event-acceptance-test.log: SDK isolation, sandboxed
helper, ordinary renderer/compositor/process and Firefly/Electron reference. SDK
checks cycles/aliases/holes/prototypes/freeze, stable snapshots/cache/GC, init reset
and dispatch no-op, origin/lastEventId/WindowProxy source/empty ports controls,
rejected values with zero traps, reverse page data and independent revoke/cache
collection. Helper checks synthetic source data around mutation, raw-function
rejection, reverse page input, zero accessor calls and page identity. Actual native
postMessage passes both directions with a source reply, native event flags and
mutable principal data; function serialization fails with DataCloneError. A source
posts then detaches: its listener stays silent, the page receives the serialized
post and published snapshots stay readable. Basic source WindowProxy rewrapping
cannot expose a Source global through its page constructor. Prior gates pass.

This is data-field protection, not IPC/sender authentication. Synthetic origin strings
remain untrusted; event flags are not a main-process authorization protocol. Map/
ArrayBuffer/typed-array/host clone parity, actual transferred ports, worker/channel
messaging and source proxy edge cases remain unverified. Native raw data/source can
retain origin state until event collection. Other fields, callback frame/job-origin
isolation, shared registry/custom element boundaries, other observers, arbitrary
transfers, navigation/BFCache and hard resource bounds remain open. Public preload/
contextBridge/IPC stay rejected. No newer pin, standalone Mite change, new platform/
package/audit or visible-desktop validation. Existing warnings persist.

Principal callbacks now run without ambient Source VM contexts at the guarded
listener/handler, non-Promise Report-style WebIDL and event exception-reporting
entries (ADR 0044). The VM saves/roots the original stack; a synthetic principal
context isolates stack capture and page job/timer attribution. Native Source owner
frames remain active and restore before unwinding, preserving deferred physical
detach even when page reentry drops the last owner and forces GC. Foreign principal
beforeunload return conversion also stays in principal scope. Owned callback errors
are filtered before raw native reporting; ordinary page errors/onerror continue.

The EventTarget generator now adapts three exact free definitions, adding the
exception reporter without a new TU substitution or vendor changes. All other
bytes still round-trip through the preservation test. Upstream pin integrity and
strict helper signature verification pass; no newer pin was evaluated.

All four gates pass in build/principal-callback-final-test.log: SDK, sandboxed
helper, ordinary renderer/compositor/process and Firefly/Electron reference. A
negative baseline showed eight Source-frame leaks, two Source-owned timers and one
surviving page job; the fixed original fixture shows zero/one/eight. The expanded
SDK Report probe shows zero/one/nine, with a tenth page job surviving last-owner/
GC reentry. The helper verifies eight page jobs and a ticking page interval after
Source detach; actual Source jobs/timers stay blocked and the active caller finishes.
Foreign beforeunload ToString stack/job checks and previous boundaries pass.

Generic WebIDL Rethrow/NotSpecified/Promise-returning conversions, user-object
operations, other native callbacks, borrowed transfers and shared registries remain
open; this is not a general membrane or API allowlist. Public preload/contextBridge/
IPC stay unsupported. No standalone Mite change, new platform/package/audit, complete
reclamation, hard resource containment or visible-desktop validation is claimed.
Existing deployment/linker warnings remain.

Native NodeFilter invocation is now guarded before node wrapping and through
acceptNode getter/proxy lookup, callback execution and numeric result conversion
(ADR 0045). One owned NodeFilter TU adapter renames only the exact helper definition;
internal traversal calls retain the public guard. Source preservation/drift tests,
one-TU enforcement and explicit generated dependencies keep vendor source untouched.

Registered filters check associated/actual realm revocation and retain owners through
conversion. Source errors become fixed caller-realm TypeErrors without Source frames
or raw thrown objects; this intentionally changes exception identity even for own-
world callers. Principal filters retain ordinary errors and execute beneath a clean
principal context, keeping their Promise jobs independent of an ambient Source.

All four gates pass in build/node-filter-acceptance-test.log: SDK isolation,
sandboxed helper, ordinary renderer/compositor/process and Firefly/Electron reference.
Additional throwing-valueOf and detached-child error-constructor controls pass in
build/node-filter-final-test.log (SDK/helper). SDK covers own identity/receiver/Node
world, result conversion, raw callback/conversion error denial, retained iterator
revocation before lookup after GC, last-owner release and GC during getter with
successful subsequent callback/conversion, and page getter/body/valueOf jobs surviving
Source detach without stack disclosure. The helper checks live traversal, proxy
errors, unchanged page errors and iframe removal during numeric conversion: Source
synchronous work finishes, bridge output denies and its late job stays silent.

Traversal .filter getters still return raw callback objects across wrapper worlds;
this separate property boundary remains OPEN before public preload. Native traversers
can retain origin realms after detach. This step is not a complete traversal/object
membrane, generic call_user_object_operation policy or reclamation proof. Public
preload/contextBridge/IPC remain unsupported, with other callback modes, arbitrary
transfers, registries, navigation/BFCache, workers/finalization and resource bounds
still open. No standalone Mite change, newer Ladybird pin, platform/package/audit
or visible-desktop validation. Existing warnings remain.

The raw traversal .filter getter boundary from ADR 0045 is now guarded (ADR 0046).
Two generated binding adapters route only NodeIterator/TreeWalker filter returns;
the existing native adapter records constructor provenance. Own-world callback
identity and active null filters remain. Cross-world reads reject with a generic
reader TypeError, without Source stacks or getter/proxy execution. Native traversal
invocation continues through its separate lifetime/conversion guard.

Weak native-filter/writer metadata prevents prototype/foreign-callback provenance
confusion. Same-world bypass requires reader/writer/associated/actual realm agreement
when registered worlds participate. Physical detach marks origin revocation sticky,
including after writer GC. Earlier guard layers can validate their own preexisting
records; missing provenance and unresolved callable realms otherwise reject. Native
metadata adds no callback/realm GC roots and prunes dead rows on creation/read.

Validation: all four gates pass in build/filter-access-acceptance-test.log (SDK,
sandboxed helper, ordinary renderer/compositor/process and Firefly/Electron reference).
Additional SDK/helper checks pass in build/filter-access-final-test.log, including
a deliberately unobserved native constructor and a revoked callable proxy.
SDK covers Source/page/second-world reads, own identity, foreign prototype rejection,
unchanged native traversal, generic errors, writer collection with a retained native
foreign-callback container and TreeWalker/filter collection after references drop.
The helper covers both traversal types with function/bound/callable-proxy/object/
object-proxy callbacks, zero traps, identity/null controls and getter denial after
Source detach. Generation tests reverse constructor/getter edits to recover exact
original bytes and reject missing/duplicate/drifted seams. Pin and strict helper
signature checks pass; vendor files remain unmodified.

The compiled helper's native traversal-publication hook exists only for this harmless
fixture and has no privileged API; the ordinary helper registers no worlds/hooks.
Unknown provenance/unresolved callable realms intentionally fail rather than return
raw callbacks. These getter paths do not establish a general callback/object API
membrane. This pin's Document traces registered NodeIterators, so native filter/
realm retention remains possible even after JS references drop; no complete resource
reclamation/audit is claimed. Public preload/contextBridge/IPC, other callbacks/API
registries, arbitrary transfers, workers/finalization, navigation/BFCache and hard
bounds remain open. No standalone Mite change, newer pin, new platform/package or
visible-desktop validation. Existing warnings persist.

Standalone Mite now implements a native node:os foundation (ADR 0047): system/
architecture identity, hostname, home/temp paths, endianness, available parallelism,
free/total memory, uptime/loadavg and EOL/devNull. It uses existing libuv and our
LibJS/JS module adapters, with no Node/V8, LibWeb, new dependency or vendor change.
CJS/bare/node:/ESM module identities and supported named exports share the normal
builtin cache. OS information strings snapshot at module import; dynamic queries
and native environment reads remain live. PATH_MAX home-directory failures match
the observed native Node behavior instead of accepting oversized environment paths.

All 28 standalone CTest suites pass in build/os-ctest.log. The new Node 24.15.0 OS
fixture compares fixed host values, 15 argument/receiver checks, 12 coercions,
retained function coercion, module identities/named imports, EOL/devNull descriptors,
fresh/nonnegative load arrays, native memory/uptime bounds and 15 environment
observations. Environment tests include precedence, empty values, roots/relative/
Unicode/long paths, HOME failure metadata and replacement process.env objects.
Changing free-memory/load values are range/shape checked rather than advertised as
simultaneous identical snapshots. Separate fixtures require six unsupported API/
property groups to throw explicitly. otool still shows platform libraries only,
without Node/V8/LibWeb/Qt. Firefly's shared runtime build and full existing Electron
reference gate pass in build/os-firefly-test.log; its ESM/HTTP migration fixture also
imports/executes OS named functions in the native main process before renderer load.

CPU/network lists, userInfo, priorities and constants remain unimplemented with
ERR_MITE_UNSUPPORTED_API; constants is absent from ESM named exports. Full function/
error descriptor parity, all native error paths and privileged/setuid/capability
execution remain unverified. Windows/Linux native behavior, the Windows path branch
and fallback buffer still need runners. No packaging/new npm/audit or visible-
desktop validation is claimed. Public preload/contextBridge/IPC and the other
remaining runtime/renderer milestones stay open. Existing build warnings persist;
Ladybird remains unmodified at the same pin, with no new candidate evaluated.

Mite node:os now adds native cpus() and userInfo() (ADR 0048). CPU model/speed/
millisecond times copy into fresh rooted JS graphs before uv_free_cpu_info. Native
unavailable CPU information follows Node's empty-list behavior; that failure branch
was not forced here. Native account data comes from uv_os_get_passwd, independent
of HOME; byte snapshots move into VM-owned arrays before uv_os_free_passwd. The JS
wrapper returns null-prototype ordered records with existing Buffer-based encodings,
fresh Buffer results, nullable shell and native UID/GID numbers.

The new mite-os-data suite compares 49 Node 24.15.0 account observations: encodings/
case/aliases/NUL/unknown fallback, non-object/callable/inherited options, one getter/
proxy read, thrown identity, revoked proxy, HOME independence, independent Buffers
and module/named-export identity. Fourteen real CPU records pass model/shape/prototype,
finite speed, integer millisecond counters, monotonic samples, ignored args/receiver
and independent object/array mutation checks. Changing counters/speeds are native
observations, not simultaneous equality or performance measurements.

All 29 standalone CTest suites pass in build/os-data-ctest.log. The Firefly main ESM/
HTTP fixture executes both new named APIs and retains renderer isolation; the full
existing Firefly/Electron gate passes in build/os-data-firefly-test.log. Previous OS
and remaining explicit-unsupported tests pass. Upstream integrity remains clean,
with no new library/package, vendor source edit or pin candidate.

Network interfaces, priorities and constants remain explicitly unsupported. Native
Windows/Linux, real non-ASCII account/model data, syscall/account failures, hotplug,
privileged execution and full metadata/error/monkey-patched-intrinsics parity remain
unverified. Public preload/contextBridge/IPC, broader runtime services, packaging,
platform/audit and visible-desktop requirements remain open. Existing warnings persist.

Mite os.networkInterfaces() now copies native libuv interface snapshots into fresh
ordinary JS objects grouped by UTF-8 interface name (ADR 0049). IPv4/IPv6 address/
netmask/family, lowercase MAC, internal flag, CIDR and IPv6-only scopeid retain the
reference field order. Native allocation/free ownership and GC roots cover abrupt
construction failures. Interface keys are own data properties; grouping never reads
Object.prototype. Unknown families/native failures reject rather than invent data;
UV_ENOSYS follows Node's empty-object policy. Those failure branches remain unforced.

The new pure C++ mask helper/probe covers all IPv4/IPv6 prefix lengths and invalid
noncontiguous masks: 320 cases. The new Node 24.15.0 interface differential suite
matches 12 native interfaces / 22 records on this macOS host. Independent Python
address/integer-mask checks validate families/CIDR, with IPv4/IPv6 loopback, MAC/
scope/internal metadata, own prototypes/descriptors/order, ignored arguments/receiver,
fresh graph mutation and CJS/ESM identity. Cross-process snapshots are sequential
observations, not an atomic network configuration snapshot.

All 31 standalone CTest suites pass in build/os-network-ctest.log. Firefly's main
ESM/HTTP migration fixture calls the new named API and keeps renderer isolation;
its existing full Electron-reference gate passes in build/os-network-firefly-test.log.
Previous OS/data and unsupported-API gates pass. No new dependency, privileged
renderer capability, vendor edit or pin candidate; upstream integrity stays clean.

Priorities/constants remain unsupported. Native Windows/Linux, real unusual/non-
ASCII names, noncontiguous configured masks, syscall failures, interface hotplug/
reconfiguration, privileged execution and full error/prototype-poisoning parity remain
unverified. Public preload/contextBridge/IPC and the broader runtime, platform,
packaging/audit and visible-desktop goals stay open. Existing warnings persist.

Mite node:os now implements native getPriority/setPriority (ADR 0050), with optional
PID/one-argument setter overload, strict int32 PID and -20..19 integer priority
validation, Node error codes, validation order and undefined setter success. Native
range checks precede C++ int conversion and libuv calls; OS errors use the existing
SystemError fields. No new dependency, privileged renderer capability or vendor seam.

All 32 standalone CTest suites pass in build/os-priority-ctest.log. The priority gate
compares 106 observations with Node 24.15.0: argument types/ranges/undefined/precedence,
native getter ESRCH/EINVAL metadata, PID/default/same-value operations, receiver,
function metadata and CJS/ESM identity. Disposable children verify own nice changes
0 -> 1 -> 19 using both setter forms; no foreign/parent process priority is changed.
Detailed output is in build/os-priority-differential.log. Firefly's ESM/HTTP main
fixture reads/confirms its own original priority and its existing full Electron
reference gate passes in build/os-priority-firefly-test.log.

Two measured intentional differences have explicit controls: installed Node aborts
on getPriority(-0) in its native IsInt32 check; Mite normalizes -0 to PID zero.
Node's invalid-argument formatter also executes hostile argument proxy property
reads, propagating a thrown Error; Mite rejects with TypeError/code and zero property
traps. The reference control accepts either that known assertion or successful
normalization in a future Node. These cases are separate from the 106 shared rows.

OS constants remain explicitly unsupported. Native Windows/Linux, class/elevation
mapping, priority boosts/other-process permissions, native setter failures and full
error accessor/constructor/formatting or monkey-patched-intrinsics parity remain
unverified. Public preload/contextBridge/IPC, broader runtime/platform/packaging/
audit and visible-desktop milestones remain open. No newer Ladybird candidate;
upstream integrity stays clean. Existing warnings persist.

Mite os.constants now exports native UV_UDP_REUSEADDR, dlopen, errno, signals and
priority groups (ADR 0051). The owned C++ adapter uses conditional public names
and actual SDK/libuv values, with null prototypes, immutable numeric properties,
mutable/extensible group slots and natively frozen signals. The os.constants
property is read-only/nonconfigurable; CJS/default/named ESM reads share one object.
No build-time Node query, new dependency, vendor source seam or hardcoded host-value
snapshot is required. Numeric flags do not imply UDP/addon/signal-handler support.

All 33 standalone CTest suites pass in build/os-constants-ctest.log. The new constants
gate compares all 121 native values available here against Node 24.15.0, including
names/order/types, descriptors/prototypes, extensibility/freezing, strict/Reflect
mutation/deletion, group replacement and shared identities. A child with replaced
public Object.freeze still gets frozen signals. The complete default/namespace
export-name sets also match. Firefly's main ESM fixture reads the new named export,
checks native errno/priority/signal fields and retains renderer isolation; its
existing full Electron-reference gate passes in build/os-constants-firefly-test.log.
Standalone linkage remains platform-only, without Node/V8/LibWeb/Qt. Upstream
integrity remains clean at the same pin.

The targeted os export-name surface is present on tested macOS ARM64; full module
compatibility remains PARTIAL. Native Windows/Linux constant sets/values, complete
error/function metadata and monkey-patched-intrinsics behavior, privilege/environment,
real unusual OS data, reconfiguration and earlier unforced error paths remain open.
The priority negative-zero/error-formatter differences remain intentional. No new
UDP/addon/signal implementation, npm/platform/package/audit or visible-desktop
validation is claimed. Public preload/contextBridge/IPC and broader runtime/renderer
requirements remain incomplete. Existing build warnings persist; no newer pin tried.

Mite now provides a native node:crypto random foundation (ADR 0052): synchronous/
callback randomBytes and UUIDv4 randomUUID through existing libuv uv_random. There
is no JS PRNG fallback, Node/V8, LibWeb dependency, new crypto library or vendor seam.
Sync bytes fill private rooted VM storage; Buffer views share those owned bytes.
Async workers use EventLoop::queue_work with native-only storage and rooted owner-
thread JS completion. Native byte storage releases after VM copy before user callbacks
and on abrupt completion, rather than lingering in unreachable GC closures. Existing
pending-work/fatal/shutdown accounting remains in use. Partial RNG output never publishes.

All 34 standalone CTest suites pass in build/crypto-random-ctest.log. The random gate
compares 76 Node 24.15.0 validation/shape cases, plus UUIDv4 format/variant/uniqueness
smoke, independent Buffer mutation, poisoned Math.random, named/CJS identities,
synchronous zero-size callback and nonempty async Promise ordering, arity, 64 KiB
result, 16 queued allocation-stress requests and fatal callback handling. Stress is
not a forced-GC or cryptographic-quality proof. Missing crypto methods/named imports
explicitly fail. Detailed output is in build/crypto-random-differential.log. Firefly's
main ESM/HTTP fixture awaits random bytes and validates a UUID before isolated renderer
loading; its existing full Electron-reference gate passes in
build/crypto-random-firefly-test.log. examples/mite-cli/random.mjs executes with Mite
and Node; standalone linkage remains platform-only. Upstream integrity stays clean.

Intentional differences: every UUID uses fresh OS entropy without a cache; nonempty
async callback strict receiver is undefined instead of Node's internal RandomBytesJob.
Hash/HMAC/cipher/signature/KDF/key-generation, randomFill/randomInt/timingSafeEqual and
Web Crypto/global crypto remain absent. Provider/queue failures, extreme allocations,
Windows/Linux native behavior, full error/receiver/intrinsics parity, zeroization,
certification/FIPS and a full audit remain unverified. No new npm/platform/package or
visible-desktop support is claimed. Public preload/contextBridge/IPC and broader
runtime/renderer milestones stay open. Existing build warnings persist; no newer pin.

Mite now implements incremental node:crypto createHash/Hash (ADR 0053). The owned
C++ adapter checks the existing pinned OpenSSL EVP backend directly, avoiding
LibCrypto provider asserts without changing upstream. Independent RAII contexts,
copy snapshots, fixed/SHAKE lengths, raw native byte-view bounds, Buffer codecs,
finalization/receiver state and existing Transform/pipeline support are implemented.
GC-rooted host/output objects and native context cleanup preserve the embedding
boundary; no new library/pin, Node/V8 or LibWeb dependency was introduced.

All 35 standalone CTest suites pass in build/crypto-hash-ctest.log. The new gate
compares 162 observations with Node 24.15.0 plus independent copies, getter/Symbol
recovery, ESM/CJS identity, Transform/pipeline, allocation stress and fatal stream
callback handling (build/crypto-hash-differential.log). SHA256('abc') has a fixed known
answer control; MD5/SHA1/SHA2/SHA3/BLAKE2/SHAKE are exercised locally. Two isolated
reference children capture Node native asserts on SHAKE outputLength -0 and invalid
object-mode stream chunks; Mite accepts zero and reports invalid chunks through a
TypeError error event. Future fixed reference outcomes are also accepted. Allocation
stress does not force GC. Hash outputLength above Mite's 2 GiB-minus-one Buffer limit
explicitly fails; extreme allocations and provider/allocator failures are unforced.

Firefly's main ESM fixture validates createHash before renderer loading; its existing
Electron-reference gate passes in build/crypto-hash-firefly-test.log. The hash CLI
example executes in Mite and Node with default text and a source file. Standalone
linkage remains platform-only, and Ladybird/gitlink/manifest stay unchanged and clean.
HMAC/ciphers/signatures/KDFs/keys, getHashes, Web Crypto and other absent APIs remain
unsupported. Full provider inventory, stream/error/metadata parity, secure heap/FIPS/
zeroization/audit and native Windows/Linux validation remain open. Public preload/
contextBridge/IPC, packaging and broader milestones remain incomplete. Existing
SDK/linker/locked-desktop limits persist; no newer upstream pin was evaluated.

The user requested regular pushes, a GitHub Pages project/API website, release
build automation and a pinned web-component-designer-demo app for macOS/Windows.
The implementation branch through 063123d has been pushed to origin. HMAC work was
not started; the publication/release/sample request is the current priority.

The new static homepage describes both Mite and Firefly, with original styling
inspired by Electron/Node/Bun, responsive code tabs and explicit support boundaries.
Markdown API references document the implemented Mite/Firefly signatures, events,
errors and examples. The pinned markdown-it build renders the existing architecture,
compatibility and development docs too. GitHub Pages uses an SHA-pinned Actions
workflow; pull requests build/check without deployment. Local validation: 66 generated
pages and 1,087 local link/resource/fragment checks, desktop/mobile tab keyboard
behavior and overflow checks, plus visually inspected 1440/390 captures in build/.
No runtime or upstream change. Native test results remain the previous 35 suites.

Release/sample work remains in progress: demo source 0b3c56bcd30c8047b14f847c7ec1ffab114cc597
was inspected and its locked npm dependencies installed with scripts disabled;
its explicit Monaco build and TypeScript compilation pass locally. This does not
prove it runs in Ladybird. Firefly still lacks native Windows/Linux desktop adapters,
a redistributable package layout, relocation/resource tests and signing. Those are
required before claiming downloadable Windows/macOS apps or a complete release.

The website deployment succeeded on GitHub Actions run 42107630877; the homepage
and Firefly API URL return HTTP 200 at jogibear9988.github.io/elytra. The Pages
environment explicitly permits the implementation branch as well as main.

The pinned Web Component Designer sample now compiles and runs through Mite +
Firefly/Ladybird on tested macOS ARM64. tooling/build_designer.py validates origin,
clean tracked source, exact commit and lock hash, runs npm ci with lifecycle scripts
disabled, explicitly builds Monaco/workers and TypeScript, then stages public assets
and the main module with notices. Our loopback-only Mite HTTP server preserves the
upstream URL/import-map layout and restricts normal requests to GET/HEAD within the
web tree; the renderer has no added native capabilities.

Native acceptance passes in build/designer-test.log: assets/worker MIME, HEAD, path
rejection/missing resources, production report endpoint absence, then actual app
boot, a committed button edit/content round-trip, visible Monaco DOM and absent
process/require. Explicit --smoke mode alone enables a tokenized, bounded sample-local
test report; it is not a privileged public IPC implementation. The LibWeb compositor
capture build/designer-mite-ladybird.png was visually reviewed (palette, docking,
canvas and Monaco). The earlier Python HTTP/1.0 serving attempt had an intermittent
resource failure; the intended Mite HTTP server succeeds. Visible desktop presentation
on the locked host, all designer interactions/workers/collaboration/GPU/LLM/downloads,
external/offline behavior and native Windows/Linux remain unverified. No alternate
engine or vendor change. Native bundle/release automation is the next in-progress
step; a compiled web tree alone is not an installed native release.

Experimental native macOS packaging/release infrastructure is implemented (ADR
0054). An owned C++ Finder launcher execs the bundled Firefly/main module. Firefly's
owned adapter discovers adjacent physical helpers/resources while retaining the
source-tree layout. The packager stages libraries and all ten pinned helpers,
removes absolute build RPATHs, checks dependency resolution, preserves helper
entitlements/flags and verifies ad-hoc bundle signatures. Its helper inventory is
guarded against upstream CMake changes. Common notices live inside each app and
archives; Rust crates without standalone notice files retain their distributed
source archive. Metadata records pins, host, source state, configurations and hashes.
Ladybird remains unmodified; no alternative runtime/renderer is bundled.

The Designer and framework bundles run from build/release candidate/ (spaces in the
relocated path): build/designer-relocated-test.log passes server + actual document
edit/Monaco/absent-Node smoke, and build/firefly-relocated-test.log passes the full
existing Electron-reference suite. All ten helpers' signature flags and entitlements
match the original binaries. The development Firefly gate still passes in
build/release-firefly-test.log. A separately configured Release Mite build passes
35/35 suites in build/release-mite-test.log (23.93s); linkage remains platform-only.
The local bundle candidate used Debug Mite + Release Firefly; the release driver
builds Release for both. SDK/Rust linker warnings remain. The conservative bundle
minimum macOS version is the build host, not a claim for older deployment targets.

The Native release workflow handles exact version tags/manual dispatch, native
macOS ARM64 and Windows x64 builds, required runtime/renderer/isolation/helper/demo
checks, separate runtime/framework/Designer archives and checksums, then draft
prerelease upload only after every platform passes. Published releases cannot be
overwritten. All Actions are SHA pinned; actionlint 1.7.12 (official checksum verified)
accepts both workflows. No version tag, complete hosted release build, native Windows
package or release upload was created/claimed. Current Windows Firefly/build/package
limitations explicitly fail the required job and block complete publication. GitHub's
manual workflow UI will require the file on the default branch; tag-triggered builds
use their tagged workflow. Native Windows CI/porting is the next release blocker.

The user-requested website/API docs and pinned runnable macOS demo are now delivered;
release infrastructure is present but the macOS+Windows release request remains
incomplete. Native Windows/Linux, unlocked desktop presentation, signing/notarization,
full license/security audit, complete Designer feature coverage and remaining public
preload/contextBridge/IPC requirements stay open. Follow the release and upstream
update guides; do not skip a required platform or weaken sandboxing for a green run.

Fresh-CI follow-up: the demo builder now distinguishes its own newly cloned
--no-checkout repository from a user-modified existing checkout. A first clone's
unmaterialized tracked files must not be rejected as user deletions. New clones are
shallow, then fetch the exact pinned commit; existing dirty sources still reject.
Both a new full checkout and the final shallow-clone path build successfully in
build/designer-fresh-build.log and build/designer-ci-build.log. The fresh staged app
also passes the actual Firefly boot/edit/Monaco gate using the new Release Mite for
its server checks (build/designer-ci-test.log). Release argument rejection and
workflow syntax checks pass. The release driver fetches locked Cargo sources before
offline notice collection; cargo fetch --locked completes without upstream changes.

The user rejected the first website's overall layout and requested a fresh design
closer to Node/Bun (with some Electron influence), plus direct commits/pushes to master
without pull requests. master now carries the existing implementation history; no
remote master branch existed previously. Publishing/source links/getting-started docs
now target master. The Pages environment explicitly permits master; production push
triggers no longer deploy the earlier implementation branch.

The homepage is rebuilt around oversized paired product wordmarks and owned SVG
Mite/Firefly characters, followed by a full-width three-example code workbench,
ruled project descriptions, a real Designer capture, an implementation ledger and
an engine overview. Paper/ink styling, green/yellow product accents and self-hosted
Bricolage/Plex fonts also update API pages. Fontsource 5.3.0 OFL notices/checksums are
preserved. The actual Node/Bun/Electron homepage captures were visually inspected;
no reference artwork or logos were copied. API/support claims remain conservative.

Local website validation: 69 pages and 1,151 relative link/resource/fragment checks;
all three tabs, keyboard selection, image/font loading and no document overflow at
1440/768/390/320 widths. Desktop/mobile homepage, content and API-page captures were
visually inspected in build/site-v2-*.png. Copy controls provide clipboard and selection
fallback; there is no server or framework runtime in the published site. No native
runtime or Ladybird source changed. The Windows CI setup was interrupted for this
user-requested redesign; its owned release-workflow/composite-action draft is saved in the local stash
`wip/windows-environment-before-homepage-redesign` for resumption. It does not
establish native Windows support.

The redesign was committed/pushed directly to master (d589ad3), master is now the
GitHub default branch, and Pages run 42113339017 completed successfully. CSS/JS/favicon
URLs are now content-addressed by the static builder to prevent cached assets from
the rejected layout being mixed into a fresh document. The same filenames are shared
by homepage and API docs; relative GitHub project-Page paths remain checked. No PR
was created. Earlier main/codex branches are preserved; future implementation commits
and website deployments use master as requested.

Native Windows CI is now being established before changing platform claims. The
restored owned composite action initializes the Visual Studio clang-cl x64 environment
and exports only required toolchain variables; release and development workflows share
it. The development workflow uses windows-2025, pinned Node/Rust, native pkg-config,
a pinned vcpkg bootstrap and binary cache, Release Mite plus all enabled CTest suites,
then preserves diagnostics and checks unmodified upstream even after failures. It has
no continue-on-error or WSL/alternate-engine fallback. Local actionlint accepts the
workflow/composite syntax; the first actual Windows run is pending the direct master
push. This establishes an execution path, not verified Windows support. Firefly's
native Windows and packaging/release gates remain incomplete. The saved draft stash
is retained as a backup; its changes are restored in the working implementation.

First native Windows run 42114085281 failed in checkout before compiler setup:
Git for Windows rejected our reserved device basename mite/tests/dns/nul.mjs.
Rename it to null-byte.mjs and update the DNS runner; keep the NUL-hostname test
behavior unchanged. A tracked-path audit finds this one reserved/invalid Windows
path in our repository and none in the pinned Ladybird tree. Local DNS differentials
pass after rename; Windows build/test support is still unproven until the next run.
No checkout filter or upstream edit is used to hide an incompatible source tree.

The second native Windows run (42114277910, job 127549626550, source 1cad9cc)
passed checkout, Python/Node setup, Visual Studio clang-cl initialization, native
pkg-config installation, pinned Rust selection and vcpkg bootstrap, and is currently
in CMake dependency configuration. Job logs are not available until upload (the
in-progress logs endpoint returns BlobNotFound); this is not a terminal build result.
Keep watching this exact run until an authoritative terminal status is observed.

A focused tracked-path guard now rejects the actual nul.mjs checkout regression,
other reserved device/character components and trailing periods/spaces. Five local
contract/index tests pass; an Ubuntu-only source preflight guards the real native
Windows job and release version job. This is not Linux runtime or complete Windows
path validation. Native pkg-config setup is shared with the release workflow.
Completed vcpkg archives will be saved even after later compile failures, and runs
on the same ref serialize instead of canceling an active dependency build. Workflow
syntax and upstream integrity pass locally. Native compile/runtime, Firefly and
Windows packaging/release support remain unverified; no SDK/vendor patch or alternate
engine is introduced. See docs/development/windows-build.md for the exact scope.


Documentation expansion: reviewed Node's Learn material and versioned 24.15.0 API
index plus Electron's introduction, API/process and security references as a baseline.
Added 22 application-author pages: documentation home, three concepts, eight guides,
Node/Electron migration, support policy, baseline/gap inventory, delivery roadmap,
documentation contribution rules and detailed CLI/app/BrowserWindow/webContents
contracts. Existing overview pages and README link these paths. The baseline separates
reference depth from implemented APIs and inventories missing runtime/desktop families;
versioned archives/search and complete per-method runtime entries remain work.

Validation: all source-relative Markdown targets exist; the static build resolves
92 total pages and 4,561 local links/resources/fragments with the accompanying
navigation draft. The new hash-file recipe passes on actual Release Mite and Node
for digest, missing argument and missing file; the documented HTTP example passes
real loopback GET and streamed POST echo. Desktop contracts were checked against
owned firefly.js and existing test evidence; no new native platform/security/desktop
acceptance is claimed. Upstream integrity passes, with no Ladybird/runtime changes.
Windows run 42114277910 still reports dependency configuration in progress; no
terminal native compiler/runtime result has been observed in this documentation step.


Documentation navigation follows the new author-facing structure: the Docs header
opens the documentation home; six collapsible groups expose guides, concepts,
reference, migration, compatibility and development. Current category/link are
marked, h2/h3 generate per-page outlines, and native details remain usable without
JavaScript. Mobile initially collapses menu/outline; desktop keeps the current
category visible. This changes documentation layout and the header destination;
the approved homepage design remains intact.

Validation: 92 generated pages / 4,561 local links/resources/fragments; no source
Markdown target is missing. Browser UI checks cover home, BrowserWindow reference
and baseline at 1440/768/390/320, including expanded-menu overflow, active category,
responsive details and outline fragments. Desktop/mobile captures were visually
inspected. Existing homepage tab/keyboard/image/font checks also pass at those
widths. Electron serves only as a website preview/test tool, not product runtime.
No native runtime test rerun is claimed for this documentation-only change.


Custom window chrome (ADR 0055): frame:false, native minimize/maximize/unmaximize/
restore and state queries/events now exist in the owned Qt adapter. An owned CSS
shim recognizes Electron app-region/-webkit-app-region drag/no-drag rules, with
bounded expiring top-document geometry and native mouse routing. No native binding
is installed in the page and Ladybird remains unmodified. titleBarStyle:hidden,
inline/shadow/frame/cross-origin/import/CSP/zoom cases and native resize-edge/full
non-client parity remain unsupported/unverified; public preload/IPC is unchanged.

Validation: Release native build; ChromeProbe passes real Qt input/position changes,
no-drag exclusion, double-click and stale expiration. The full Firefly suite passes
in build/chrome-firefly-test.log, including actual LibWeb CSS/media/layout/change/
removal/isolation and shared Electron frameless/minimize/maximize/restore states.
Firefly state events pass, but locked-host Electron maximize-event delivery did not;
reference event timing and visible native presentation remain unverified. No native
Windows/Linux acceptance is claimed. Designer shell/package changes follow separately.


Native Windows run 42114277910 is now terminal failure, not still configuring:
dependencies configured and initial native objects compiled, then the runtime JS
integrity gate correctly rejected CRLF conversion of tooling/runtime-js/build.mjs.
Add narrow LF checkout attributes for the hashed runtime builder/adapter inputs and
generated artifacts; do not normalize away a hash mismatch or edit upstream. The
core.autocrlf=true checkout regression verifies all protected bytes against the
existing manifest. Local runtime integrity and upstream guards still pass. Native
Windows compilation/tests beyond that gate remain unverified; the queued older
workflow may reproduce the failure until a commit containing this fix runs.


Designer custom-toolbar and smaller-package slice: the staged pinned app now uses
frame:false, Electron app-region CSS, real HTML minimize/maximize/restore/close
buttons and a six-action per-window capability. Exact token/Origin/Host/POST and
empty-body checks reject arbitrary channels/payloads; public preload/IPC remains
unsupported. The native smoke exercises the buttons through actual LibWeb and
closes through the HTML close button, not a main-process destroy shortcut.

The builder/packager support --dependencies production (default) versus all. npm
lock dev:true entries are omitted; shared/production optional packages stay. Two
contract tests cover nested/shared/scoped/optional packages, notices, all mode and
invalid/missing inputs. The pinned esbuild now bundles 19 browser app/package entry
points with shared chunks, CSS modules, preserved asset URL bases and five minified
Monaco worker entries. Classic bootstrap scripts, palette JSON, package metadata,
assets and license notices are retained. --no-bundle keeps source modules for
investigating optional features outside the measured bundle graph. DXF uses the
actual pinned package module entry; upstream HTML names a nonexistent file.

Release packaging strips local/debug symbols while keeping dynamic exports,
preserves helper entitlements/flags, re-signs/verifies and records SIZE.json. The
old candidate measured 466.2 MiB of actual Designer files; the first bundled/stripped
candidate is about 249 MiB (framework about 215.7 MiB), before compression. The
installed pinned Electron reference is 252.6 MiB without the Designer payload; this
is a scoped local measurement, not a universal size/performance claim. The initial
bundle dropped palette JSON; native relocation exposed the error, now corrected
and included in HTTP asset checks. Package.json remains available as runtime data.

Validation: fresh pinned source/staging build, production filtering, actual native
boot/edit/Monaco/minimize/maximize/restore/HTML-close/isolation smoke; fixed-action
capability negative tests run on Mite HTTP. A relocated repaired candidate passes
the same native test; its stripped framework passes the complete Firefly suite in
build/chrome-relocated-firefly-test.log. A clean packaging regeneration follows to
refresh provenance/signatures/checksums. The local .app path for that regeneration
is build/designer-verified-release/Web Component Designer.app. Strict helper
signature/entitlement comparison passed. Native pixel presentation still needs an
unlocked session; optional export/collaboration/GPU/LLM/full worker coverage is not
claimed. Ladybird is still the unmodified old pin pending the separate update review.

The macOS Release packages workflow builds/test/packages master snapshots in Release
mode with production bundled dependencies and stripped native symbols, uploads
archives/checksums/size JSON and reports actual sizes in its job summary. Manual
exact alpha tags may publish a clearly macOS-ARM64-only prerelease only after its
full native/package job succeeds; this is separate from the all-platform workflow,
whose Windows gate remains required for complete cross-platform publication.
actionlint, Python syntax, upstream integrity and 94-page/4,662-link site checks pass.
No hosted macOS package success or GitHub alpha is claimed yet; publish only after
an actual successful hosted build. Latest Ladybird candidate 97ad425b is fetched
for a separate pin/adapters/build/test step, with unchanged Rust/vcpkg versions.


Funding configuration: copied the user's existing Designer/layout2vector FUNDING.yml
into .github/FUNDING.yml. It names jogibear9988 on GitHub Sponsors and Patreon,
matching both existing project files. No funding transaction or account setting
changed. Validation: exact source-file comparison; publishing follows the authorized
master push with the current work.


Package-size audit: the clean 6c3bc8f/old-Ladybird-pin regeneration is complete at
build/designer-verified-release/Web Component Designer.app, with clean source
metadata, Release/Release builds, strict signatures/helper entitlements and a
passing relocated native smoke including HTML close. It measures 249.8 MiB of
actual files; Firefly hello-world is 215.7 MiB. The packaged Mite CLI also receives
symbol stripping. This is a local experimental candidate, not a published alpha.

The native dependency/symbol audit confirms no separate Mite CLI or second complete
LibJS engine in the app; versioned dylib aliases are symlinks. Firefly/LibCrypto do
share 903 OpenSSL public symbol definitions, a consolidation candidate without a
claimed byte estimate. Only 34 KiB of physical dylibs are outside the link closure.
Unicode's roughly 38 MiB is mostly constant data; dead stripping is already enabled
and release LTO remains disabled. docs/development/package-size.md records these
measurements and scoped next experiments. The new smoke assertion also checks that
actual external-toolbar drag/no-drag geometry covers the HTML maximize button;
it passes on the current Ladybird update candidate along with native actions.


Ladybird update (ADR 0056): pin/gitlink now advance together to
97ad425b5125f20c516e1a8d3b5bb02f391fc975, 166 commits after the previous pin.
Rust/vcpkg pins stay unchanged. Reviewed Rust callback delegation, narrow helper/
callback/timer/listener/observer seams and curated Qt source dependencies. Upstream's
new plain-text tooltip entry points otherwise pull in browser TabBar code; our owned
bounded Qt::PlainText label adapter supplies only those embedding functions, with
native markup/bounds/hide tests. No vendor source or sandbox policy changed.

Validation: Release Mite and all 35 CTest suites pass (19.46s); updated Release
Firefly builds; all four runtime/renderer/isolation/helper gates pass in
build/ladybird-update-firefly-test.log. The bundled Designer also passes with the
new actual external-CSS geometry assertion and all HTML window controls in
build/ladybird-update-designer-test.log. Upstream integrity, 48,247 vendor paths,
LF checkout regression, workflow syntax and 95-page/4,716-link docs pass. Existing
Rust/SDK linker warnings, native Windows/Linux and unlocked pixel presentation
remain open. Windows cache restore can reuse prior archives across a source-pin
update; vcpkg still validates individual package ABI keys before using them.

Next for the user-requested alpha: push the accepted update and funding/package
commits to master, observe the actual macOS Release package job through completion,
fix any hosted-only failures, then publish a clearly macOS ARM64 alpha only after
its native/relocated gates pass. No GitHub release or hosted success is claimed yet.
The all-platform workflow retains its Windows requirement for a complete release.


Native Windows compile follow-up: run 42121184088 (1c5f535, job 127575365884)
passed the corrected checkout/integrity gate and then failed in our FileSystem.cpp
on absent S_IFSOCK/S_IFBLK/O_SYNC identifiers. Guard optional native file-type
cases/exports and use libuv's portable UV_FS_O_SYNC rather than a fabricated value.
Local Release compilation and all existing filesystem differentials/policy fixtures
pass in build/windows-fs-portability-test.log. A first relative-path runner invocation
was invalid because the runner changes cwd; the validated invocation uses the full
executable path. Upstream is unchanged. Actual Windows compile/runtime acceptance
still awaits a new job; do not infer support from macOS checks.


Relocated Designer smoke follow-up: the updated engine package regenerates to
249.4 MiB (framework 215.3 MiB), but one smoke launch timed out while another using
the same binaries/assets passed. Fixed, test-only startup stages locate the stall
after geometry and during minimize/restore. The old renderer test awaited HTTP/
timer completion while minimized before asking that same renderer to restore the
window. Visibility throttling can suspend the very work it awaits.

The test owner now observes the real native minimize event, records a required
DESIGNER_NATIVE_MINIMIZE_OK marker and restores through BrowserWindow from the main
process after 350 ms. The renderer then confirms restored state and closes through
the real HTML button. Production has no automatic restore. This removes the test
liveness dependency without skipping minimize or faking success. Bounded fixed-name
stage diagnostics remain --smoke-only, and actual stdout/stderr are retained in
build/designer-last-ui.log. The complete native test passes with the packaged runtime
and the re-signed relocated diagnostic app in build/designer-restoration-{test,
relocated-test}.log, including the observed native minimize and HTML close markers.
A final package regeneration follows; do not publish the earlier intermittent fixture.
The already-running hosted aefccef job predates this test fix; keep its result distinct
from the next exact-source job used for alpha publication.


Alpha workflow scope is explicit: v*-alpha.* tags do not trigger the complete
all-platform release matrix. They use the separately gated manual macOS-only
workflow; other version tags still require macOS and Windows, with no skipped
platform acceptance. This avoids launching a misleading complete-release job for
a deliberately limited alpha. actionlint accepts the negative tag filter. No tag
or release is created before the requested actual hosted package success.


First hosted macOS Release run 42123747221 (aefccef, job 127582471245) is terminal
failure during vcpkg libidn2 configuration: system autotools prerequisites lack
autoconf-archive macros. The later Ninja/compiler errors are consequences of the
aborted toolchain setup, not evidence Ninja was absent. Add autoconf-archive to both
native macOS workflow prerequisite commands and source-build requirements. The pinned
vcpkg-make source explicitly checks these macros. No dependency/source guard is
bypassed; actionlint passes. Completed dependency downloads were preserved.

Run 42125657222 was started with the same known-incomplete prerequisite command;
replace it with the corrected-source run rather than claiming either build passed.
The alpha remains conditional on actual hosted compilation/native/package success.


Windows run 42125657167 (4ec4360, job 127589126627) now passes the filesystem
compile barrier and reaches object 297/300. Its embedded-host target fails in MSVC
thread because our Process.h shadows the CRT process.h on Windows's case-insensitive
include path, hiding _beginthreadex. Rename the private header to ProcessHost.h and
update the three owned includes. Local Release compilation and both host-module/
embedding CTest suites pass (1.77s); native Windows still needs the next actual job.
No SDK/vendor patch or skipped host target is used.

The clean 004b7fe package with updated Ladybird, bundled production assets, native
symbol stripping and corrected minimize test is regenerated at
build/designer-final-alpha/Web Component Designer.app. Its full relocated test
passes in build/designer-final-alpha-test.log, including main-owned restoration
and actual HTML close. Metadata is clean Release/Release with the exact source/pin;
actual file sizes are 249.43 MiB Designer and 215.30 MiB framework. This is a local
candidate only; hosted run 42125911196 is still in its compile/test/package step.
Its predecessor failed the missing autotools macros; no hosted alpha is claimed.


Alpha publication is now the last step after asset upload: create/reuse only a
draft, attach all verified archives/checksums/size reports, then publish it as a
prerelease (not latest). Existing published releases refuse overwrite. This avoids
a visible partial alpha if uploading fails and supports retrying an incomplete
draft. actionlint passes. This changes publication orchestration only; no alpha
tag or release has been created and hosted native success is still required.


macOS dependency cache follow-up: the first failed configure saved a partial cache
under the exact pin key. Restoring that immutable key then prevented later runs
from saving newly completed packages. New macOS runs use a unique save key plus
pin-specific/general restore prefixes, so successful ports/downloads accumulate
after both success and failure. vcpkg's individual ABI keys still decide reuse;
no compiled app is accepted from this dependency cache. actionlint passes. The
currently active 004b7fe run uses its original cache policy; do not infer that the
new policy has run there or cancel its active build just for a cache improvement.

Next: establish native Windows CI, implement/validate the missing desktop/process/
sandbox adapters and packaging, then unblock the requested macOS+Windows Designer
release upload. Keep required jobs failing honestly until their native gates pass.
Continue remaining Source API/callback boundaries and event-field conversions,
callback membranes and validated IPC with resource/sender/frame limits, alongside
Windows feasibility.
Broaden host APIs and real-package coverage around
those vertical slices. CommonJS named-export detection, synchronous
require(ESM), eval CJS globals and require.resolve options remain unsupported.
Timer refresh, timeout warnings, immediates and process events remain
tracked as unsupported in the compatibility matrix.

## Standalone Rust aggregation (2026-11-06)

Native Windows run 42127377539 (c98fbf1, job 127594989171) passes the
ProcessHost.h compile barrier but fails linking duplicate Unicode/regex/initializer
symbols from independently built Rust staticlibs. The owned standalone graph now
links a single Cargo aggregate with the five upstream Rust roots/features, retaining
the original FFI-header builds and C++ initializers. Pinned package inventory and
--locked prevent independent dependency upgrades; drift tests reject version,
checksum, source and name changes. Static ELF duplicate-definition suppression is
removed in this graph. No Ladybird edits, force-link override, Firefly dependency
or renderer-engine duplication is introduced (ADR 0057).

Local Release Mite builds and all 37 CTest gates pass (19.40s), including the two
lock checks. Build/test evidence: build/rust-aggregate-{configure,build,test}.log.
The first aggregate link has no duplicate symbol definitions; Apple ld may still
warn that the same archive path is referenced through several imported targets.
otool shows no LibWeb/Qt dependencies. Actual unstripped CLI bytes increase from
51,254,056 to 51,286,136; no size savings are claimed. This does not optimize the
215.30 MiB local framework candidate. Its separate duplicate OpenSSL code, disabled
release LTO and Unicode/resource costs remain measured optimization candidates.
Site validation passes 97 pages / 4815 local links/resources/fragments.

Native Windows acceptance awaits the next hosted job. Hosted macOS run 42125911196
is still in progress at compilation/test/package; no alpha has been published.
The persistent implementation goal remains active and upstream stays unmodified.

## Explicit LTO experiment (2026-11-06)

Owned renderer/release drivers accept --lto, keeping defaults off. Standalone
CMake accepts ENABLE_LTO_FOR_RELEASE as a cache option. Requested but unsupported
IPO fails explicitly; package BUILD.json records both projects' cppLTO settings.
This enables upstream's C++ optimization without editing Ladybird or enabling
Rust cross-language LTO/JIT. Normal runtime/renderer/security/package gates remain
required. Local Release Mite emits -flto=thin, builds, and passes all 37 tests
(28.51s under concurrent renderer compilation); logs are build/lto-mite-*.log.
Actual unstripped CLI size is 50,675,688 bytes versus the same-source non-LTO
51,286,136 bytes: 610,448 bytes smaller. No performance claim follows from test time.

The local LTO renderer build is live in exec session 86784, logging to
build/lto-renderer-build.log; it requires a full affected-object rebuild. Framework
size and native sample/security results remain pending. Do not enable CI defaults
or publish an LTO package on the CLI-only evidence. Existing non-LTO verified
packages remain in build/designer-final-alpha. Windows run 42130072857 is compiling
the aggregate source; macOS run 42125911196 remains live. No alpha is available yet.

## Native Linux runtime gate (2026-11-06)

Add the missing native Linux standalone job on Ubuntu 24.04 x64 with Clang 22
from the official LLVM Noble repository, Rust 1.98.0, CMake 4.2.0 and our pinned
vcpkg manifests. It builds all Mite test hosts, runs every enabled CTest suite,
locked npm differentials and CLI examples, and records ldd. No GUI graph or
Node/V8 runtime replacement is introduced. Cache saves preserve newly completed
dependencies even after failure; Cargo/CMake/vcpkg/CTest diagnostics are retained.
actionlint accepts the workflow. Compiler repository and CMake distribution were
verified against their primary sources linked in development/linux-build.md.

The first hosted result is pending; Linux runtime/Firefly/package support remains
unverified. Existing local LTO renderer session 86784 continues (past 1,000 of
2,951 affected steps); native Windows run 42130072857 remains at compilation and
macOS run 42125911196 remains live at its compile/test/package step. No restarted
job or alpha publication is inferred from the elapsed observation time.

## Crypto reference depth (2026-11-06)

Add a dedicated node:crypto reference for the implemented four exports, covering
parameters/limits, return/callback ordering, native error fields, Hash lifecycle,
stream/manual use, provider limits and explicit absent APIs. It links existing
random/hash examples and their verification gates rather than adding duplicate
sample programs. Detailed current-subset contracts derive from owned source and
Node differential fixtures; the pinned Node 24.15 primary reference is linked
as the compatibility baseline, not as an assertion that its full API exists.
Add grouped reference navigation and links from both overview pages and the
documentation-baseline inventory. No runtime implementation or platform status
is changed. Both existing examples pass under Mite and Node, including the SHA-256
abc vector and independent snapshot check. Site validation passes 99 pages and
5126 local links/resources/fragments (build/crypto-reference-site.log).

Local LTO renderer session 86784 and hosted native jobs remain live; their
completion and framework-size results are independent of this documentation slice.

## Windows links; diagnose VM startup (2026-11-06)

Windows run 42130072857 (390b86d, job 127604194189) is terminal failure in CTest,
after native compilation/linking succeeds. Five of 37 suites pass; runtime hosts
exit 0xc0000005 at VM startup even for -e, while CLI help/version work. This confirms
the Rust aggregation resolves the native link barrier, not runtime acceptance.
The separate upstream-guard fixture fails deleting Windows read-only Git objects.

Move that isolated fixture's .git directory away instead of manually deleting it;
the same uninitialized-checkout assertion and all seven local guard tests pass.
Add a Windows-x64-only startup probe using native exception-context/DbgHelp stack
reporting, with a deliberate fault negative control and then the real Mite eval.
It preserves actual failure status and is absent from the shipping CLI. The CI
development build retains C++/Rust symbols, the bounded report and probe PDB.
actionlint and Python compilation checks pass; the Windows-specific C++ probe and
symbolization remain pending actual native compilation. No guessed engine fix,
exception recovery, vendor modification or skipped runtime suite is introduced.

Local LTO renderer session 86784 now completes successfully. Native renderer,
Firefly/Electron, isolation and helper tests are running in
build/lto-firefly-test.log; actual framework package size remains to be measured.
Linux run 42130905759 and macOS package run 42125911196 remain live.

## LTO framework measurement and acceptance (2026-11-06)

The complete local LTO renderer build succeeds, as do check-firefly,
check-firefly-renderer, check-firefly-isolation and check-firefly-renderer-helper
(build/lto-firefly-test.log). Real Designer boot/edit/Monaco/HTML controls pass
both before packaging and through the relocated .app launcher:
build/lto-designer{-test,-relocated-test}.log. The clean a377d04 candidate is at
build/designer-lto-experiment, version 0.1.0-lto.1, Release/Release with cppLTO ON,
production bundled assets, stripped native symbols and valid ad-hoc signatures.
All 7190 package hashes match. This is a local experiment, not a published alpha.
Final same-candidate Mite CTest also passes all 37 suites in 16.63s, including
the revised Git fixture (build/lto-mite-final-test.log).

Actual results contradict the hoped-for framework saving: framework 227,251,879
bytes (216.72 MiB), Designer 263,042,987 bytes (250.86 MiB), versus earlier non-LTO
004b7fe candidate 225,757,294 / 261,548,402 bytes. Net growth is 1,494,585 bytes:
native executables -612,128; libraries +2,136,720 (LibWeb +1,981,504); notices
-30,007. App assets are unchanged in byte count. This is a recorded-candidate
comparison, not an identical-commit A/B or performance benchmark. Keep LTO optional
and release defaults off; pursue size-focused compiler settings/shared crypto next.
Both notice inventories contain 350 Rust packages; every fallback source-archive
payload matches, despite differing archive bytes. No notices/source are removed.
Documentation validation passes 99 pages and 5130 links/resources/fragments.

Windows 42130479203 uses the same VM code as the confirmed failing run and lacks
the new fault reporter; it was explicitly cancelled to let queued diagnostic run
42132246241 proceed, not to erase a failed gate or imply success. The actual failure
42130072857 remains recorded. macOS package 42125911196 and Linux 42130905759 remain
live. No alpha is created before hosted native package acceptance.

## Checked size profile and null-safe SDK deallocation (2026-11-06)

Add optional --optimize-size to renderer/release drivers and owned Release CMake:
-Oz (/O1 on MSVC-compatible drivers) follows upstream options on C++/Objective-C++
targets. Rust/vcpkg profiles and feature/resource/helper inventories are unchanged.
A generated-command guard confirms the final flag is selected, not overridden
by upstream -O3; it checks 277 standalone / 2894 renderer commands locally.
Package metadata records each project's enabled setting and chosen flag.

The first local -Oz CLI aborts in Rust execution_context_from_abi. Disassembly
of initialize_host_defined_realm proves unconditional deallocation of moved-from
owners. C++ permits calling class-specific deallocation for null delete operands,
but upstream's SDK operator forwards null to the Rust non-null destroy contract.
The owned ExecutionContextAdapter.cpp includes the unchanged original TU and
routes only that destroy call through a null guard. Configure/build exact-body /
call-count and exactly-one-TU checks require review on upstream drift. The pinned
Ladybird checkout remains untouched. ADRs 0058/0059 record contract and scope.

A native SDK regression directly exercises null deletion, real VM/realm ownership
movement and cleanup. All 38 local Mite tests pass with the adapter under -Oz
(26.26s, build/size-mite-adapter-{build,test}.log); without it the startup panic is
retained in build/size-mite-*-trace.log and size-realm-disassembly.log. The LLDB
launch produced no observation and its owned debugger processes were terminated;
no debugger result is claimed. Unstripped CLI is 50,793,784 bytes. Framework savings
remain unproven. Initial renderer size build session 93969 remains live; after it
finishes, reconfigure/rebuild to include the new adapter before native acceptance.
Do not treat its original in-flight graph as already adapted. Defaults remain off.
Exact-seam drift and an actual generated-command -O3 override are rejected in
negative controls. Site validation passes 101 pages / 5231 local links/resources/
fragments. Native Windows and Linux acceptance of this new adapter/profile remain
pending; the separate pre-adapter Linux baseline result is recorded next.

## Windows diagnostic SDK annotation (2026-11-06)

Diagnostic run 42132246241 (a377d04, job 127612697222) is terminal compile failure
in our new probe, before runtime diagnostics: AK/Windows.h intentionally undefines
legacy IN, but DbgHelp.h still uses it. Save/restore that macro around the SDK
include only; no Windows SDK or Ladybird edits and no change to shipping runtime
behavior. The actual compiler output identifies the missing annotation. This fixes
the observed include seam; native probe compilation/stack reporting and the original
VM access violation remain unverified until the next job. Python/actionlint checks
pass; no native Windows success is claimed from the local source inspection.

## Native Linux baseline accepted (2026-11-06)

Run 42130905759 at ff51eab (job 127606954442) is terminal success on Ubuntu 24.04
x64 with Clang 22 / Rust 1.98.0. All 37 enabled Mite suites pass in 14.77s, including
host embedding, modules/packages, event ordering, FS/network/DNS/HTTP, OS, random/
hash and real npm fixtures against Node 24.15.0. CLI hello/ESM/CommonJS examples
also run successfully. ldd lists libatomic, libm, libstdc++, libgcc_s, libc and the
normal loader; no LibWeb, Qt, Node or V8 runtime dependency is present. Upstream
integrity passes after the build. This establishes the documented subset on that
native host/source, not broad distribution or full Node compatibility.

Update central support/build/API/compatibility documentation to record this exact
baseline rather than continue saying Linux was never run. Firefly Linux/windows,
desktop sandbox/helpers and packages remain unimplemented/unverified. The baseline
predates the new execution-context adapter and size profile; their next Linux job
must independently accept them. No prior success is used to certify new flags.
Site validation passes 101 generated pages / 5243 local links/resources/fragments
(build/linux-baseline-site.log).

## Manual hosted size-profile selection (2026-11-06)

The macOS workflow exposes optional optimize_size (false by default) and forwards
it as a fixed CLI flag to the same release driver. Native/runtime/renderer/security/
Designer/relocation gates are unchanged. The summary reports actual recorded C++
LTO/size settings alongside package bytes; older metadata is explicitly "not
recorded". Push defaults do not change and no profile is promoted to a successful
release by selecting a flag. actionlint and site validation cover this orchestration
change; an actual hosted size-profile build remains pending.

Local size renderer build completes and is reconfigured/relinked with the owned
execution-context adapter. Full renderer/Firefly/isolation/helper tests are live
in session 86887 / build/size-firefly-test.log before Designer/package measurement.
The obsolete f1a90a9 Windows run 42134116438 was cancelled because it predates the
confirmed DbgHelp annotation fix and would repeat the same known compile failure;
the corrected 5b612ed run 42134241990 is now live. The prior failure remains recorded.

## Accepted local size-profile package (2026-11-06)

The size renderer is now reconfigured/relinked with ExecutionContextAdapter.cpp;
the generated-command guard checks all 2894 C++ commands. All four native gates
pass in build/size-firefly-test.log: Firefly/Electron, renderer/compositor/process,
isolation and registered-helper security. The real Designer workflow passes both
before packaging and through the relocated .app launcher, including edit/round-trip,
Monaco, fixed-action capability rejection, observed minimize and HTML close:
build/size-designer-test.log and build/size-designer-relocated-test.log.

The clean 5625bfb local package at build/designer-size-experiment is version
0.1.0-size.1, Release/Release, C++ size ON (-Oz), LTO OFF, production bundled assets
and stripped symbols. Strict ad-hoc signatures and all 7190 file hashes verify.
Framework is 212,568,119 bytes (202.72 MiB), Designer 248,359,227 (236.85 MiB):
13,189,175 bytes / 12.58 MiB below the earlier default 004b7fe candidate (5.84% /
5.04%). Executables -591,136, libraries -12,568,032 (LibWeb -8,979,552), notice
archives -30,007. App/resource byte counts and helper inventory stay unchanged;
Rust notice inventories and all 19 fallback source-archive payloads match.
This compares recorded candidate commits, not identical-source A/B or performance.
No feature or required notice is removed. Hosted profile/default changes remain
pending; the package is not a published alpha.

Linux 42134116536 at f1a90a9 (job 127618157946) also completes successfully:
38/38 tests in 20.96s, including the new SDK null-deallocation regression, CLI
examples, normal system-only ELF linkage and upstream integrity. This accepts the
adapter under default Linux Release, not the untested Linux size profile. Windows
42134241990 (job 127619613442) is now terminal failure: 7/38 suites pass, including
the SDK probe's real VM/realm construction and null/moved-owner cleanup. Thus the
remaining full-Mite startup fault is not proof that basic engine construction fails.
The corrected reporter's deliberate-fault control produces symbolized frames;
actual Mite startup executes address zero (0xc0000005). Its first reporter cannot
unwind a zero PC; collect a bounded caller candidate before guessing a runtime fix.
macOS 42125911196 remains live in the full release step. Site validation passes
101 pages / 5248 local links/resources/fragments.

## Windows zero-PC caller diagnostics (2026-11-06)

Run 42134241990 at 5b612ed is terminal failure after successful compilation:
7/38 tests pass, including real VM/realm SDK construction/deallocation and the
fixed Git guard. The deliberate-fault control produces native symbolized frames;
real full-Mite evaluation faults executing zero, so the old zero-PC loop emits no
caller frames. Extend only the diagnostic probe to read one pointer-sized captured
stack candidate safely with ReadProcessMemory, require a loaded module, label it
as a candidate and unwind a local copied context. This is not execution recovery
or a guessed runtime fix; an indirect tail jump may require further investigation.
Print native access metadata and preserve the actual exception status.

Move the real startup gate immediately after compilation so a known startup fault
fails before npm/broad runtime work. Once it passes, every enabled suite still
must run/pass. actionlint/Python checks cover orchestration; the new Windows-only
unwind path awaits native compilation and actual trace evidence. Local tested
size-profile app and default Linux acceptance remain separate completed evidence.

## Windows startup cause: libuv before process queries (2026-11-06)

Run 42136659095 at 9752348 (job 127626480547) fails honestly at the early startup
gate; broad suites are skipped after that failure, not treated as successful.
Its native access record is execute/address zero. The recovered caller is
uv_os_getppid+0x29, followed by Mite::create_process at Process.cpp:101 and run at
Runtime.cpp:142. This corroborates an indirect call in the pinned libuv 1.52.1
Windows source: pNtQueryInformationProcess is loaded by uv_loop_init's one-time
initialization, but uv_os_getppid does not initialize it itself. Our EventLoop was
constructed only after create_process had already queried ppid.

Move the existing owned-loop construction before process queries, retaining realm
roots and normal shutdown. No private libuv function, throwaway loop, guessed PID,
vendor patch or runtime fallback is introduced. A fresh child-process regression
compares ppid to the actual Python launcher under both Node and Mite, before timers
or I/O could mask startup order. All 38 local Release/size-profile CTest suites pass
(18.05s, build/libuv-startup-{build,test}.log), including both fresh-parent checks.
check-firefly also passes its native/lifecycle/ESM/HTTP/control gates in
build/libuv-startup-firefly-test.log. Actual Windows acceptance remains pending the
next job; the local checks are not relabeled as Windows runtime evidence.

## Read-only Rust notice inventory audit (2026-11-06)

Add tooling/audit_rust_notices.py, which compares the actual package's pinned Rust
inventory with offline/locked default/all-feature Cargo metadata, unfiltered and
target-filtered. Traverse all workspace roots and dependency kinds (including
build/dev), keep package IDs opaque, reject incomplete graphs/feature expansion,
and require the baseline Ladybird pin. The emitted report is explicitly analysis
only, never changes package notices and reports no net saving.

Actual ARM64 macOS audit of build/designer-size-experiment finds 350 baseline,
402 unfiltered-union and 316 platform-union external packages. There are 51
excluded candidates occupying 7,680,055 bytes (7.32 MiB gross), but also 17 added
feature candidates. Large excluded candidates include Windows import-library
source archives; actual build target/host/flags/roots/features and generated inputs
still need matching before a distribution mode can safely narrow inventory.
The current tested app sizes and default packager are unchanged. Three focused
graph tests cover cyclic/renamed/build/dev retention, missing nodes and incomplete
all-feature expansion; the actual audit report is build/rust-notice-audit.json.

## Bounded Windows prerequisite retry and Linux startup acceptance (2026-11-06)

Windows runs 42138308876/d9b1ec0 (job 127632017561) and 42138486271/db0b611
(job 127633175751) are terminal failures before compilation: the Chocolatey
pkgconfiglite feed returns HTTP 504 and 503 respectively. Neither exercises the
libuv startup fix. Retry the same pinned 0.28 install at most three times with
15/30-second backoff, preserving installation/executable failure as a failed job.
No version fallback, skipped prerequisite or runtime-test relaxation is introduced.
actionlint, diff whitespace and upstream pin checks pass locally; PowerShell and
the actual retry/startup behavior still require native Windows CI.

Linux run 42138308785 at d9b1ec0 (job 127631969080) passes all 38 suites in
21.16s, CLI/linkage and upstream integrity after the libuv construction-order fix.
This is default native Linux Release acceptance, not desktop or size-profile
acceptance. macOS run 42125911196 remains authoritatively in progress at the full
Release step; it is not cancelled/restarted due to its duration. Hosted package
acceptance and alpha publication remain pending.

## Native bounded random integers (2026-11-06)

Implement node:crypto.randomInt([min,] max[, callback]) in the existing owned
facade using native OS/libuv entropy, with CJS/ESM named exports. Validate safe
integer endpoints and a positive range below 2**48; use six-byte rejection sampling
to avoid modulo bias. Async retries reuse the existing rooted native-worker path,
preserve entropy errors and keep pending work alive. There is no new dependency,
renderer privilege or Ladybird change. No integer cache is introduced: callback
timing can differ from Node's warm-cache nextTick path, explicitly documented in
the API reference, compatibility matrix and ADR 0060.

The new mite-crypto-random-int suite compares 48 validation cases with Node
24.15.0, checks 448 native bounded samples (negative/48-bit/safe-integer endpoints),
48 concurrent callbacks, CJS/ESM identity, return/receiver/error shapes and fatal
callback handling. An isolated Node VM evaluates the facade with controlled
entropy for both rejection boundaries, delayed retry, single completion and error
identity; no shipping entropy override is added. The random CLI example now uses
the API and runs under both Node and Mite. All 39 local Release/size-profile CTest
suites pass in 16.34s (build/random-int-{build,test}.log).

check-firefly passes native controls, lifecycle, ESM/TLA/OS/HTTP, isolation,
crash/error and shutdown gates (build/random-int-firefly-test.log); its ESM fixture
now checks sync and async randomInt in the actual main process. Site validation
passes 102 pages / 5305 links; actionlint, Python syntax and diff checks pass.
New API hosted Linux/Windows acceptance remains pending, and the earlier tested
202.72 MiB package is not relabeled as containing this change.

Windows run 42139627506 at 07ff79a passes the formerly failing prerequisite step
and is live in compilation; no full runtime acceptance is claimed yet. macOS run
42125911196 remains live in the Release step, so alpha publication is still pending.

## Native HTTP client vertical slice (2026-11-06)

Add ClientRequest/request/get to node:http, reusing native TCP, OutgoingMessage,
IncomingMessage and the pinned llhttp parser. The owned parser now has explicit
response/HEAD modes, bounded status text and status metadata; its request default
preserves the server path. Support URL/options overloads, fixed/chunked streamed
uploads, interim/HEAD/204/EOF responses, headers/trailers, read pressure, socket
timeouts, abort/destroy and error cleanup. Generate builtin ESM export aliases
through private local names so public get cannot collide with the loader binding.
Capture URL through the builtin host, without a mutable-global dependency.

Each client request owns one closing connection. Agent/pooling, HTTPS/TLS/fetch,
upgrades/CONNECT, custom transports, AbortSignal and local binding remain explicit
unsupported work. No Node/V8/LibWeb runtime dependency or Ladybird patch is added.
ADR 0061 and the new HTTP API reference document the partial surface and limits;
the client example runs alongside the existing loopback server example.

tooling/tests/test_http_client.py compares actual Node/Mite traffic with an independent
Python TCP peer: GET/POST/PUT, URL/options, streamed/chunked uploads, fragmented
headers/bodies/trailers, duplicates, HEAD/204/EOF/interim responses, paused 2 MiB
payload digest, malformed/oversized/ambiguous framing, truncated-body errors,
lookup failure, timeout, abort and close state. Sixteen argument cases preserve
observed Node error/legacy option-spread behavior. Separate policy checks reject
unsupported options/upgrades and mutated path/header injection; fatal callbacks
retain failure status. The final targeted suite passes in
build/http-client-differential.log. All 40 local Release/size-profile CTests pass
in 18.53s (build/http-client-{build,test}.log), retaining all prior server suites.
check-firefly passes with an actual main-process client/server exchange in the ESM
migration fixture before the separate native renderer load
(build/http-client-firefly-test.log). Site validation: 104 pages / 5518 links;
actionlint, Python syntax, diff checks and upstream integrity pass. Hosted client
acceptance is pending; earlier package sizes do not include this new source.

Linux 42140140126 at 6ed8a21 (job 127638194072) passes 39/39 suites in 21.08s,
including native randomInt. Windows 42139627506 at 07ff79a (job 127636462060)
now passes real native startup and 25/38 suites: the libuv order fix works.
Thirteen suites still fail, several before/during Python text I/O with the Windows
locale codec; other path/filesystem differences and a Node-reference network
timeout remain to diagnose. This is not full Windows acceptance. macOS release
42125911196 remains live; no alpha release is claimed.

## Windows harness text encoding (2026-11-06)

The first post-startup Windows suite result (42139627506, 25/38 passed) exposes
fixture writes and captured UTF-8 runtime output going through Python 3.12's
locale ANSI codec. Hash/OS/path fixtures fail to encode Unicode source, and
OS/encoding/buffer/URL readers fail to decode UTF-8 bytes. Enable PYTHONUTF8=1
through the shared native Windows action's GITHUB_ENV for subsequent Python
processes, including child test launchers. This changes the harness text contract,
not Mite/Node argument conversion or filesystem results. No failed suite is skipped
or normalized away. actionlint, diff and documentation-site checks pass locally;
actual Windows text I/O and remaining native failures await the next job.
The CLI Unicode argument, module/package path and filesystem differences plus the
Node-reference backpressure timeout remain separate unresolved evidence.

## Windows wide CLI argument entry (2026-11-06)

The first actual post-libuv suite (42139627506) still fails arguments.js with
non-ASCII arguments. Replace only the Windows shipping CLI's narrow main with
wmain and explicit checked UTF-16 -> UTF-8 conversion via WideCharToMultiByte.
Use CP_UTF8/WC_ERR_INVALID_CHARS, size the destination from the native API's byte
count including NUL, retain all string storage and argv pointers through run(),
and terminate argv with nullptr. Conversion errors fail with exit 9 before JS.
The runtime embedding contract remains UTF-8; native hosts choose their own entry
boundary. The pinned Windows uv_setup_args simply returns the supplied argv.
No locale/codepage change, command-line re-tokenizer or Ladybird patch is needed.

Extend the shared runtime suite with non-ASCII eval source plus Unicode, empty,
quoted, spaced, trailing-backslash, tab and combining-character arguments under
both Node and Mite. On native Windows, an unpaired surrogate must reject explicitly
before its JS marker executes. All 40 local macOS Release/size-profile suites pass
in 16.48s (build/windows-argv-{build,test}.log); site checks pass 104 pages / 5522
links, actionlint and diff checks pass. These local checks do not compile or certify
wmain: actual Windows acceptance remains pending the next native job. Unicode
script/module filenames, interactive console and other filesystem/network failures
remain separate unresolved platform work. Update CLI/Windows docs accordingly.

## Native module path representation and Unicode entry files (2026-11-06)

Separate native filename strings from generic file-URL path strings in our
ModulePaths/ModuleLoader adapter. Public filename/dirname/module.paths values now
use preferred native separators and UTF-8; URL construction keeps generic
separators before percent escaping. Runtime entry resolution/opening now converts
UTF-8 arguments explicitly to std::filesystem native paths and retains that path
for actual file operations, avoiding the Windows ANSI path constructor/string
conversion. No Ladybird modification or extra runtime dependency is introduced.

Correct the package fixture's Unix-only __dirname + '/node_modules' assertion to
path.join. It still compares actual Node/Mite module.paths, without filtering the
results. The ESM error fixture now checks missing drive on Windows vs nonlocal host
on POSIX. The loader validates Windows file-URL drives explicitly. Valid UNC module
URLs remain unsupported and now fail with ERR_MITE_UNSUPPORTED_API on Windows;
a separate native-only policy test checks this without a Node SMB network probe.

New mite-module-paths creates Unicode entry filenames/directories, ESM dependencies,
CommonJS bridge imports and npm package main targets, including URL #/% escaping.
It compares actual payloads, import.meta URL/filename/dirname, argv entry,
require.resolve and module.paths metadata with Node. All 41 local macOS
Release/size-profile suites pass in 18.06s (build/native-module-paths-{build,test}.log),
and check-firefly passes native controls/lifecycle/ESM/HTTP/renderer/error/shutdown
gates (build/native-module-paths-firefly-test.log). Documentation site, actionlint
and diff checks pass. Update CLI/module/platform docs and compatibility scope.

Actual Windows wide-entry/Unicode path acceptance remains pending; local tests do
not certify the Windows conditional paths. Windows run 42141915944 at 1df78c8 is
live in dependency bootstrap; Linux HTTP-client run 42141507395 at d1c9513 remains
live in compilation. No active build is restarted due to observation duration.
Native filesystem API differences and the Node-reference network timeout remain
separate work, as do UNC support and interactive console behavior.

## Windows filesystem namespace boundary and fair path comparisons (2026-11-06)

The first real Windows filesystem output (42139627506) shows Node returning
absolute error paths and a namespaced first-created recursive mkdir path while
Mite uses the original relative path. Node's native binding prepares namespace
paths before libuv. Our fs facade now captures the existing path adapter, prepares
Windows string/UTF-8 Buffer paths before native work, preserves readdir trailing
separators and keeps Dirent.parentPath as supplied by the caller. Native error
path/dest fields strip the internal namespace prefix; mkdir keeps its actual
first-created native path. POSIX path bytes and descriptor behavior are unchanged.
No vendor source is patched.

The differential filesystem harness now recreates the same owned work directory
between Node and Mite, rather than comparing absolute output from distinct 0/1
roots. Output and file bytes remain compared directly, without path rewriting.
New paths.mjs exercises recursive mkdir, Unicode file URL/Buffer identity,
sync/callback-promise error source/destination fields and Dirent parent metadata.
The targeted filesystem suite passes, and all 41 local Release/size-profile CTests
pass in 17.22s (build/windows-fs-path-{build,test,full-test}.log). check-firefly
passes native controls/lifecycle/ESM/HTTP/renderer/error/shutdown checks in
build/windows-fs-path-firefly-test.log. Site, actionlint, Python syntax, diff and
unmodified upstream checks pass. Native Windows namespace acceptance is pending;
the scalar descriptor file-content mismatch and Node-reference network timeout
remain independent unresolved failures, not skipped or normalized away.

Linux run 42141507395 at d1c9513 (job 127642655545) is now terminal success:
40/40 suites in 18.88s, including native randomInt and HTTP client, plus CLI,
linkage and integrity gates. Update API/matrix/Linux docs to this source-scoped
baseline. It does not accept subsequent Unicode/module/filesystem changes or
desktop/size-profile support. Windows 42141915944 remains live in compilation;
macOS release 42125911196 remains live at the full Release step.

## Preserve native scalar write positions (2026-11-06)

Inspect the Node 24.15 native GetOffset contract against our fs facade after the
Windows scalar-validation final-file mismatch. Node passes safe integer write
positions to libuv verbatim, including negative values below -1; our facade
collapsed every negative number to the -1 cursor sentinel. Preserve safe negative
integers instead. Non-number/fractional/out-of-safe-negative inputs still select
the cursor, positive excessive positions retain the explicit unsupported policy,
and read-position validation is unchanged. Native Windows can interpret distinct
negative offsets differently, so do not emulate them by collapsing values.

Add write-positions.cjs to the real filesystem differential suite: eleven position
values through sync, callback and FileHandle writes, snapshot actual bytes after
each operation and after an ordinary cursor write. This measures both data and
cursor effects instead of comparing only returned byte counts. The targeted
filesystem suite passes; all 41 local macOS Release/size-profile CTests pass in
17.77s (build/write-position-{build,test,full-test}.log). API docs record the native
position boundary. Site, syntax, diff and upstream checks pass. This is a source-
grounded correction; whether it fully explains the Windows final-file difference
requires the next actual native job. No Windows acceptance, vendor patch or test
normalization is claimed.

## Deterministic network drain coverage (2026-11-06)

Revisit the original native Windows reference timeout at 42139627506. Its actual
Node stdout is 'backpressure true' before the 20s timeout: the fixture waited for
drain even though write returned true, which does not promise that event. This is
a fixture error, not evidence that native Node cannot resume the accepted socket.

Keep the full 3 MiB+29 payload and delayed paused receiver, with two transfers:
direct write waits for drain only when false, and a corked write above the watermark
must return false and register drain before uncork. Both wait for their write
callback, require an empty write queue and verify acknowledgement, actual byte
count, checksum and bytesWritten. This covers both valid OS completion paths while
retaining forced backpressure. No timeout increase, payload reduction or output
filtering is used. Run both runtimes even if one times out; any timeout still fails
the comparison instead of preventing the other runtime from being observed.

The targeted native TCP suite passes locally (build/native-drain-test.log); all
41 local Release/size-profile CTests pass in 16.84s
(build/native-drain-full-test.log). Architecture, site, Python syntax, diff and
actionlint checks pass. Windows acceptance of the changed fixture is pending.
The live 1df78c8 Windows run 42141915944 has successfully compiled the wide CLI and
passed startup, and now executes broad suites. It predates later path/offset/test
fixes, so neither compilation nor this status implies full Windows acceptance.

## Windows 32-suite result and remaining URL/environment contracts (2026-11-06)

Run 42141915944 at 1df78c8 (job 127644811641) is terminal failure with 32/40
suites passing in 62.18s. Its behavior suite now accepts native wide CLI eval,
Unicode/quote/backslash argument preservation and explicit malformed-surrogate
rejection. Native randomInt and HTTP-client suites pass too. Full Windows remains
incomplete. The run predates ec9e33c module paths, 4cba391 namespaces, 673b130
write offsets and 4994a92 drain correction; those fixes still need their own job.

The remaining output identifies two further actual contract differences. Preserve
pathToFileURL trailing separators using the native path.sep check before Windows
slash conversion, matching Node at drive roots and explicit foreign-style options.
Windows os.tmpdir now observes the current process.env object, including replacement
and missing SystemRoot/windir fallback, while POSIX still uses native environment
lookup. Update the OS design and CLI/platform evidence docs accordingly.

Correct two reference-harness assumptions without weakening Mite checks: the known
Node hash IsUint32/IsSharedArrayBuffer assertions must have the platform abort
status (observed Windows 134, POSIX -6) and matching assertion text; Mite must still
return its safe result. The live cwd fixture now removes a Windows drive prefix
before testing POSIX resolve. Both runtimes had returned false with the old incorrect
oracle, so this is not a runtime path patch or output normalization.

All 41 local Release/size-profile CTests pass in 18.14s
(build/windows-contracts-full-test.log); focused URL/OS tests pass separately
(build/windows-url-env-{build,test}.log). Python syntax, site, actionlint, diff and
upstream checks pass. New Windows branch/reference checks await the next native
job; the active 4994a92 job 42143633063 remains live in compilation. macOS release
42125911196 remains live; no release or complete platform support is claimed.

## Native child-process vertical slice and helper reaping ownership (2026-11-06)

Implement node:child_process spawn/ChildProcess with libuv process and three native
pipe handles on the existing owned EventLoop. Validate/normalize arguments,
environment, cwd/URL, stdio and selected flags/timeouts before native allocation;
recheck the private boundary. Child state roots its event callback until every
initialized native handle closes. Writes own byte snapshots/rooted completions;
read pressure stops native reads. Preserve spawn/exit/close/error metadata, kill,
separate process/pipe ref/unref, independent detached-child lifetime and VM teardown
without late JS calls. No Node/V8, shell fallback or new native dependency is added.
Shells, exec/execFile/fork/sync APIs, IPC, AbortSignal, privilege switching and full
Socket/stdio error parity remain explicit unsupported/incomplete work.

The first actual Firefly main-process child test timed out even though standalone
process/pipe comparisons passed. The pinned LibWebView ProcessMonitor owns SIGCHLD
and calls waitpid(-1), consuming unregistered application children. Replace just
that non-Windows TU with an owned adapter using registered-PID waitpid calls on a
10ms Core timer, without installing a competing SIGCHLD handler. Retain its ABI
and copyright; stop/release monitor timers explicitly. Configure/build checks pin
the upstream header/source and fail on drift/source-count mismatch. Windows keeps
the existing upstream handle monitor. Ladybird source remains unmodified; standalone
Mite does not link the Firefly adapter. ADR 0062/update guide record the ownership
boundary, bounded polling cost and future removal/review requirement.

tooling/tests/test_child_process.py compares real Node/Mite processes with independent
Python children: Unicode args/cwd/env, URL/string cwd and missing cwd, 1MiB echo,
forced stdin pressure and paused output, separate stderr, nonzero exit, ENOENT,
ignore/inherit, argv0/coercion metadata, kill/timeout, eight concurrent children,
unconsumed output, 18 validation cases, unsupported modes, fatal callbacks and
detached/unreferenced parent exit with a bounded child-survival handshake. Python
is only a test peer. The CLI example runs under both runtimes. Targeted suite
passes in build/child-process-test.log; all 42 local Release/size-profile CTests
pass in 18.93s (build/child-process-{build,full-test}.log).

All four actual renderer/desktop gates pass with the selective monitor:
build/child-process-{firefly,renderer,isolation,helper}-test.log. Firefly's ESM
fixture reads a real child pipe, and existing renderer/helper crash/lifecycle/
isolation/resource controls remain required and pass. Add API reference, example,
compatibility entry and navigation. Site, Python syntax, actionlint, drift/pin and
diff checks pass. New module/adapter hosted Windows/Linux and package acceptance
remain pending; no old package is relabeled or remeasured as containing this code.

Windows 42143633063 at 4994a92 (job 127649860700) is now terminal failure with
36/41 suites passing in 39.39s: filesystem, negative write offsets, module/package
basics and the corrected drain test pass. Four remaining failures predate f3f08b9's
URL/environment/reference fixes. The new Unicode module metadata test exposes
Windows 8.3 alias spelling: Node retains RUNNER~1 while std::filesystem canonical
expands runneradmin; preserve/resolve that behavior in the next loader step rather
than silently normalizing observed paths. Native Unicode file contents load, but
the metadata comparison remains a real failed gate.

macOS release 42125911196 at 004b7fe is terminal failure after its long compilation:
ring 0.17.14 fails two const feature assertions in cpu/arm/darwin.rs while building
libwebrtcclient_service. This is a CPU-feature build failure, not a successful hosted
package. Its original log is job 127590007748. Alpha publication remains pending;
investigate the owned Rust target/CPU build configuration without editing vendors.

## macOS ARM64 release CPU baseline (2026-11-06)

Fix the owned release build configuration after the confirmed ring 0.17.14 ARM
feature assertion failure in macOS 42125911196. Its Cargo command used native CPU
detection. Use upstream's existing ENABLE_CI_BASELINE_CPU option, selecting Apple
M1 for both project C++ and Rust on macOS ARM64; change no Ladybird/crate sources,
versions, warnings or assertions. The runner's exact missing features are not
established. New alpha/package acceptance remains pending.

Release orchestration enables the baseline for both Mite and Firefly. The renderer
driver exposes --baseline-cpu and rejects currently unverified platforms. Verify
actual generated commands before compilation with tooling/check_macos_cpu.py,
including preservation of -Dwarnings. BUILD.json and the workflow summary record
each project's cpuBaseline. Local development retains native defaults. ADR 0063
and release/size documentation explain scope, reproduction and comparison limits.

Validation: real generated standalone graph checks 275 project C++ / 7 Cargo
commands; renderer graph checks 2861 C++ / 19 Cargo commands. Mutating those real
inputs proves rejection of disabled baseline, native C++, native Rust and missing
Cargo commands. A locked/offline Rust 1.98 ring 0.17.14 probe reproduces both const
assertion errors with generic ARM64 and passes the SHA-256 abc known-answer check
with apple-m1 (build/cpu-baseline-ring{,-negative}.log). This is not proof of the
hosted runner's exact native feature set. The complete standalone Release/size
profile rebuild passes all 42 CTest suites in 22.11s, including child processes,
crypto, HTTP and npm (build/cpu-baseline-mite-{configure,build,test}.log).
Existing duplicate-archive linker warnings remain; no new symbol suppression.
Renderer configuration/size guard passes; renderer binaries and Designer packages
have not yet been rebuilt for the new CPU baseline, so old sizes are not relabeled.
Pin, Python syntax, actionlint and diff checks pass; site checks 107 pages / 5799
local links. Hosted ef61124 Linux/Windows compilation and f3f08b9 macOS release
compilation were confirmed live during this step; no jobs canceled or alpha
published. Windows 8.3 module metadata and full hosted package gates remain open.

## Windows module realpath spelling candidate (2026-11-06)

Replace the five module-loader canonicalization calls with one owned real_path
boundary. POSIX retains std::filesystem::canonical. Windows walks absolute,
lexically normalized components using UTF-8 libuv lstat/stat/readlink, preserving
ordinary short/case spelling and following symlinks/junctions. Broken targets and
excessive link traversal fail; lexical-only identities are not substituted. No
Ladybird, Node embedding, crate or filesystem public-API changes. ADR 0064 records
the observed distinction from Node's native realpath variant and remaining scope.

Expand mite-module-paths to compare raw Node/Mite Unicode/npm/entry metadata
through ordinary and linked directory paths, and require shared CJS/ESM cache
instances through links. Windows uses a mandatory junction, mixed-case entries
and GetShortPathNameW aliases when exposed by the volume. Replace the inappropriate
Python Path.resolve spelling oracle with samefile identity checks, retaining the
exact Node output comparison. No normalizing of observed output. UNC module URLs
remain explicitly unsupported. Local native Release/size-profile rebuild and all
42 CTests pass in 19.00s (build/module-realpath-{build,test}.log). The expanded
linked-path regression passes locally; the new Windows implementation and
Windows-only test branches await native CI. Site/Python/pin/diff checks pass.

Latest completed hosted source ef61124: Linux 42147228375 / job 127661544940
passes all 42 suites in 26.16s, including child processes. Windows 42147228418 /
job 127661590822 passes 39/42 in 35.66s, accepting the previous URL, environment
and reference-harness corrections. Three genuine failures remain: short-name
module metadata (this candidate), child subprocess stdout CRLF versus Node LF,
and an HTTP response observing writableFinished=false where Node reports true.
Those latter two require separate runtime corrections, not output filtering or
weaker test assertions. No hosted package success or alpha is claimed.

## Windows CLI console byte preservation candidate (2026-11-06)

Native ef61124 child-process observations expose an actual console output defect:
Mite's Windows CRT text streams translate LF to CRLF, unlike Node, including bytes
captured in native child pipes. Earlier Python text-mode capture normalized this
away. Configure only the CLI-owned stdout/stderr descriptors as binary in wmain,
before argument diagnostics or Mite::run. Preserve closed descriptors rather than
calling the CRT mode API on an invalid descriptor. Embedded hosts retain control
over their own stream modes. This does not certify Windows terminal code-page or
formatting parity and does not change subprocess byte streams/test observations.

Add raw-byte Node/Mite console behavior assertions for LF, explicit CRLF, NUL,
SUB and Unicode UTF-8 on stdout AND stderr; no newline decoding/filtering. Local
Release/size-profile rebuild and all 42 CTests pass in 19.40s, including the new
console check and child-process suite (build/windows-console-{build,test}.log).
Windows implementation still awaits native runner acceptance. Matrix and Windows
build docs distinguish this scope; hosted Linux child-process acceptance is now
recorded. Site checks 108 pages / 5856 local links; pin/Python/diff checks pass.
The Windows HTTP writableFinished observation remains an independent unresolved
failure; investigate actual write completion/event ordering next. No new package
size, hosted package success or alpha publication is claimed.

## Immediate TCP write completion / HTTP finish candidate (2026-11-06)

Investigate the third genuine Windows ef61124 failure from job 127661590822:
the HTTP fixture's finished field records whether its finish listener ran, not
the writableFinished getter. Its first normal GET response completed consumption
before that listener, whereas Node observed finish. Mite always queued TCP
writes, even empty completion writes after already sent headers. A new local
observable regression proves Node's small/empty/small writableLength values
[0,0,0] versus old Mite [1,1,2], with identical payload and asynchronous callbacks.

Use uv_try_write in the owned TCP adapter. Full immediate success returns a
private marker to JS Socket/Writable, which still defers public callbacks. Empty
writes with no native queued bytes use this path without an extra native request.
Partial immediate success queues only the remaining suffix while retaining the
owned allocation/rooted callback until completion or cancellation. EAGAIN/ENOSYS
fall back to uv_write; other native errors propagate. No blocking mode, upstream
edit, stream-source change or new dependency. ADR 0065 and HTTP docs record scope.

Keep all existing TCP backpressure/checksum/independent-peer/cancellation/error
tests. Add the shared fast-write workflow with payload/ack/order/async-callback
checks. Extend HTTP raw-peer comparisons with actual writableFinished/Ended,
24 immediate complete-request exchanges and a deliberate early 413 before an
upload ends. The early reply must arrive without fabricating request completion;
do not force all responses behind finish. Targeted TCP/HTTP tests and all 42 local
Release/size-profile CTests pass in 19.18s (build/tcp-fast-write-{net,http,full}-test.log).
Final native source builds pass. The standalone uses the M1 CPU baseline; the
local renderer returns to its documented development native CPU profile with
size optimization, rebuilding the current shared runtime, not relabeling an old
package as an M1 release.

All four actual GUI/native gates pass: check-firefly, check-firefly-renderer,
check-firefly-isolation and check-firefly-renderer-helper, including real main
HTTP/child work, renderer/compositor/process checks and isolated/helper security
controls (build/tcp-fast-write-native-gates.log). Both generated size-flag guards
pass; existing duplicate-archive/dependency deployment-version linker warnings
remain, with no new suppression or minimum-OS certification. Python, actionlint,
pin/diff checks pass; site checks 109 pages / 5910 local links. New Windows/Linux
hosted acceptance and the complete hosted M1 package build remain pending. No
new package size or alpha publication is claimed.

## Buffered native execFile vertical slice (2026-11-06)

Add callback-based node:child_process.execFile and its CJS/ESM export identity on
the existing native spawn/pipes/lifecycle. Return a live ChildProcess, collect
stdout/stderr independently with incremental decoding or Buffer/hex encodings,
and call back asynchronously once on close or the earlier startup error. Preserve
nonzero-exit code/killed/signal/cmd and native startup metadata; exact native error
text/stack fidelity remains incomplete. Optional callback execution still applies
the same timeout/output policies. No Node/V8 runtime, native transport duplication,
new dependency or Ladybird modification.

Default capture budget is 1 MiB per stream. Overflow destroys output readers and
requests configured termination, with ERR_CHILD_PROCESS_STDIO_MAXBUFFER and the
retained prefix. Infinity/explicit undefined disable comparison; null acts as
zero, matching the reference. Decoded mode retains Node's code-unit truncation
with a byte-derived budget; Buffer mode gives the exact byte prefix. Timeout and
killSignal use native termination, not an implied descendant-supervision guarantee.
Shells, AbortSignal, privilege switching, exec/fork/sync APIs and util.promisify
integration remain unsupported. Existing spawn semantics remain separate.

New mite-exec-file uses independent Python peers and Node comparison for separate
text/binary/hex output, split Unicode, startup/cwd/nonzero errors, ordinary/default/
zero/fractional/Unicode/undefined limits, a live overflowing child's actual
termination, 1 MiB stdin echo, SIGTERM/SIGKILL timeout, callback overloads/absence/
asynchrony/count/order, 29 validation cases, explicit unsupported modes and fatal
callbacks. Local target tests pass, including the final added termination and
undefined-limit cases (build/exec-file-test{,-final}.log). All 43 Release/size-profile
CTest suites pass in 20.61s (build/exec-file-full-test.log). The CLI example passes
under both Node and Mite. API, matrix, README and ADR 0066 record current scope.

Rebuild Firefly's shared runtime and require actual execFile output in the ESM
main workflow alongside spawn, HTTP and native renderer loading. All four actual
native desktop/renderer/isolation/helper gates pass (build/exec-file-native-gates.log).
Pin, Python, actionlint and diff checks pass; site checks 110 pages / 5964 local
links. New API hosted acceptance remains pending, with no package relabeling,
new size claim or alpha publication.

During this step, older Windows 42148050382 at 2c0d09c / job 127665903960 completed
40/42 in 44.13s, failing short-name metadata and console CRLF before those fixes;
HTTP passes in that run, confirming the earlier finish ordering failure can be
intermittent. This is not acceptance of the later corrections. Windows 42149420070
at 6ba8af1 / job 127670651541 was confirmed compiling; macOS 42144098371 at f3f08b9
remained live in its release compile. Do not cancel or replace live builds merely
for observation time. New Windows corrections/TCP path and the full hosted M1
package build still require their own native results.

## Atomic native context API publication (2026-11-06)

Implement the renderer-side ContextBridge engine for exposeInMainWorld(apiKey,api)
over the existing BridgeData/FunctionBridge. Nested data/function graphs preserve
cycles and repeated function identity, freeze target-realm copies and publish an
enumerable/nonwritable/nonconfigurable namespace only after complete preparation.
Reject occupied own properties without running their getters, avoid inherited
setters, and revoke all staged endpoints on failure. Keep default data snapshots
function-free; opt-in native function tokens contain no VM pointers/callbacks and
require a checked destination materializer. Root publication temporaries and
sanitize foreign completion errors rather than leaking principal exception objects.

Bound each owner to 64 publications/functions and 256 UTF-16 namespace units,
plus existing snapshot and per-function limits. Require exact source/document,
an active source owner and the first real script/owner caller frame: native
function Realm alone does not authorize principal reentry through call/apply.
Use weak publisher closures and source-detach hooks to revoke all functions and
pending promises, retaining safe frozen data. Unsupported API values reject
explicitly. Source enumeration scratch allocation is still not hard containment.
No privileges, dependencies, Node/V8 embedding or Ladybird changes are introduced.

New native SDK coverage checks nested aliases/cycles/frozen prototypes, copied
results, trap-free rejection and staged rollback/function/key limits, occupied
getters/inherited setters, deliberately leaked publisher calls, principal reentry
while a source executes, pending rejection, detach and GC. The new test initially
exposed a lexical-name collision between SDK scripts, fixed with block-scoped
fixtures. It also changed the existing weak-filter collection observation. Native
retaining-path inspection found no ownership path; now require an empty root-path
observation at a separate native boundary before collection, retaining BOTH old
weak realm/filter collection assertions. This also drains previous incremental
sweeps. No runtime collector change, retry or removed assertion.

The compiled sandboxed helper now exercises a harmless nested fixture API before
first author script in top-level/srcdoc documents, with absent raw publisher/Mite
globals, trap-free arguments, removed-frame function/Promise rejection and a live
parent API. The unmodified helper is a negative control. Fixture publication skips
transient initial about:blank, which can reuse its Window for srcdoc; duplicate
immutable namespaces correctly reject. This is not a product navigation policy.

All four native gates pass in build/context-api-native-gates.log. After final
publisher rooting/error sanitation and caller tests, the affected isolation/helper
gates pass again in build/context-api-final-validation.log. Existing security,
renderer/compositor/process and Firefly main workflows remain enabled. The Mite
runtime/build graph is unchanged; previous 43 local suites are not relabeled as
new platform evidence. Pin, Python and diff checks pass; site checks 111 pages /
6017 local links. ADR 0067, compatibility and update guidance record scope.

Public preload/contextBridge/IPC remain unsupported in BrowserWindow. Required
next integration: application-selected preload sources, a production helper with
no diagnostic globals, per-window/document/navigation policy, bounded main/renderer
transport, malicious wire-input controls and actual Electron migration fixtures.
Do not select the diagnostic helper for applications. TLS/HTTPS/fetch, remaining
desktop APIs and complete native Windows/Linux packages also remain open.

Hosted 6ba8af1 Linux 42149419974 / job 127670385257 passes 42/42 in 28.46s.
Windows 42149420070 / job 127670651541 completes 39/42 in 50.86s. The initial CJS
path metadata and console byte fixes pass; ESM metadata now differs only in URL
escaping of RUNNER~1 (Node %7E, Mite literal ~), before later alias/junction cases
can run. Child-process execution reaches its detached/unreferenced-parent case,
then Python communicate times out on Mite; inspect native handle inheritance and
actual parent liveness before changing any timeout. DNS custom lookup fails in
the Node reference with native status 3221226505 while Mite completes; keep that
comparison failed pending diagnosis. New API hosted acceptance/full hosted M1
packages remain pending. No alpha or new package-size claim.

## Canonical module file-URL escaping (2026-11-06)

Fix the ESM URL discrepancy from native Windows 6ba8af1: filesystem metadata now
preserves RUNNER~1, but ModuleLoader::path_to_url serialized literal ~ while Node
and Mite's public pathToFileURL use %7E. Reproduce it locally with a real tilde
directory before changing the encoder (build/module-tilde-before.log). A separate
ASCII punctuation audit also finds raw square brackets instead of %5B/%5D; encode
those alongside tilde in the owned loader. General URL parsing and native path
spelling remain unchanged; Ladybird remains unmodified.

Extend the existing Unicode/native metadata fixture with ~[] in its real directory,
require import.meta.url===pathToFileURL(import.meta.filename).href, and require
raw/escaped tilde AND bracket specifiers to share module instances. Preserve exact
Node output comparison and all CJS/package/alias/junction checks. Add a real
tilde-named Firefly ESM dependency, imported raw and escaped, whose own metadata
must match pathToFileURL too. The targeted path suite and all 43 local native
Release/size-profile CTests pass in 20.55s (build/module-tilde-{test,full-test}-final.log).
Rebuilt actual Firefly migration/lifecycle/native controls/HTTP/child/renderer-crash
workflow passes with that dependency (build/module-tilde-firefly-test-final.log).
Pin/Python/site/diff checks pass. New Windows acceptance, including the formerly
unreached junction/short-alias checks, remains pending. The detached-child pipe
capture and Node-reference DNS fail-fast are independent open issues; no timeout
or output normalization was relaxed and no alpha/package size is claimed.

## Windows CLI inherited-stdio policy and liveness diagnostics (2026-11-06)

Add the recommended early uv_disable_stdio_inheritance call in Windows wmain,
before stream modes/argument setup. Pinned libuv's CreateProcess path enables
handle inheritance and deliberately duplicates requested stdio, but the CLI had
not cleared inheritance on its own inherited standard handles. An ignored-stdio
descendant can consequently retain output capture after its parent exits. This
is a candidate correction for the 6ba8af1 timeout, not definitive attribution
without the new native Windows result. The call is best effort, not a complete
arbitrary-handle policy. Keep it in CLI startup; embeddable Mite::run does not
silently change its host's process-wide descriptor policy. No vendor changes or
new dependencies; explicit stdio:'inherit' retains deliberate libuv duplication.

Replace the detached fixture's ambiguous run/communicate timeout with separate
parent wait and capture EOF observations sharing the original three-second
deadline. Distinguish a still-live parent from an exited parent with descendant
pipe ownership. An independent Python parent/child positive control intentionally
retains inherited capture pipes and must trigger that diagnosis; acknowledgement
releases it. Every path releases the owned helper. The separate five-second
cleanup drain never extends acceptance, and no timeout/output normalization is
relaxed. Existing marker still proves independent child survival after parent exit.

Local POSIX CLI rebuild and all 43 CTests pass in 23.62s
(build/stdio-inheritance-{build,full-test}.log). The final stricter zero-remaining
deadline calculation and positive control pass the targeted child suite again
(build/stdio-inheritance-test-final.log). These local results validate the harness
and existing explicit pipe/inherit behavior, not the new Windows-only startup body.
API/Windows notes and ADR 0062 record scope. Pin/Python/site/diff checks pass;
site checks 111 pages / 6022 local links. Windows acceptance, the Node DNS native
reference abort, full hosted M1 packaging and public preload/IPC remain open.

## Hosted Windows and Linux runtime acceptance (2026-11-06)

Revalidate authoritative CI jobs for 745c7af: Windows run 42155631612,
job 127691083073 passes native clang-cl x64 Release and all 43 CTests in 49.23s;
Linux run 42155631610, job 127689774212 passes all 43 in 32.81s plus CLI/linkage
gates. This accepts the measured canonical module URL/metadata, raw console,
spawn/execFile and detached-parent capture corrections without relaxed timeouts,
output normalization or skipped comparisons. Prior Node-reference DNS fail-fast
does not recur; its root cause remains unknown and comparison remains enforced.
Update runtime platform/API evidence; do not infer Windows/Linux Firefly desktop,
sandbox, packages, optional size-profile or general Node compatibility.

macOS f3f08b9 run 42144098371/job127654883895 fails renderer Rust ring with the
known native-target-cpu static feature assertions. That source predates the M1
baseline fix in 2c0d09c. The 745c7af package run is queued behind its diagnostic
upload; no manual cancellation/restart and no alpha publication. Hosted packages
with the baseline fix still need their own acceptance.

## Public CommonJS preloads and production helper (2026-11-06)

Implement BrowserWindow.webPreferences.preload with absolute UTF-8 regular-file
validation, a construction-time source snapshot and explicit file/type/size errors.
Limit source to 64 KiB/window, 16 configured windows and 1 MiB aggregate. Bind
configuration to native view handles. An owned generated Qt WebContentView TU
adds exactly one preparation hook; pinned source/header hashes, restoration and
missing/duplicate-seam controls gate configure/build. Ladybird stays unmodified.
The hook configures newly hosting pages before first script, including process
replacement; existing hosting pages receive configuration before host loads.

Add a separate production FireflyRuntimeWebContent/ProductSession, without test
probe APIs. Validate narrow internal session configuration, strip application
entries before upstream principal-world preload handling, and create protected
isolated worlds/context publishers for top-level file/http/https documents. Skip
initial about:blank/child frames. Require only firefly/contextBridge; no process,
Buffer, filesystem loader or privileged IPC. Copy native metadata before JS entry;
revoke inactive document worlds and limit active worlds to 32. Every supported
document loaded into the selected window receives its API; no origin allowlist.
Syntax/uncaught preload failures deliberately terminate the renderer and reject
pending loads instead of Electron's preload-error/continue policy. HTTPS-specific
preload, broader DOM/callback/value compatibility, error parity and IPC remain open.

Select the production helper in development and require it during packaging;
never ship the diagnostic helper or silently fall back upstream. Preserve physical
helpers/resources, sandbox entitlements, relative linkage, notices and signatures.
Update the hello-world with a working isolated data/function preload. Release
packaging now repeats public preload acceptance against its own app executable.
Add API reference/ADR 0068 and update current availability/migration/security docs;
historical internal-gate notes retain their original scope.

Validation: production helper and Qt app compile; all four native Firefly,
renderer/compositor, SDK isolation and diagnostic-helper gates pass
(build/public-preload-gates.log). Shared real Electron/Firefly tests cover first
script, frozen nested APIs/Promise results, three windows, no subframe installation,
navigation and file-to-HTTP process replacement. Firefly-specific tests cover eight
constructor failures, exact 64 KiB source acceptance, CommonJS metadata/receiver,
unsupported require, trap-free argument getter rejection, construction snapshot
reuse and syntax/throw fail-closed loads (public-preload-test-snapshot.log).
Standalone Mite code/graph unchanged; 745c7af native 43-suite gates remain its
runtime evidence rather than a claim of a new standalone feature.

An initial package used the stale build/designer-app and correctly FAILED the
current Designer controls gate despite editor/Monaco success. Rebuild the pinned
Designer from source into public-preload-designer-app with production dependencies
and 19 bundled entries/5 workers; full native Designer controls then pass. Repackage
fresh, move to build/public-preload current relocated é文, run the public preload
suite and actual Designer launcher/edit/Monaco/HTML/native controls there. All pass;
7191 file hashes and both strict ad-hoc app signatures verify
(build/public-preload-current-relocation.log). These are dirty-source local
validation artifacts, not a hosted release or new comparable size benchmark.
Pin/source-adapter/Python/actionlint/diff checks pass. Site validates 113 pages /
6155 local links/resources/fragments. Hosted macOS 745c7af
run 42155631670/job127698599246 is now compiling with the M1 baseline correction;
its final package result remains pending. No alpha published. Native Windows/Linux
Firefly, privileged IPC, TLS/fetch, desktop services and complete release scope
remain unfinished; the full goal stays active.

## Explicit shared OpenSSL size reduction (2026-11-06)

Audit the entire current native package against its actual pinned OpenSSL 3.6.5
SSL/Crypto archives. The earlier 903-symbol sample understated the full graph:
Firefly/WebWorker, LibCrypto and LibTLS contain 3946 repeated external names / 7890
redundant definitions. The standalone Mite CLI is still absent from the app;
engine duplication is not the cause. Exclude Mach-O private externals consistently
from the API inventory; they correctly become local when linked into a dylib.

Add opt-in FIREFLY_SHARED_OPENSSL / --shared-openssl for the macOS ARM64 shared
Firefly graph. Build one owned dylib by whole-archiving the same pinned SSL/Crypto
inputs; redirect all consumer link/interface references and exact archive paths
without changing original imported targets, upstream sources or dependency versions.
Preserve SYSTEM header semantics and compile requirements; reject unsupported
platform/ABI/archive/graph configurations. Independent Mite stays unchanged/static.
A first compile exposed missing private include propagation; fix the owned target
rather than engine headers. The final configured graph has archive links only in
the provider, and all four native Firefly/renderer/SDK/helper groups pass
(build/shared-openssl-gates-final.log).

Add a native SHA-256 known-answer + trusted/hostname-verified TLS 1.2/1.3 in-memory
binary round-trip probe with a wrong-hostname rejection control; it passes
(shared-openssl-provider-test-final.log). This validates provider linkage, not a
new Mite HTTPS/TLS API. Packaging records sharedOpenSSL and automatically audits
both apps before hashing: require the complete pinned 10927-name external input
inventory in exactly one physical provider, explicit main/engine consumers and no
residual copies. Record archive hashes, 16 direct consumers and the 5905408-byte
stripped provider; distribute the relevant OPENSSL.json in each app release archive.
The original package fails the required-provider audit as a negative control.

Rebuild/repackage the same Release C++ -Oz / LTO-OFF profile in enabled and disabled
modes. Baseline renderer CPU remains native/OFF, Mite baseline M1/ON. Default mode
explicitly excludes a stale provider in the reused build/lib directory; both app
inventories prove the original link graph and no packaged provider. All four gates
pass again disabled (shared-openssl-disabled-gates.log), as do its packaged public
preloads. Enable/configure/provider-test/disable again passes, leaving the local
development graph at its original default mode (shared-openssl-toggle.log).

Same-source/profile packaged comparison: framework 212779511 -> 205956349 bytes
(202.92 -> 196.42 MiB), Designer 248575406 -> 241752244 (237.06 -> 230.55 MiB).
Both save exactly 6823162 bytes / 6.51 MiB, including the provider and signatures.
Executables shrink 7357200, libraries grow 533888, signature metadata grows 150.
App, resource and notice byte counts are unchanged. The 19 fallback source archives
have identical decompressed contents but different gzip metadata; rebuilt Assets.car
also differs in bytes. Do not claim bit-for-bit reproducibility or attribute those
same-size differences to the native saving.

The final automatic-audit packages are moved to build/shared-openssl final relocated
é文. Public Electron/Firefly preloads and actual Designer launcher/edit/Monaco/HTML/
native window controls pass there; 7195 hashes and both strict app signatures verify
(shared-openssl-final-relocation.log). These are dirty-source local experiments,
not a published release, hosted CPU-baseline result or runtime/memory benchmark.
ADR 0069, package-size/release/update docs, opt-in release flag and macOS manual
workflow input record the scope. Defaults remain OFF until hosted acceptance.
Pin/Python/actionlint/diff and website checks (114 pages / 6212 local links) pass;
no Ladybird patch added.

New authoritative c517096 CI also passes Windows run 42159392317/job127701205588
(all 43 in 50.92s) and Linux run42159392285/job127701167108 (all 43 in 26.45s).
macOS 745c7af run42155631670/job127698599246 remains confirmed live in the full
Release/package step. No running job canceled/restarted and no alpha published.
Public IPC, broad runtime/desktop compatibility and native Windows/Linux Firefly
packages remain open; the full implementation goal stays active.

After pushing fd2124e, dispatch macOS experiment run
[42161235765](https://github.com/jogibear9988/elytra/actions/runs/42161235765)
at that exact source with optimize_size=true, shared_openssl=true and
publish_alpha=false. GitHub accepts the dispatch (HTTP 204) and reports it pending
behind the confirmed running macOS package job. Default modes remain unchanged;
this queued experiment is not hosted acceptance. No alpha tag/release created.

## Bounded data-only IPC wire foundation (2026-11-06)

Extend BridgeData with versioned data-only encode_wire/decode_wire, keeping existing
context publication tokens private and rejected from wire data. Use flat node-index
arrays to retain cycles/aliases/undefined/holes and own enumerability/null prototypes.
UTF-16 unit arrays preserve unpaired surrogates/NUL in values and keys. Fixed 16-digit
IEEE-754 bit strings preserve finite precision/extremes/subnormals/-0/infinities;
normalize NaN and reject noncanonical NaN bits before LibJS value materialization.
No Node/V8, dependency or Ladybird change. Frozen reconstruction remains the default
for all existing bridges; explicitly selected mutable reconstruction supports future
IPC clone arguments/results without mutating or sharing the source graph.

Bound input to 1 MiB and lexical JSON array depth 8 before parsing. Reject objects,
unknown tags/version/arity/types, fractional/out-of-range indices/units, duplicate
keys, orphan nodes, invalid sparse indices/length and internal function tokens.
Enforce default hard node/property/string/depth/per-array limits in both directions,
including implicit array length metadata, plus aggregate sparse length 16384.
Callers cannot raise those limits. No JS object is created until full validation.
This does not authenticate sender/window/frame/document or expose public IPC.

Native checks cover independent handwritten remote graphs, frozen/mutable ownership,
GC, precision/cycles/prototype safety, byte mutations, malformed-input recovery and
exact node/UTF-16/sparse/1-MiB boundaries. An attempted second VM in the browser SDK
hits upstream's one-VM-per-process guard; preserve that invariant. Use a separate
native producer that exits before a fresh process/VM decodes, materializes, checks
its own intrinsics/identities after GC and re-encodes identical canonical bytes.
Malformed remote processes must exit 1 with no output; no native abort/timeout is
accepted. This uses actual LibJS, not Node or a JS serialization substitute.

Initially running the heavy wire checks inside the browser SDK makes its existing
weak-filter GC fixture fail intermittently. Diagnostics identify a conservative
StackPointer -> RawNativeFunction -> Realm root, not a wire edge or owned metadata
path. Ten predetermined diagnostic runs fail at run 4; do not call it a runtime
fix or retry into green. Move the NEW allocation-heavy wire checks into their own
native process/realm host. Existing browser SDK and NodeFilter assertions stay
unchanged. All ten predetermined browser SDK runs then pass, alongside dedicated
wire checks (ipc-wire-clean-sdk-stress.log). No GC/runtime/ownership guard relaxed.
check-firefly-isolation depends on the new check-firefly-wire process gate, so the
existing release driver runs both. All four final native Firefly/renderer/isolation/
helper gates pass (ipc-wire-final-all-gates.log). Pin/Python/actionlint/diff/site
checks pass. Standalone Mite is unchanged; no new package-size/release claim.
ADR 0070 and renderer/roadmap notes record scope. Transport selection, native sender/
document generation validation, pending-work/teardown policy and actual public
ipcMain.handle/ipcRenderer.invoke plus Electron fixtures remain the next work.

User steering: keep CommonJS supported, convert all authored samples to modern ESM;
CJS compatibility fixtures remain required. The hello-world preload needs an ESM
source/build path because native sandbox preloads currently accept CommonJS only.
Release status revalidated: no GitHub releases exist; macOS 745c7af
run42155631670/job127698599246 is confirmed live in Compile/Test/Package, started
03:49:12 UTC. At 04:37:43 UTC that step has run about 49 minutes. Optimized fd2124e
run42161235765 is still pending with no allocated job. No canceled/restarted running
build, no alpha yet. Finish the modern sample/build/package integration next, then
push the coherent commits and keep the requested optimized CI candidate current.
The full product goal remains active, including all native desktop/release targets.

## Modern ESM sample sources and first published alpha (2026-11-06)

User requests CommonJS compatibility but modern ESM for every authored sample.
Keep all CommonJS compatibility fixtures and implementation; convert CLI hello to
hello.mjs with imports/top-level await and remove the CJS-only example in favor of
the existing ESM JSON/hash examples. Update docs/README/Linux example commands.
All authored examples are now free of .cjs source files. Targeted native commonjs,
module and package suites pass (3/3, 2.11s); hello/modules/hash run with native Mite.

Convert Firefly hello-world main and preload sources to .mjs and page script to a
module. Add pinned esbuild preload bundling and build_hello.py: main stays ESM,
preload source ESM becomes the generated CommonJS payload required by both sandbox
implementations. Node/npm are build tools only; no Node/V8 engine is shipped.
Direct native .mjs preload selection remains unsupported, documented explicitly.
Build provenance records source hashes, formats and the bundler lock. Package the
built hello directly instead of synthesizing an ESM wrapper around a CJS main.

The first top-level await app.whenReady sample works in Firefly but hangs in the
pinned Electron reference; keep the reference timeout strict. Use the shared
whenReady().then(async createWindow) pattern, retaining native ESM main and async/
await inside initialization. Real Electron and Firefly now both boot the same
main/page/preload sources, changing only the desktop import in reference fixtures
(hello-esm-ready-test.log). The shared test helper accepts explicit extra script args.
Release gates run the modern sample comparison and native packaged Launcher smoke.
Actual existing Firefly/Electron regression tests also pass after that helper change.

Fresh packages relocate to build/modern samples relocated é文. The modern hello
passes against Electron/Firefly and through its actual Launcher. Actual packaged
Designer edit/Monaco/HTML/native controls pass too; all 7191 hashes and both strict
app signatures verify (modern-samples-relocation.log). No new comparative size claim.
Update sample README, first-app/API/bundling/build/release docs. Pin/Python/actionlint/
diff checks pass; site validates 115 generated pages / 6269 local links/resources.
Ladybird unchanged.

The user then correctly notes the hosted macOS build succeeded without an alpha.
Revalidate run42155631670/job127698599246: every full Release/native/package step
succeeds; alpha job127719422082 is skipped solely because publish_alpha=false.
No releases exist at that observation. Download artifact13478498209 using the
GitHub connector, verify its exact SHA-256
94a8f8c368f9b13c53ee190a879d703f86abf73174c41ebd78dd65a2eb047249,
all archive checksums and all three clean-source BUILD manifests at
745c7afb0ff7d065d055b0f6a1678c67717594a6, Release/Release, M1 baseline ON/ON.
Its real sizes are framework 215.27 MiB / Designer 249.38 MiB; this is the baseline
hosted build, not the smaller local provider experiment.

Create annotated v0.1.0-alpha.1 exactly at that verified source, push the tag (alpha
excluded from all-platform build trigger), create a draft prerelease, upload all
five unchanged assets, verify server asset sizes/digests, then publish prerelease
without marking it latest stable. Release436117231 is live at
https://github.com/jogibear9988/elytra/releases/tag/v0.1.0-alpha.1 . Remote GET confirms
published/non-draft, prerelease and five uploaded assets. Preserve original dev.34
filenames/BUILD versions and explicitly explain immutable promotion provenance.
No local/dirty package substituted and no rebuild needed. Notes explicitly state
macOS ARM64/ad-hoc/not notarized, source745 limits, no new preload/ESM/provider work,
and no Windows/Linux desktop binary claim. Update current release/download docs.
The requested first alpha is now delivered. Optimized fd2124e run42161235765 has
started; do not cancel it. The modern ESM source/build integration still needs its
own hosted package acceptance. Full native cross-platform/API/IPC goal stays active.

After pushing wire98a8788 and modern-samples bf8c65d to master, dispatch the
updated optimized macOS candidate at bf8c65d: run42166516264 is pending with
optimize_size=true, shared_openssl=true and publish_alpha=false (HTTP 204).
GitHub replaces the superseded pending default bf8c65d run42166450753; the already
running fd2124e experiment42161235765 remains untouched. This updates the requested
smaller-build candidate to include ESM-authored samples and wire gates. No second
alpha is claimed before its own complete hosted artifact validation. The published
first alpha remains the immutable 745c7af snapshot described above.

## 2026-11-06 — ESM desktop feature catalog

Add original ESM source apps and individual API-linked guides for all 21 topics in
Electron's current examples overview, plus quick-start and lifecycle (23 apps).
Record upstream reference URLs, reviewed date, exact dependencies and per-example
status in examples/electron/catalog.json. Build-time runtime selection is explicit:
Firefly by default, a separate pinned Electron reference tree for comparison;
never fall back from a missing Firefly capability. Authored preloads stay ESM and
are bundled with locked esbuild into each sandbox's required CommonJS format.

Six narrow real Firefly/Electron slices pass: isolated quick-start, browser module
worker, initial online/offline state, iframe postMessage handshake, frameless
startup, and lifecycle cancellation/retry. Also compare forced app.exit(7) and
its exact non-cancellable trace. The 17 other Firefly examples fail preflight with
explicit missing-API/platform errors; these are BLOCKED, not feature acceptance.
Their reference/native interactions remain manual/unverified. No test purchases,
protocol registration, permissions, recent-document changes or notifications.
IAP requires a real trusted receipt verifier; local AI requires a real backend.

Real file:// tests exposed Ladybird CSP self-origin resource blocking. Use an
owned loopback static server with exact public HTML/module/CSS route allowlists,
Host validation and no privileged actions, main/preload/utility/config exposure.
Independent Python HTTP probes pass against Node and Mite for allowed GET/HEAD,
MIME, rejected POST/Host/query/traversal/private-file requests. Fixed IPC action
names validate exact window, main frame and approved page in reference examples;
this does not implement Firefly's missing public IPC. CSS/script CSP remains
explicit; the frameless alias shim's inline-style limitation is documented.

Validation: complete test_electron_examples.py native run passes (six shared paths,
17 explicit blocks and forced exit; build/electron-examples-tests-final.log).
Asset probes pass on build/release-mite/bin/mite and Node. ESM/Python syntax,
actionlint, diff and unmodified Ladybird pin checks pass. Release orchestration now
builds/tests both trees and probes the asset boundary; hosted acceptance of this
new gate remains pending. Documentation navigation includes the catalog and each
topic's setup/limits. Ladybird unchanged; desktop platform/API implementation
remains incomplete. The published alpha.1 predates these examples.

## 2026-11-06 — Detailed desktop API references

Replace the short app summary with a member-indexed reference for the implemented
singleton: parameter/default/return/error tables, shared readiness Promise behavior,
normal quit sequence, synchronous cancellation/retry and partial window rollback,
forced exit/status validation, all five emitted events and exact argument shapes.
Describe differences from Electron's last-window defaults and missing OS/native
members explicitly; registering an arbitrary EventEmitter name is not support.
Link actual ESM lifecycle sources, native/reference fixtures and alpha provenance.

Expand BrowserWindow with per-method control/query/close/destruction contracts,
properties, registry identity and option validation. Distinguish restore from
unmaximize and native title from page title. Expand webContents member entries and
loading/event details, validation versus rejected Promises, superseded loads,
timeout/crash limits and PID lifetime. Correct stale preload wording: public
contextBridge is partial, public main/renderer IPC remains unsupported. Update the
documentation baseline to reflect these references and the 23-app catalog.

Validate against owned firefly/core/api.mjs and existing native/reference tests;
no runtime/upstream edits in this documentation step. Site/link/fragment validation
passes after these changes; the six real catalog assertions and forced-exit trace
passed in the preceding step. Windows/Linux desktop, missing APIs, broader native
integration and newer hosted artifact acceptance remain work in progress.

## 2026-11-06 — Native HMAC and byte comparison

The preceding goal turn made concrete progress: committed/pushed the 23-app catalog
and detailed desktop references; Pages deployment succeeded. Resume from that
clean tracked state. The complete specification remains active and incomplete.

Implement node:crypto createHmac/Hmac and timingSafeEqual in the independent Mite
runtime and Firefly main process. C++ owns OpenSSL 3 EVP_MAC contexts with RAII;
no deprecated HMAC_CTX, Node/V8 dependency or Ladybird edits. Consume exact key
bytes at construction, support empty/long keys and incremental byte ranges, and
release native contexts after digest/collection. Native CRYPTO_memcmp compares
equal-length byte ranges; type/length/caller logic is outside that property.
Reuse existing Buffer/encoding adapters, with explicit unsupported key objects.

Keep HMAC's repeated manual digest empty-output semantics. Match Node's lazy
Transform initialization: options.encoding applies to string keys, while stream
validation/output setup happens only when a stream operation needs state. Eager
construction or stripping that option lost reference behavior; the real encoded
stream differential now covers it. Mite explicitly finalizes the streaming path,
returning empty output on later manual digest rather than reproducing unstable
post-stream native bytes observed in Node. Invalid object-mode input yields a JS
error. No complete stream/error/cross-receiver/secure-heap/FIPS claim.

Add an ESM signing/verification example with environment-supplied key, strict hex
length/format, native byte comparison and invalid-signature status. Actual Node
and Mite runs agree for Unicode messages, tampering, malformed/trailing-newline
signatures and uppercase hex. Extend the real Firefly ESM migration fixture with a
known HMAC and native equality. New APIs replace only the prior createHmac-absent
fixture; unsupported KeyObject creation remains explicitly tested instead.

Validation: 357 Node HMAC/equality observations and 127 independent Python/RFC
known-answer vectors pass, including SHA256/SHA512 block boundaries, key snapshots,
raw/detached/resized views, encoding/error recovery, ESM/CJS identity, native-state
allocation pressure and Transform/pipeline output (hmac-tests-with-example.log).
All 44 CTests pass (hmac-regression.log, 114.11 s). Actual rebuilt Firefly check-firefly
passes lifecycle/controls/crash/ESM main plus the production public-preload boundary
(hmac-firefly-tests.log). Site validates 140 pages and 8063 local links/resources/
fragments; syntax/actionlint/diff checks pass. Update API/matrix/README and ADR 0071.

Native Windows/Linux acceptance of these new APIs and a new hosted package remain
pending. The published alpha.1 predates this work. Public IPC, TLS/fetch, native
Windows/Linux Firefly and broader specification requirements remain incomplete.

## 2026-11-06 — Hosted optimized package evidence

Re-poll the confirmed-live run42161235765 rather than restarting it. Job127719422607
now completes SUCCESS, including all Release/native/provider/Designer/packaging
gates. Alpha job127738152360 is skipped because publish_alpha=false; alpha.1 is
still the published immutable 745c7af snapshot. Fetch job logs and artifact13481625336
through the GitHub connector. Download and verify exact outer SHA-256
678d9ddcf9242ac8488a93a42cce07655aee42ad93b9de8ffbbfe19e4bb53514,
all included checksums and all three archive-root BUILD.json manifests. Designer
also includes application/browser BUILD.json files; select the archive root rather
than mistaking nested build provenance for duplicate release manifests.

All three root manifests name clean fd2124e154c438fb312e859d6fdfe42e22a8844d,
Release/Release, M1 baseline ON/ON, -Oz ON/ON, LTO OFF/OFF, production/stripped and
sharedOpenSSL ON. Framework is 205458549 bytes (195.94 MiB); Designer241227249 bytes
(230.05 MiB). ZIP bytes: framework98218961, Designer110211255, Mite30194701. Record
exact profiles and source/provenance in package-size and release docs. The local
same-source OpenSSL comparison remains separate; do not attribute the entire
source-to-source reduction to one flag. No new tag/release or newer-master/Windows/
Linux acceptance is claimed. Keep subsequent live/pending builds intact.

## 2026-11-06 — Priority: repository readability refactor

The user explicitly makes repository-wide source organization, readable formatting,
ESM authoring and useful comments the main priority. Finish that work before new
API implementation resumes. The preceding HMAC turn was concrete progress.

A native invoke/handle implementation was in progress when this steering arrived.
Preserve it in stash d8e6a1c728c50b419894dd09ba81a41b42e08af4 (WIP native
invoke/handle transport before repository readability refactor). It compiles after
local fixes but has not passed the new real IPC fixtures and is not published or
claimed usable. Do not drop it. Apply/rebase it onto the refactored paths only
AFTER the readability work is complete; resolve its old include/source paths then.

Move the native Mite runtime into core, modules, bootstrap, adapters and platform
service families (process/filesystem/network/text/system/crypto). Move the CLI to
mite/cli. Update all owned include/build/source links. Keep explicit source lists
and the runtime-only dependency boundary. Add a directory map and per-file purpose,
ownership/thread/realm notes. Expand compact native code to normal four-space
blocks, separate definitions/declarations and preserve license/integration seams.

Add EditorConfig, clang-format/Prettier/Ruff profiles and pinned build-only formatter
inputs with an orchestrator. JS definition spacing is AST-aware; native spacing
preserves string/comment contents. Vendor/generated bytes and intentional module/
parser fixtures need separate treatment. This commit establishes the machinery;
it does NOT claim all remaining repository sources already satisfy the new rules.

Validation: rebuilt standalone Mite and Firefly; 44/44 CTests pass in 86.72 seconds
(refactor-native-tests.log). All four native desktop/renderer/isolation/helper gates
pass (refactor-native-firefly-tests.log), including existing security/GC checks.
The 39 native runtime/CLI files pass the pinned formatting check. Documentation
validates 141 pages / 8123 local links/resources/fragments. Ladybird unchanged.

Remaining HIGH-PRIORITY refactor sequence:
1. Convert maintained builtin and bootstrap sources to ESM; generate/check the
   synchronous embedding representation without adding Node to native builds.
2. Apply readable formatting/comments and appropriate organization throughout
   Firefly, build/test tooling and authored web/sample sources. Keep deliberate
   CJS fixtures and generated/third-party code explicit rather than mislabelled.
3. Wire repeatable style/integrity checks; audit stale paths, comments and commands;
   run full runtime/native/application/site acceptance before resuming IPC.
Scratch build/migrate_modules_to_esm.py stages the planned source move but has not
been applied at this checkpoint. The general product goal remains incomplete.

## 2026-11-06 — Maintained builtin sources are ESM

Continue the user's readability priority; IPC remains parked in the recorded stash.
Move authored compatibility modules into core/network/filesystem/crypto/process/
system source families and use named default ESM factories. Native dependencies
remain explicit injection arguments. Split crypto assembly, entropy, digest/HMAC
and byte helpers into actual private ESM imports. DNS now has descriptive option/
error helpers and comments explaining scheduling and hostname preservation. Expand
compound declarations/control bodies, use normal indentation and separate methods.

Convert the generator and portable npm adapters to ESM. Compile private builtin
imports into a checked synchronous factory expression, preserving strict execution
and original native call/constructor semantics. Initial globals are authored as
bootstrap/globals.mjs and generated as the eager bootstrap adapter. Normal native
builds read committed outputs and run Python integrity checks; Node/npm are still
build tools only, not runtime dependencies. CJS application compatibility remains.

Extend the manifest to all authored source/helper/catalog hashes and generated
builtin/bootstrap files. Preserve pinned package provenance and complete notices.
Reject compiler export-format drift or unbundled private imports. Avoid rewriting
unchanged generated files. LF attributes and the Windows checkout-index gate now
cover authored ESM inputs as well. The controlled entropy fixture intentionally
loads the generated host-factory ABI; no test override is shipped in Mite.

Validation: outputs reproduce exactly; Python integrity and all 44 CTests pass
(refactor-esm-tests.log, 54.28 s). Rebuilt Firefly check-firefly passes real native
main lifecycle/I/O/crypto plus production preloads (refactor-esm-firefly-tests.log).
The 21 maintained module/bootstrap/build-adapter JS files pass the new formatting
check. Source/API docs and commands follow the new paths. Ladybird unchanged.

Still REQUIRED before returning to feature work: complete Firefly/tooling/sample
formatting, source-purpose comments and organization; convert remaining maintained
Firefly script factories to ESM authoring; use explicit syntax-fixture exceptions,
wire a whole-repository style gate and finish full native/application/site checks.
This is a coherent intermediate commit, not completion of the user's refactor.

## 2026-11-06 — Repository readability refactor completed

Complete the user's current priority before resuming API implementation. Firefly's
renderer bridge is grouped into worlds, serialization, context publication and
callback families (events/timers/observers/traversal). Renderer helper filenames
now distinguish entry, production preloads, diagnostics and upstream adaptation.
Subsystem READMEs and source comments describe responsibilities, ownership,
revocation, native integration and the generated-source boundary. Qualify includes
by subsystem; supply owned adapter include paths in our CMake integration only.
Ladybird remains unmodified at the existing manifest/gitlink pin.

Author the desktop API and DOM app-region installer as ESM and generate committed
synchronous native adapters with their own reproducible source/output manifest.
Native builds verify it without Node. Mite's generator and standalone integrity
remain independent of Firefly. The website and Designer browser scripts now use
module loading too. Keep actual CommonJS compatibility fixtures and third-party
bundles in their intended formats; generated embedding expressions are internal ABI.

Separate the 60 Python regression CLIs into tooling/tests, update CMake/CI/docs
commands, and retain their standalone helper imports and fixture roots. Apply the
pinned formatter policy across native, Python, JS/web/config, CMake and Rust files:
normal indentation, expanded dense statements and blank lines between definitions.
Four intentionally malformed parser inputs have named exceptions. Add a source
style workflow that also reproduces both generators. Comments explain contracts
and non-obvious behavior; getters do not acquire repetitive boilerplate.

Validation on macOS ARM64:
- Standalone Mite and production Firefly rebuild successfully from the new paths.
- All 44 runtime CTests pass (29.36 s; refactor-final-mite-tests.log), including
  CommonJS/ESM identity, npm packages, integrity and Windows-style checkout bytes.
- All four native desktop/renderer/isolation/helper gates pass unchanged security,
  GC, process and callback assertions (refactor-final-native-gates.log).
- Fresh gallery staging passes six shared native Firefly/Electron startup cases
  plus app.exit(7); the other 17 entries still reject unsupported APIs explicitly
  (refactor-final-gallery.log). No additional desktop API support is claimed.
- Fresh production-bundled Designer passes assets/server checks and actual LibWeb
  boot, document editing/round-trip, Monaco and HTML window controls
  (refactor-final-designer-tests.log). Asset boundary and fixed window-capability
  checks also pass independently under Mite and the Node reference.
- Style checks cover 95 Python, 358 JS/web/config, 119 native and 12 CMake sources
  plus the Rust source. Both generators reproduce exactly; integrity, actionlint
  and unchanged-upstream checks pass. Site validation passes 141 pages and 8125
  local links/resources/fragments. Logs use build/refactor-final-* (not committed).

The readability priority is complete locally. Hosted runs are a separate platform
acceptance step; these results add no unverified Windows/Linux desktop claim or
new release. The overall product goal is still incomplete. Next resume the parked
native invoke/handle transport from stash d8e6a1c728c50b419894dd09ba81a41b42e08af4,
porting it to the new ESM and subsystem paths before native/security validation.
Public IPC remains unsupported until that later implementation is tested.

## 2026-11-06 — Native invoke/handle IPC vertical slice

Previous goal turn: progress (readability refactor committed/pushed and validated).
Hosted style, Pages, native Linux and native Windows gates for 61130d1 now all
succeed. macOS d585807 run 42181805021 is still confirmed running; the optimized
61130d1 candidate 42187878151 remains pending. Neither is restarted or labelled a
new alpha, and existing alpha.1 remains the earlier immutable release.

Port the parked native request/reply implementation to the new subsystem layout
and maintained ESM sources, then correct and validate it. Main handler management
is a separate core/ipc-main.mjs module. Export ipcMain from the main host module;
preload imports gain ipcRenderer.invoke. Support handle/handleOnce/removeHandler,
sync/async results, concurrent replies and bounded errors. Preserve standalone
Mite independence and the unmodified synchronized Ladybird pin.

Compose reserved events/commands over Ladybird's actual native routed connection,
with an owned fingerprinted WebContentPage seam and the owned renderer command
adapter. Authenticate actual page/navigable, active document and owning window;
select the URL from canonical native state. Check realm/request identity and
replays. Source callbacks enter the Mite event-loop boundary. Reply settlement
uses both an HTML execution context (for the microtask checkpoint) and the guarded
world frame. Raw native invoke functions cannot be exported through contextBridge;
fixed application wrappers remain possible. Navigation/crash/window teardown
reclaims pending reservations and timers, and late replies cannot revive a world.

Use the existing bounded plain-data graph codec: mutable destination copies,
cycles/aliases/holes, UTF-16 and IEEE number edge cases, no accessors/proxies/host
objects/functions. Limits are 16 endpoint requests, 64 per renderer process and
64 native host requests, 128 KiB encoded data and a real 30-second deadline.
Correct host exhaustion to send an immediate bounded failure without reserving an
extra slot or running JS in a busy VM. Pure native error strings use the codec,
rather than hand-written wire bytes. Refresh sender-frame URLs for same-document
requests. Limited frame snapshots are documented; full WebFrameMain is absent.

Validation:
- All four native desktop/renderer/isolation/helper groups pass
  (ipc-native-gates.log), retaining existing callback/principal/GC and wire guards.
- Final rebuilt check-firefly passes after ESM extraction and added tests
  (ipc-final-main-gate.log). Actual Electron/Firefly fixtures cover early preload,
  sync/async/once/missing/error handlers, parallel replies, clones and native sender
  ownership. Firefly-only tests cover trap-free rejection, raw IPC publication
  denial, 16 local slots, 3 navigation cycles plus same-document URL refresh,
  pending replies after closure and a real owned renderer SIGKILL/replacement.
  Five windows verify 64 host slots, prompt global rejection and recovery. A
  never-resolving handler verifies the actual 30-second deadline and recovery.
- Designer native regression passes boot/edit/round-trip/Monaco/window controls
  (ipc-designer-regression.log). Standalone Mite host-module/embedding/generated
  independence checks pass (3/3; ipc-mite-independence.log); unchanged Mite sources
  retain the preceding 44/44 full-suite result, not a newly claimed full run.
- Source style passes 97 Python, 381 JS/web/config, 123 native and 13 CMake sources
  plus Rust; both generators reproduce, integrity/actionlint/upstream pass.
  Site passes 143 pages / 8396 links/resources/fragments (ipc-final-site.log).

Add detailed API contract, migration/security updates and ADR 0072. Move the pinned
preload bundler installation before native desktop tests in the release script;
normal native compilation still requires no Node install. Public status is partial
macOS ARM64 only, not full Electron compatibility or a new packaged release.
The original WIP stash is retained as a safety checkpoint; it is now ported and
must not be blindly reapplied.

Remaining: one-way/sync/port/subframe IPC, full clone types, stronger navigation/
permission policy and a complete security audit; broad Mite APIs, desktop services
and native Windows/Linux desktop/packaging remain incomplete. The fixture work
also exposed failed local file-origin external ESM loading (null script result).
Tests stage inline modules from maintained ESM files to isolate IPC acceptance;
HTTP external modules retain the Designer evidence. Next investigate that local
module-loading failure, then continue the broader specification and current-source
hosted release acceptance. The overall goal remains active.

## 2026-11-06 — External file renderer ESM repaired

Previous goal turn: progress (native request/reply IPC committed and pushed).
Hosted style, Pages and native Linux/Windows Mite checks for c74bec9 succeed.
The d585807 macOS release run 42181805021 is still confirmed in progress; optimized
c74bec9 run 42191274409 is pending. Do not restart a confirmed running build or
claim either candidate is a new release.

Reproduce classic file script success alongside external/dynamic module failures
in actual Firefly; the same file graph works in the actual Electron reference.
Locate the failure in Fetch's CORS branch: opaque file origins miss the same-origin
condition and non-HTTP CORS fails before the existing file subresource loader.

Add a small owned loading policy/adapter, with the exact upstream fingerprint,
one-condition/source replacement counts and byte-preservation/drift guards.
Keep Ladybird unmodified. The production helper alone enables the policy; default
browser/diagnostic helpers stay disabled. It admits CORS-mode file Script requests
from file-origin Window clients with native file creation snapshots. Existing file
loader, MIME, CSP and sandbox behavior remain. No general CORS switch, fetch(file:)
permission, Node loader, inlining or loopback rewrite is introduced. Workers,
opaque/network clients, missing snapshots and other destinations/modes are denied
by this exception. This is not an application-root path sandbox.

Validation completed before the user's GUI-test frequency correction:
- Actual Electron/Firefly external file module fixtures pass static/dynamic imports,
  live bindings/cache identity, Unicode/space paths, reload, strict MIME/CSP and
  absent native imports. Firefly-only pages retain fetch(file:) and HTTP-to-file
  module refusal (file-modules-native-test.log).
- Native policy tests verify default-off behavior and rejected request/client
  combinations inside check-firefly-isolation (file-module-policy-test.log).
- All four native desktop/renderer/isolation/helper groups pass with the new
  adapter (file-modules-all-gates.log). IPC fixtures now load external .mjs files
  directly; the inline staging workaround is removed, retaining sender, malicious
  data, pending budgets, same-document/navigation, crash and real-deadline tests.
- Designer boot/edit/round-trip/Monaco/window controls regression passes
  (file-modules-designer-test.log). Standalone Mite host-module/embedding/generated
  independence checks pass (3/3, file-modules-mite-tests.log). No new full 44-suite
  Mite run is claimed for this renderer-only change.
- Style passes 99 Python, 393 JS/web/config, 127 native and 14 CMake sources plus
  Rust. Both generators reproduce; integrity/upstream/actionlint pass. Site checks
  pass 145 pages / 8523 local links (before this progress entry). The preceding
  IPC entry's final site log actually counted 8398 links, correcting its 8396 note.

Document the native loading contract, migration distinction and update guards in
ADR 0073 and the renderer module guide. Current-source hosted package acceptance
and broader specification work remain outstanding; no Windows/Linux desktop or
new alpha package is claimed. Existing alpha.1 predates these changes.

User workflow preference: local GUI testing is allowed, but must be infrequent,
targeted and batched because it interrupts their desktop. Persist this in AGENTS.
Use native builds, non-GUI checks and hosted CI for routine edits; avoid repeated
window/compositor/input/Designer runs for docs/formatting or unchanged behavior.
At correction time no UI test was still running; no additional GUI run was started.

Next: follow hosted acceptance and continue the missing IPC message modes/desktop
services and runtime API/platform work in the specification. Keep readability,
commented ESM authoring and limited local GUI-test frequency as standing constraints.
The overall goal remains active.

## 2026-11-06 — Native setImmediate/check-phase runtime slice

Previous goal turn: progress (external local renderer modules fixed, committed and
pushed, and local GUI-test frequency preference persisted). At this turn's initial
check, 439667b style/Pages succeed; its Linux/Windows Mite runs remain in progress
and optimized macOS run 42193613073 is pending. Do not treat an observation timeout
as a stopped native build. No local GUI tests are run in this step.

Implement the missing Node check-phase callbacks in a separate commented native
core/ImmediateQueue, shared by the CLI and embedded hosts. Use one libuv check
handle plus a temporary unreferenced idle handle to avoid blocking before that
phase. Stop both on empty queues; referenced pending calls control liveness. Drain
one FIFO snapshot per iteration, checkpoint nextTick/jobs between callbacks and
leave newly scheduled calls for a later iteration. Remove records before invoking
JS; retain callback/receiver/argument GC roots through the checkpoint. Cancel roots
promptly, compact stale IDs and free native handle storage during shutdown without
running queued JS. Preserve unmodified Ladybird and Mite's LibWeb independence.

Maintain setImmediate/clearImmediate and Immediate ref/unref/hasRef/dispose policy
in bootstrap ESM with private hooks removed before application entry. Fired or
cancelled handles cannot be reactivated. Add a cached node:timers/bare timers
module with the six original global functions, captured before application code;
CJS and ESM default/named imports share the same functions. No timers/promises,
async_hooks/context or complete Node phase parity is claimed.

Validation:
- Standalone native build succeeds. All 45 CTests pass in 57.74 seconds
  (immediates-final-tests.log), including the new immediate suite, existing
  TCP/HTTP/process/stream/crypto/module gates and Windows-style checkout bytes.
- Seventeen actual Node 24.15.0/Mite comparisons exercise CJS/ESM FIFO/checkpoints,
  callback/argument/receiver identity, ref state, cancellation including batch
  removal/stress, disposal, liveness, fatal errors, validation, I/O phase ordering
  and module/global identity (immediates-tests.log). Startup timer vs immediate
  races are deliberately not asserted as deterministic.
- The non-GUI embedded host counts native pump iterations and forces GC while
  arguments/callbacks are pending. Nested immediates require a subsequent pump,
  ticks stay in the current checkpoint, and exactly-once shutdown still passes.
- Firefly compiles with the same updated runtime (immediates-final-firefly-build.log).
  No window/compositor/input/Designer suite is launched locally; hosted desktop
  acceptance remains required for this changed main-process runtime.
- Source style passes 100 Python, 404 JS/web/config, 129 native and 14 CMake sources
  plus Rust. Both generators reproduce; source integrity/upstream checks pass.
  Documentation validates 147 pages / 8782 local links before this progress entry.
- New authored ESM CLI event-loop example runs under actual Mite and Node with
  identical output. Add the detailed timers API reference and ADR 0074.

Remaining goal scope is unchanged: complete runtime module/API/event-loop behavior,
AbortSignal/cancellation, TLS/HTTPS/fetch and broader packages; remaining desktop
IPC modes/services; native Windows/Linux desktop and distribution/security maturity.
Continue coherent vertical slices with honest platform evidence and infrequent,
batched local GUI acceptance, using hosted tests for routine desktop regressions.
The overall goal remains active.

## 2026-11-06 — Cancellation globals and native Promise timer consumers

Previous goal turn: progress (check-phase immediates committed/pushed). Hosted
Linux/Windows Mite, style and Pages gates for de663f3 now all succeed. Optimized
macOS de663f3 run 42195524566 is pending behind the confirmed running 439667b
release build 42193613073. Keep live runs rather than restarting on observation
timeouts. This step launches no local GUI/desktop tests.

Install realm-local Event/EventTarget/DOMException/AbortController/AbortSignal
without LibWeb or Node. Reuse separately pinned event-target-shim 6.0.2 for public
portable events while preserving readable-stream's existing private 5.0.1 version.
Maintain abort reason/idempotence/timeout/composition and DOMException policy as
commented bootstrap/web ESM; generated adapters preserve exact lock, dependency
source hashes and complete notices. Check authored bootstrap inputs separately
with repository-relative hashes. Normalize dependency metadata paths for Windows.
Normal native builds still require no npm install. Package staging now includes
all verified portable-runtime notice sidecars and their manifest, and refuses
modified notice bytes before copying them.

AbortController has one branded signal, preserved reasons and one-shot state.
Support throwIfAborted, abort/timeout/any and onabort/listener dispatch. Composition
marks dependent state before callbacks; private consumer algorithms are independent
of public event propagation. ModuleLoader roots the private hook object and passes
it only to trusted builtin factories. AbortController wrong-receiver errors and
three obsolete DOMException numeric mappings were corrected using actual Node
observations rather than assumptions. Timeout signals are unreferenced.

Add node:timers/promises/bare alias with shared default/named timeout/immediate
functions, values and signal/ref options. Cancelling rejects AbortError/ABORT_ERR
with cause and clears the actual native handle; settlement releases subscriptions.
Ref:false does not turn a pending Promise into native liveness. Promise intervals,
scheduler and cancellation of other I/O services remain unsupported.

Validation:
- Standalone rebuild and all 47 then-registered CTests pass (30.39 seconds,
  abort-verified-tests.log). Sixteen actual Node/Mite CJS/ESM scenarios include
  controllers, composition/propagation, EventTarget, reasons/exception codes,
  deadlines, real timer/immediate consumers, option errors, listener failures,
  references and module identity. Two differences have explicit assertions:
  portable abort isTrusted metadata is false; synthetic public abort events do
  not run Mite's private timer-cancellation algorithms (Node cancels the timer).
- Audit found test_runtime_line_endings.py existed but was not registered in CTest;
  preceding full-suite claims did not by themselves prove that check ran. Wire it
  into CMake and verify it directly plus as the new CTest (1/1, 1.61 seconds,
  abort-line-ending-ctest.log). The configured suite now contains 48 tests; no
  single full 48-test invocation is claimed. Windows-mode checkout preserves all
  current integrity-protected source/adapter/license bytes.
- After the path-normalization-only builder correction, both generators reproduce;
  integrity and runtime notice tests pass (2/2, abort-final-integrity-tests.log).
  Exact-source/upstream and actionlint checks pass. Ladybird pin remains unchanged.
- Firefly rebuild succeeds with the updated shared runtime
  (abort-final-firefly-build.log). Desktop smoke/relocation remains hosted work;
  no additional local window/compositor/input/Designer tests are run.
- Source style checks pass 102 Python, 419 JS/web/config, 129 native and 14 CMake
  sources plus Rust; the final CMake-only test registration is checked separately.
  Documentation passes 149 pages / 9049 local links before this entry.

Add API reference, ADR 0075 and a modern ESM cancellation example. Public support
is partial, not full Node/Web conformance: foreign/polyfill signals in Promise
timers, complete EventTarget options/descriptors and composite listener-only GC/
finalization/large dependency-graph lifetime remain unverified or unsupported as
specified in the API. Desktop/native platform acceptance of this source is pending;
no new alpha or unsupported platform claim is made. Overall goal remains active.

Next: follow hosted acceptance and strengthen cancellation lifetime/composition
before extending I/O consumers. Continue TLS/HTTPS/fetch, remaining runtime APIs,
IPC message modes/desktop services and native Windows/Linux desktop/distribution
work. Keep readable ESM/native boundaries and infrequent batched local GUI tests.

## 2026-11-06 — Weak target checkpoints and cancellation cleanup

Previous goal turn: progress. Linux Mite, source style and Pages for cd8f796
succeed. Windows compiled and passed runtime behavior except the newly registered
line-ending fixture: Python text-mode stdin supplied CRLF paths to checkout-index.
Fix that test transport separately. Optimized macOS run 42200655334 is in progress;
no new release or desktop-platform acceptance is claimed here.

Fix forced-GC failures entirely in owned adapters. WeakRef construction/deref
retains object/symbol targets until the complete nextTick/Promise checkpoint, then
advances LibJS execution generation. Queue rooted finalization registries through
an otherwise unreferenced libuv async handle; run cleanup on the VM owner thread
with checkpoints and fatal-error policy. Restore the original cleanup hook on
teardown. Adapt native unregister tokens through weak keys and independent group
tokens because the pinned engine otherwise traces target-as-token strongly.

Track actual abort listeners, capture identity, once/removal, onabort and guarded
removal. Reachable sources conditionally retain observed composites. Timeout
callbacks retain observed signals through weak/strong slots; cleanup holdings have
no strong path back to their signal. Collection releases unused dependency links
and actual native timer handles. Keep closure scopes separate to avoid accidental
capture. Add API/concept documentation and ADR 0076 with lifetime limits.

Validation:
- All 49 registered CTests pass before the final original-hook preservation fix
  (79.90 seconds, lifetime-all-tests.log), including twelve actual Node 24.15.0/
  non-GUI embedded-host lifetime comparisons and existing cancellation/I/O suites.
  The final native rebuild and repeat full suite also pass all 49 tests
  (lifetime-hook-tests.log).
- Cases cover same-job object/symbol retention, later collection, asynchronous and
  nested cleanup, callback failure/teardown, weak shared unregister tokens,
  listener-only composites, removed/once/guard/attribute release, timeout delivery
  and native handle counts. GC timing and arbitrary dependency graphs are not
  universal conformance claims; explicit close/abort remains required.
- Both generators reproduce; source integrity, upstream and actionlint checks pass.
  Style passes 103 Python, 433 JS/web/config, 129 native and 14 CMake sources plus
  Rust. Site validates 150 pages / 9112 local links including this progress entry.
- No local GUI tests or desktop applications are launched. Firefly is compiled
  only; both rebuilds succeed (lifetime-firefly-build.log and
  lifetime-hook-firefly-build.log). Hosted desktop and native platform acceptance
  remain separate.

Overall goal remains active. Continue broader cancellation lifetime/consumer
integration, TLS/HTTPS/fetch, remaining runtime APIs, desktop IPC/services and
native Windows/Linux desktop/distribution work. Keep GUI acceptance infrequent,
targeted and batched, using headless tests and hosted CI for routine validation.

## 2026-11-06 — Binary Git path transport for Windows checkout regression

Windows cd8f796 compiled successfully but failed the line-ending regression test:
text-mode subprocess stdin converted LF-delimited paths to CRLF, and checkout-index
looked up filenames containing a trailing carriage return. Use UTF-8 binary,
NUL-delimited paths with checkout-index -z, avoiding platform newline translation.
No runtime behavior or source-integrity expectation is relaxed.

Validation: direct checkout regression passes; registered CTest passes 1/1 in
1.62 seconds; Ruff formatting passes. The preceding runtime commit's final full
49-test run passes in 105.00 seconds. Hosted Windows validation of this correction
is pending; no Windows desktop support is claimed. No GUI tests are launched.

## 2026-11-06 — Flatten nested cancellation sources

Previous goal turn: progress (36f7e2c and 22d1b14 committed/pushed). Source style
and Pages for 22d1b14 now succeed; native Linux/Windows remain in progress.
Optimized macOS 42200655334 is still running; the newer optimized run 42205271586
is pending. Preserve live builds. No new alpha or native desktop acceptance is
claimed in this step.

Nested AbortSignal.any now links directly to deduplicated original sources,
instead of retaining intermediate composite graphs. Empty composites contribute
no abort sources. Preserve complete iterable validation and first-already-aborted
selection before flattening. Reachable sources retain observed outer signals;
intermediates can be collected without losing delivery or needing recursive
retention propagation. All changes stay in owned ESM/generated adapters, with the
Ladybird pin unchanged.

Validation:
- Actual Node 24.15.0 and non-GUI Mite embedded-host tests add diamond/deduplicated
  graphs, collected intermediates and reachable outer signals. A separate nested
  listener-only fixture exposes a Node reference defect: all 32 notifications are
  lost after intermediate cleanup. Assert that result explicitly and Mite's 32
  deliveries separately. This is a documented correctness difference, not claimed
  equality. Fifteen lifetime scenarios now run in the registered lifetime suite.
- Standalone rebuild succeeds; all 49 CTests pass in 92.30 seconds, including
  existing cancellation/timer/I/O tests (nested-all-tests.log).
- Source style checks pass 103 Python, 436 JS/web/config, 129 native and 14 CMake
  sources plus Rust. Both generators reproduce; runtime/Firefly integrity,
  unmodified upstream and actionlint checks pass. API/ADR explain flattening and
  the measured reference difference. Site validates 150 pages / 9115 links including this entry.
- Firefly compile-only verification succeeds (nested-firefly-build.log).
  No local GUI tests or desktop applications are launched.

Next: extend cancellation to real I/O consumers, then continue TLS/HTTPS/fetch,
remaining Node/Electron APIs and native desktop/distribution work. Arbitrary GC
schedules and complete Web/Node conformance remain unverified. Overall goal active.

## 2026-11-06 — AbortSignal cancellation for native TCP resources

Previous goal turn: progress (nested cancellation b5cadad committed/pushed).
Hosted Linux and Windows Mite for 22d1b14 both succeed, including the corrected
binary checkout regression and weak/finalization lifetime suite. Source style and
Pages and native Linux Mite for b5cadad succeed; its Windows Mite run remains
in progress. macOS optimized run 42200655334 remains running; newer
optimized b5cadad run 42206030456 is pending. Preserve live runs; no new alpha or
Windows/Linux desktop package is claimed.

Pass realm-local private abort hooks into node:net. Support signal in Socket
construction/createConnection/connect factory and Server.listen. Socket abort
destroys the actual native transport with AbortError/ABORT_ERR and original reason
cause; late DNS results cannot reconnect it. Pre-aborted errors defer until callers
can install listeners. Discard the signal option before entering the portable
stream constructor to avoid a second incompatible consumer. Dispose subscriptions
on destruction or failed factory startup. Direct Socket.connect signal options
and createServer signal options still reject explicitly; associate the signal at
construction or listen instead.

Server abort uses ordinary close: stop accepts, preserve accepted connections and
wait for them to drain. Defer numeric listen startup, suppress pre-aborted listening
notification, bind subscriptions to native transport identity and dispose them on
close/bind/lookup failure. A failed listener's old signal cannot close a new one.
A queued write waiting for connect reports ERR_SOCKET_CLOSED_BEFORE_CONNECTION
separately from the Socket's original abort error.

Actual echo/once-data tests exposed an existing EventEmitter defect: once bypassed
Readable's overridden on and failed to start data flow. Route once/prependOnce
through public subscription hooks, with a dedicated subclass identity/order test.
Add detailed node:net API, ADR 0077, compatibility updates and a modern ESM TCP
cancellation example with composed deadline and separate listener/socket lifetimes.

Validation:
- Standalone rebuild succeeds; all 50 CTests pass in 61.96 seconds
  (net-abort-all-tests.log). Thirteen new actual Node 24.15.0/Mite TCP scenarios
  exercise connected/pending/pre-aborted resources, buffered writes and late
  resolver callbacks, propagation stops, accepted-client survival, validation,
  startup cancellation, manual-close reuse and failed-bind reuse.
- Preserve explicit assertions for measured differences: synthetic public events
  do not cancel Mite's private native consumers; unconnected pre-aborted sockets
  remain destroyed (reconnect unsupported); failed bind releases its stale signal
  (Node 24 can let that signal close the next listener). No output normalization
  disguises these differences. Existing TCP independent OS-peer/fatal/unsupported
  gates and EventEmitter/streams/HTTP/lifetime regressions pass in the full suite.
- Firefly rebuild succeeds (net-abort-firefly-build.log). No local GUI tests or
  desktop applications are launched. Hosted desktop/platform acceptance remains
  separate for this source.
- Both generators reproduce; runtime/Firefly/upstream integrity and actionlint
  pass. Style checks cover 104 Python, 450 JS/web/config, 129 native and 14 CMake
  files plus Rust before adding the example. The additional example is formatted
  and passes its targeted style check (451 maintained JS files in total).
  Documentation validates 152 pages / 9392 links including this entry/example links.
- The TCP cancellation example runs in actual Mite and Node with identical output:
  echo payload, original abort reason, then listener closure.

Next: extend I/O cancellation to HTTP/process/filesystem/stream consumers and
continue TLS/HTTPS/fetch, broader runtime APIs and desktop IPC/services/native
Windows/Linux distribution work. Native DNS request interruption, complete TCP
phase/descriptor/reconnect parity and arbitrary event/GC cases remain open.
Overall goal remains active; keep local GUI acceptance infrequent and batched.

## 2026-11-06 — Bound pre-aborted TCP reference peer lifetime

Linux run 42207827651 compiled 4b6f5f9 successfully but timed out in Node
24.15.0's pre-connecting TCP fixture, before Mite was run for that case. The fixture
waited for server.close while an accepted reference peer remained open during
pre-aborted Socket startup. Explicitly destroy accepted test peers. Preserve the
same zero-connect, one-AbortError and cause assertions; established cancellation
and independent OS-peer gates separately prove native closure. Do not turn a
reference timeout into a supported-platform success claim.

Validation: all thirteen actual Node/Mite TCP cancellation cases pass locally
(http-abort-reference-cleanup-tests.log). New hosted Linux/Windows verification is
pending. Native Windows b5cadad now succeeds. Source style/Pages 4b6f5f9 succeed;
its native Windows run remains pending observation. No GUI tests are launched.

## 2026-11-06 — Request-lifetime HTTP AbortSignal cancellation

Previous goal turn: progress (native TCP 4b6f5f9 committed/pushed). Its source style
and Pages succeed; Linux and Windows compiled but both failed in the same Node
reference pre-aborted TCP peer teardown fixture. Commit d3aafbe gives accepted
reference peers an explicit lifetime without relaxing connect/error assertions.
Rerun hosted gates after pushing the coherent HTTP step. macOS optimized
42200655334 is still running; newer optimized 4b6f5f9 42207843778 is pending.
No new alpha or Windows/Linux desktop support is claimed.

Associate private realm-local abort hooks with HTTP ClientRequest, not its Socket.
Support request/get/new ClientRequest options and URL overloads. An actual abort
destroys the request with AbortError/ABORT_ERR and original reason cause, closes
its native transport and discards late resolver results. Pre-aborted requests are
destroyed on return while error/close delivery remains deferred. Cancellation can
occur during deferred socket assignment, lookup, upload, response or paused body.
Do not detach at upload finish: the response still belongs to that request.
Dispose subscriptions on destruction/normal transport closure.

Queue request errors before closing an unconnected Socket; abort incomplete
responses during native socket teardown, preserving tested request error / response
aborted / request close / response ECONNRESET error-close ordering. Retain one-shot
failure/close guards and completed-body state. Real cancellation is independent of
public event propagation; synthetic events still intentionally do not run private
consumers. Inherited HTTP Server.listen signal cancellation is now explicitly
accepted with a graceful response from an already accepted connection.

Validation:
- Standalone rebuild and all 51 CTests pass in 64.60 seconds
  (http-abort-all-tests.log). Sixteen actual Node 24.15.0/Mite HTTP cancellation
  scenarios cover pre/immediate/socket-association/lookup races, upload finish,
  partial and paused responses, deadlines, URL options, types, propagation stops,
  settled disposal and graceful listener closure. Synthetic-event differences are
  asserted separately, not normalized into equality.
- An independent Python OS socket peer observes EOF/reset after cancellation
  during an incomplete response and an open 8 MiB upload. This verifies native
  closure separately from application flags and two copies of Mite.
- The non-GUI lifetime suite now has sixteen scenarios: eight completed HTTP
  requests become collectable while their original controller remains reachable.
  Existing HTTP client independent-peer/framing/EOF/parser/fatal/policy workflows
  pass unchanged except removing signal from the unsupported-option list; invalid
  signals now have explicit ERR_INVALID_ARG_TYPE coverage.
- The subsequent targeted TCP cancellation suite passes all thirteen cases after
  the peer cleanup fix. Hosted Linux/Windows acceptance of that fix remains pending.
- Source style passes 105 Python, 468 JS/web/config, 129 native and 14 CMake sources
  plus Rust. Both generators reproduce; runtime/Firefly/upstream integrity and
  actionlint pass. Ladybird remains unmodified at its pinned revision.
- Firefly rebuild succeeds (http-abort-firefly-build.log). No local GUI tests or
  desktop applications are launched. Hosted desktop/platform acceptance remains
  separate for this source.
- Modern ESM HTTP cancellation example executes under actual Mite and Node with
  identical payload/reason/incomplete-body/server-close output. Add API/cancellation
  reference, compatibility updates and ADR 0078. Site validates 153 pages / 9456
  links including this progress entry.

Next: continue TLS/HTTPS/fetch and broader runtime I/O/services, including remaining
filesystem/process/general stream cancellation; complete desktop IPC/services and
native Windows/Linux distribution/security maturity. OS DNS interruption, Agents,
pooling and complete HTTP lifecycle/platform/error conformance remain open. The
overall goal stays active; keep local GUI acceptance infrequent and batched.

## 2026-11-06 — Record hosted reference races and bound Windows test teardown

Previous goal turn: progress (HTTP 8698725 and peer cleanup d3aafbe committed/pushed).
Hosted Linux/Windows build that source and pass the new HTTP cancellation suite.
Both expose Node 24.15.0 delivering one late connect after pre-abort/error in the
TCP reference fixture; local macOS delivers zero. Assert the two observed reference
outputs separately, preserving the strict zero-connect / one-correct-abort-error
requirement for Mite. Do not normalize that Node difference into equality.

Windows also fails temporary-directory deletion with WinError 32 after the orphan
survival workflow; the source fixture still has an OS file lock at teardown. Add a
bounded five-second Windows PermissionError cleanup retry. Parent exit/capture and
child-survival acceptance deadlines/assertions remain unchanged; persistent locks
still fail. This is test cleanup, not evidence that all native process inheritance
or Windows file semantics are correct.

Validation: targeted TCP/process regression and formatting are recorded in this
turn's verification; new hosted acceptance remains required. No GUI is launched.
Optimized macOS 8698725 run 42209659578 is now running, so preserve it rather than
restart it. Native Windows/Linux desktop and new alpha acceptance remain pending.

## 2026-11-06 — Native TLS client records and verified stream transport

Previous goal turn: progress (HTTP cancellation committed/pushed). Hosted 8698725
Linux and Windows pass HTTP cancellation but reveal a Node late-connect race in
the pre-aborted TCP reference: retain Mite's strict zero-connect requirement and
assert observed reference variants separately. Windows also encounters a locked
process-test source at directory teardown. Commit 0cad92a isolates these harness
corrections without weakening functional deadlines; new hosted verification is
pending. Optimized macOS 8698725 run 42209659578 is confirmed running; preserve it.

Add a real node:tls client over existing libuv TCP. Native OpenSSL memory BIOs own
handshake/records/certificate verification and return copied byte snapshots. Use
SSL in the existing pinned OpenSSL build; keep Mite independent of LibWeb/Firefly
and Ladybird unmodified. Separate native provider/OS trust files under network/tls
from the authored ESM Socket/Duplex adapter; reproduce the synchronous embedding
factory and checked manifest. Shared Firefly provider integration continues using
the existing SSL/Crypto targets rather than adding another engine or dependency.

Support TLS 1.2/1.3, DNS/IP SAN verification, full-label wildcards, explicit/default
anchors, ALPN/SNI, queued writes, one MiB transfer/read pressure, close_notify and
AbortSignal teardown. Client streams are Socket/TLSSocket instances. Serialize
application records in 16 KiB operations and bound native snapshots/pending bytes.
No provider retains JS pointers. Private signal cancellation disposes subscription
and native transport/provider state, including pending handshakes. System anchors
use macOS Security, OpenSSL Windows ROOT provider and Linux distribution/default
locations; complete OS trust-policy/revocation and deployed default trust remain
unverified. Custom ca replaces defaults; explicit permissive mode records failure.
CN fallback/partial wildcards and implicit environment verification bypass are not
supported. TLS servers/reusable contexts/wrapping/client certs/ciphers/sessions/
inspection and HTTPS/fetch remain explicit unfinished requirements.

Validation:
- Actual Node 24.15.0/Mite clients pass 34 version/case comparisons against separate
  Node/OpenSSL peers: both versions, custom CA/arrays, SNI/ALPN/no-ALPN, IP/wildcard,
  expired/self-signed/untrusted/hostname failures, permissive chain/host modes,
  early writes, one MiB pressure, GC and abort. The non-GUI embedded host forces
  GC before handshake and after verification. Default-store rejection is checked;
  no claim covers every public/enterprise certificate policy.
- Sixteen malformed/unsupported API policies reject explicitly, including bad
  versions/prototype names, CA types/UTF-16, ALPN bounds, signals and unavailable
  server/context modes. An independent raw OS peer observes ClientHello then EOF
  when each runtime cancels a stalled handshake, without exposing application data.
- A TLSv1.3 example runs in actual Node/Mite with identical trusted echo output.
  It exposed a missing-ALPN null StringView crash before commit: handle absent
  selection as an empty value and add no-ALPN cases for both versions.
- Final standalone rebuild and all 52 CTests pass in 40.19 seconds
  (tls-final-tests.log), including existing HTTP/TCP/streams/crypto/process and
  integrity/checkout suites. Earlier full TLS runs also pass, but the final run
  includes the no-ALPN regression and bounded plaintext-read correction.
  A subsequent targeted TLS run also passes after adding positional-connect
  overload coverage to the existing CA-array case (tls-positional-tests.log).
- Firefly builds with the same SSL-linked runtime (tls-final-firefly-build.log).
  No local GUI/desktop tests are launched. otool shows platform libraries only,
  no Node/V8/Qt/LibWeb dependency in standalone Mite; Security/CoreFoundation are
  system trust dependencies. Hosted native/desktop acceptance remains pending.
- Style passes 106 Python, 474 JS/web/config, 133 native and 14 CMake sources plus
  Rust. Both generators reproduce; runtime/Firefly/upstream integrity and actionlint
  pass. Add TLS API reference, compatibility status, ESM example and ADR 0079.
  Site validates 155 pages / 9732 links including this entry.

Next: build HTTPS/fetch on the verified client transport, extend TLS options/trust/
metadata/closure coverage as needed, and continue remaining runtime I/O/services,
process/filesystem/general stream cancellation, desktop IPC/services and native
Windows/Linux distribution/security work. The overall goal stays active; local
GUI acceptance remains infrequent, targeted and batched.

## 2026-11-06 — Reject unavailable TLS provider policies explicitly

The final TLS API audit found provider/security and unsupported TCP options could
otherwise be ignored by the new facade. Reject secureOptions, sigalgs, ecdhCurve,
OCSP/partial-chain/client-key settings and unsupported binding/descriptor/IPC
fields before allocating/connecting. Keep defaults explicit; do not pretend a
requested provider policy was applied. Forward supported keepAlive/initial-delay
options to the existing TCP implementation and exercise them in shared TLS cases.

Validation: all 52 CTests pass in 38.04 seconds (tls-policy-tests.log). The native
TLS suite now checks 23 explicit option/mode rejection cases plus the unchanged
34 encrypted workflow comparisons, forced GC and independent stalled-handshake
closure. Generated adapters reproduce current sources; Firefly compile-only succeeds
(tls-policy-firefly-build.log). Integrity/upstream/actionlint and style checks
pass; site validates 155 pages / 9733 links. No GUI is launched.
Native platform acceptance of this source remains pending. Overall goal active;
HTTPS/fetch and the broader specification remain unfinished.

## 2026-11-06 — Keep Windows native TLS evidence independent of a crashed reference

Previous goal turn: progress (TLS clients ec6d682 / policy 3981a56 committed/pushed).
Native Linux ec6d682 succeeds. Windows compiles but its Node 24.15.0 TLS reference
client exits 3221226505 (0xC0000409) without stderr in the first TLSv1.2 echo case;
that stops comparison before the Mite client is exercised. Do not call the gate
successful or assume this proves a Mite defect. The exact reference crash cause is
not established from that exit code.

Add an explicit --native-only mode to the same TLS harness. It retains ciphertext,
trust, hostname, pressure, GC and cancellation assertions while omitting only the
reference client; the independent Node/OpenSSL server remains. Windows runs it
after a successful compile even if the differential step fails, preserving a
separate diagnostic log/artifact. The normal Node comparison remains required and
still fails on a reference crash; this diagnostic does not turn it green.

Validation: native-only TLS acceptance passes locally against independent peers
(https-turn-tls-native.log), with formatting and actionlint. New Windows diagnostic
and complete platform acceptance remain pending. No GUI tests are launched.

## 2026-11-06 — Verified HTTPS clients over shared HTTP classes

Previous goal turn: progress (native TLS/policy commits pushed). Native Linux and
Windows 3981a56 now both succeed, including the 52-suite TLS baseline; the earlier
Windows Node 0xC0000409 failure did not recur in that run. Keep the explicit
independent native diagnostic introduced by 77d2e67 so a future reference crash
cannot hide Mite evidence. Comparison failures still fail the gate. Optimized
macOS 8698725 run 42209659578 remains confirmed running; preserve it. Current-source
HTTPS/guard acceptance on native platforms remains pending; no new alpha is claimed.

Add node:https request/get using the authoritative http.ClientRequest and
IncomingMessage classes. A private rooted HTTP transport plan selects https:/443,
unconnected TLSSocket creation and secureConnect activation. TLS and HTTP expose
constructor hooks only to trusted module factories; public ClientRequest extra
arguments cannot inject plans. node:http still rejects https: and never chooses a
plaintext fallback. Derive SNI from Host/hostname with IP suppression and explicit
override; allow HTTP/1.1 ALPN or no ALPN, reject HTTP/2. Forward TLS provider fields
for explicit validation, while HTTP owns path/framing/signal/timeout.

A bad-certificate HTTPS test exposed error/close in one transport tick arriving
before the queued request error. Track pending error delivery without installing
a listener that would swallow fatal errors; defer close/response abortion behind
that delivery. Keep one-shot lifetime guards and paused complete body delivery.
Requests remain abortable across upload/response; failed certificate/hostname
validation reaches no HTTP handler on the peer.

Reject ambiguous numeric IP spellings before TLS hostname/SAN selection using the
existing WHATWG URL normalizer. Direct and Host/SNI-derived aliases cannot turn an
IP route into DNS-SAN authentication. Five TLS and three HTTPS authority policies
check strict rejection; this is intentional input hardening, not Node 24 parity.
No Ladybird changes or new third-party dependencies are introduced.

Validation:
- Standalone rebuild and all 53 CTests pass in 46.17 seconds
  (https-client-all-tests.log), including existing HTTP/TCP/TLS/stream/process,
  GC, source/checkout and license gates. New HTTPS comparisons cover 22 encrypted
  workflows: GET/POST/auth/URL, DNS/IPv4/IPv6, Host/SNI/no-ALPN, both TLS versions,
  HEAD/204/trailers, one MiB paused data, signals at startup/socket/live/settled
  phases, GC and failed trust/hostname/expiry. Peer counters prove zero HTTP
  requests for failed verification and early abort.
- Independent Python peers inspect ClientHello before TLS wrapping, test decrypted
  chunked/truncated framing and reject a plaintext HTTP responder. Preserve actual
  Node EPROTO vs Mite ERR_SSL_WRONG_VERSION_NUMBER values separately. Shared class,
  default port/protocol and public-constructor plan rejection are checked. Unhandled
  certificate errors remain fatal, suppress later close callbacks and expose no HTTP.
- Fifteen HTTPS API/provider/authority policies reject explicitly. The three final
  authority checks are verified by a subsequent targeted HTTPS CTest (1/1,
  6.83 seconds, https-authority-tests.log). TLS policy coverage now totals 28.
- Native-only HTTPS diagnostics also pass locally (https-native-only-tests.log).
  Windows preserves independent TLS and HTTPS logs even when its reference step
  fails; new hosted acceptance remains required.
- Source style passes 107 Python, 482 JS/web/config, 133 native and 14 CMake sources
  plus Rust; final policy/test edits pass targeted formatting. Both generators
  reproduce, and runtime/Firefly/upstream integrity plus actionlint pass.
- Firefly compiles with updated hooks/runtime (https-client-firefly-build.log).
  No local GUI/desktop tests are launched. Changed-source desktop acceptance stays
  with hosted CI rather than repeatedly occupying the user's desktop.
- New modern ESM HTTPS example executes against the same independent TLS server
  under actual Node and Mite with identical trusted status/body output. Add API,
  compatibility/example updates and ADR 0080. Site validates 157 pages / 10018
  links including this progress entry.

Next: implement fetch/body/web-stream primitives on the verified HTTP/HTTPS stack,
continue TLS/HTTPS server/Agent/option coverage as justified, and remaining runtime
I/O, filesystem/process/stream cancellation, desktop IPC/services and native
Windows/Linux desktop/distribution maturity. Complete OS trust policy, all provider
error/closure cases and full Node/Electron parity remain unfinished. Overall goal
active; local GUI acceptance stays infrequent and batched.

## Web Stream primitives for incremental bodies (2026-11-06)

Add ReadableStream, WritableStream, TransformStream and the associated reader,
writer, controller and queue classes to Mite globals and node:stream/web. The
13 constructors are shared by global/CJS/ESM access; a rooted startup snapshot
preserves module identity after application global replacement. Bootstrap now
returns separate private cancellation and stream objects.

Reuse pinned MIT-licensed web-streams-polyfill 4.5.1 on LibJS, with lock integrity,
generated source metadata and full distributed notices. No runtime Node/V8/LibWeb
or Ladybird edits. Native ArrayBuffer.transfer supplies actual buffer detachment
for byte enqueue/BYOB reads rather than an ownership approximation.

Validation:
- All 54 standalone CTests pass in 50.68 seconds (web-streams-all-tests.log),
  including existing TCP/HTTP/TLS/HTTPS, filesystem/process, native GC/embedding,
  generated source/checkout and notice distribution/tamper gates.
- New non-GUI differential suite matches actual Node 24.15.0 for 15 workflows:
  constructor/global descriptors, readable locks/pressure/close/errors, iterator
  cleanup/from, serial sink writes and writer.ready, writable abort reason/signal,
  transform flush/error propagation, both pipe error directions, signal and
  prevention behavior, native BYOB buffer detachment/offset/EOF, byte tee isolation
  and aggregate cancellation reasons. A terminal marker rejects incomplete runs.
  Independent CommonJS fixture checks cache identity after replacing a global
  before the module's first lookup.
- Firefly compiles with the same embedded runtime (web-streams-firefly-build.log).
  Existing linker warnings about locally built OpenSSL objects' deployment targets
  persist; this compile does not validate deployment on older macOS versions.
  No local GUI apps/tests are launched.
- Source style passes 108 Python, 483 JS/web/config, 133 native and 14 CMake
  sources plus Rust; both generators reproduce. Runtime/Firefly/upstream integrity
  and actionlint pass. Site validates 159 pages / 10140 links, including the detailed
  Web Streams reference and ADR 0081 (before adding this progress entry).
- Hosted acceptance of this new slice remains pending. At inspection, the previous
  daea064 style/Pages runs pass; Linux/Windows remain running and optimized macOS
  42218926490 is pending behind an older build. Preserve running jobs. This is not
  a new release or verified native desktop platform expansion.

Limits: public-event cancellation follows the portable package rather than native
consumers' private abort hooks. Full WPT, every BYOB/termination edge, encoding/
compression streams, Node-stream toWeb/fromWeb adapters and transferable streams
are unverified or absent. Fetch/Request/Response/Headers are still absent. API docs
and compatibility distinguish the tested subset from those remaining requirements.

Next: implement owned Node/Web Stream bridges and incremental fetch bodies over the
verified HTTP/HTTPS clients, then remaining networking/cancellation and desktop
services/platform/distribution work. Keep local GUI acceptance infrequent/batched.
Overall goal remains active.

## Owned Node/Web stream bridges (2026-11-06)

Replace the portable stream package's empty Web-adapter entry points with authored,
commented ESM in runtime/bootstrap/streams. Readable/Writable/Duplex fromWeb/toWeb
now bridge incremental chunks, pressure/drain, completion/errors and resource
cancellation. The native module loader supplies original Web constructors and
private AbortSignal hooks; stopped public abort propagation does not suppress
fromWeb cancellation. No upstream or dependency patches/dependencies are added.

Readable conversion honors byte/object mode and incremental decoding. Web-facing
Buffer chunks are independent Uint8Array snapshots. fromWeb holds exclusive reader/
writer locks after completion, as Node does. Duplex reuses the same endpoint state
machine. Web cancel/abort destroys native resources; Node destroy observes both
Web endpoint shutdowns. Remove data/drain listeners at termination, with a late
legacy-error guard. Validate constructor options before acquiring Web locks.

Validation:
- All 55 standalone CTests pass in 47.07 seconds (web-adapters-all-tests.log),
  including prior runtime, GC/embedding, TLS/HTTPS, source/checkout and license gates.
- New actual Node 24.15.0/Mite comparisons cover 17 bridge workflows: byte/text/
  object identity, pipeline, demand/pressure, Web errors before consumption, writer
  ordering/close/abort, accepted-write vs callback timing, drain, live/pre-aborted
  signals (with stopped public propagation), native errors/premature close, closed
  destinations, Duplex transformations/roundtrips and validation/atomic pair locks.
- Explicit separate policy fixtures preserve measured Node vs Mite differences:
  invalid highWaterMark strands Node's reader lock but not Mite's; Node retains one
  data/drain listener, Mite removes its own. Do not claim internal cleanup parity.
- Independent Python HTTP peers prove first-block delivery before allowing the
  remainder of a one-MiB body; validate all 256 KiB uploaded via
  Writable.toWeb(ClientRequest); and require peer-observed connection failure on
  cancellation before even eight MiB of the advertised 64-MiB remainder can pass.
  A final targeted CTest passes 1/1 in 0.52 seconds after tightening the peer bound
  and adding the runnable modern ESM example (web-adapters-final-tests.log).
- Example examples/mite-stream/web-bridge.mjs runs under actual Node/Mite with
  identical output. Add detailed stream API, compatibility/tutorial links and ADR
  0082; distinguish codecs, mixed Web pipelines, fetch and other unfinished work.
- Firefly compiles (web-adapters-firefly-build.log); existing local OpenSSL object
  deployment-target warnings persist, so older macOS deployment is not established.
  No local GUI apps/tests are launched.
- Both generators reproduce and runtime/Firefly/upstream integrity plus actionlint
  pass. Final source style passes 109 Python, 488 JS/web/config, 133 native and
  14 CMake sources plus Rust. Site validates 161 pages / 10268 links before final
  example/progress links; final site validation is run before committing.
- Previous primitive commit a5441d9 is now hosted Linux green: run 42220126710,
  job 127899789104 passes all 54 suites in 67.78 seconds, including mite-web-streams.
  Its style/Pages also pass; Windows is running and optimized macOS 42220159829
  remains pending. Preserve running jobs. This bridge commit needs fresh hosted
  acceptance; it is not a new alpha publication.

Limits: native shutdown Promise timing is not identical to Node (Mite awaits
terminal closure); forever-blocked custom writes/destroy callbacks can hold it
pending. Arbitrary legacy/mixed consumers, full Web pipeline/finished helpers,
all TLS bridge/half-close/GC races, filesystem streams and transferable streams
need further work. Fetch/Request/Response/Headers remain absent. Keep the larger
specification active; proceed with incremental body/fetch objects on these
verified transport boundaries and remaining native desktop/platform services.

## Fetch objects and native incremental HTTP(S) (2026-11-06)

Install fetch, Headers, Request and Response globals through an owned ESM factory
split into headers/body/objects/transport modules under bootstrap/fetch. Native
loader hooks supply original Web constructors, private signal and disturbance
state, URL/encoding/Buffer and lazy HTTP/HTTPS/stream resolution. Standalone Mite
still embeds LibJS alone: no Node/V8/LibWeb, new npm dependency or upstream edits.

Headers preserve normalized lists, Cookie/Set-Cookie behavior and immutable
network/static-response guards. Body inputs snapshot bytes/text/URLSearchParams;
stream bodies remain incremental. Consumption, direct reads, cancellation,
tee-clone and Request ownership transfer observe actual disturbance. Network
responses support BYOB, including the explicit zero-byte EOF response needed to
settle pending BYOB reads. Convenience methods text/json/arrayBuffer/bytes collect
only when requested. Blob/FormData consumers reject explicitly.

Fetch uses existing verified HTTP/HTTPS transport, one connection per request,
byte-counted response pressure, streamed uploads, HTTP failure responses,
redirect follow/manual/error with a 20-hop limit, method/body rewrites, replay of
snapshotted bodies and cross-origin credential stripping. Private signals cancel
upload/native transport/response ownership; pre-aborted POST also cancels its
producer without an HTTP request. Completed early responses cancel unfinished
uploads. Reject compressed responses rather than misrepresent encoded bytes as
text; default Accept-Encoding is identity. Custom agents/dispatchers, unavailable
browser/cache/referrer/integrity modes and URL/port violations fail explicitly.

Validation:
- All 56 standalone CTests pass in 52.30 seconds (fetch-all-tests.log), including
  native TLS/HTTPS, GC/embedding, streams/process/filesystem and source/license/
  checkout gates. This follows the additional pre-aborted-upload ownership fix.
- New actual Node 24.15.0/Mite differential suite covers 12 object and 13 independent
  network workflows: header lists/guards, body snapshots/decoding/JSON, statuses,
  clone/direct read/cancel/transfer, signal followers, URL/method metadata,
  one-MiB incremental BYOB response behind a server barrier, 256-KiB streamed
  upload, all redirect modes/replay/credentials, truncation/limits, pre/header/body
  abort and peer-observed cancellation before eight MiB of a 64-MiB body can pass.
  Terminal markers reject silently unsettled programs. Public ESM client example
  also runs unchanged under both runtimes.
- Three explicit Node differences remain visible: Node decompresses gzip, Mite
  rejects unavailable compression; Node can stay pending when a public source
  abort listener stops propagation, Mite's private subscriptions cancel; Node can
  defer early-response producer cancellation behind a pending pull, Mite cancels
  immediately. Eleven unavailable API/options and three URL guards reject explicitly.
- Independent TLS peer observes ClientHello and zero HTTP application data on
  DEPTH_ZERO_SELF_SIGNED_CERT refusal under Node and Mite. Manual system-trust
  https://example.com probe returns 200. A final authority test ignores caller Host
  headers on the wire, matching observed Node behavior, so SNI/verification derive
  from URL. With Host deliberately wrong, Mite still returns URL-authorized HTTPS
  200 / 577 bytes (fetch-public-https.log). This external probe is not a hosted gate.
- After that final Host correction, targeted Fetch/runtime-artifact/portable-checkout
  CTests pass 3/3 in 2.19 seconds (fetch-final-tests.log). Independent HTTP peer
  explicitly checks URL-derived Host. Other existing runtime behavior is unchanged.
- Firefly compiles with final embedded sources (fetch-firefly-build.log). Existing
  local OpenSSL deployment-target warnings persist; this does not establish older
  macOS deployment. No local GUI apps/tests are launched.
- Both generators reproduce; runtime/Firefly/upstream integrity and actionlint pass.
  Final style passes 110 Python, 499 JS/web/config, 133 native and 14 CMake sources
  plus Rust. Site validates 163 pages / 10410 links before this progress entry;
  final site validation is run before commit. Add detailed Fetch reference, ESM
  sample, updated current API/compatibility/troubleshooting and ADR 0083.
- Previous dc3308b stream/HTTPS baseline now passes all 55 hosted suites on Linux
  x64 (run 42222081557, job 127906340591, 59.29 seconds) and native Windows x64
  (run 42222081782, job 127906398350, 99.26 seconds), including Web bridges. Its
  style/Pages also pass. Update those baseline docs without claiming this newer
  Fetch slice already has hosted acceptance. Optimized macOS 42222097652 remains
  pending; preserve running builds. No new alpha is published by this commit.

Remaining: decompression, Blob/File/FormData, trust/proxy/pooling configuration,
cache/referrer/integrity/browser security policy, exhaustive WebIDL/WPT and
abort/redirect/GC/half-close coverage. Native Fetch hosted acceptance is still
required. Continue toward practical runtime/npm coverage and the remaining desktop
IPC/services, native Windows/Linux backend/distribution/security requirements;
the full product specification is not complete. Overall goal stays active and
local GUI acceptance remains infrequent/batched.

## Explicit runtime identity in Fetch acceptance (2026-11-06)

Hosted Windows run 42226281625 / job 127920555770 reaches and passes the shared
Fetch object/HTTP workflows, then fails in the test harness: Path(binary).name is
node.exe, not node, so a Node reference request using gzip is incorrectly checked
against Mite's identity-only compression policy. Fifty-five other suites pass;
this is not evidence of a native Fetch implementation failure or full Windows
acceptance. The following TLS/policy checks had not yet run in that failed gate.

Replace executable-basename inference with an explicit native-runtime argument in
the diagnostic comparison. Keep all normal output and protocol assertions intact.
A real Node invocation through build/reference-node.exe now passes the Fetch gate
locally alongside Mite (fetch-windows-name-regression.log), exercising the filename
case independently of the host OS. Validation used the current working runtime;
Blob/File/FormData work remains separate and uncommitted. This commit retains the
previous revision's 11 unsupported-policy assertions; their upcoming data-API
changes are staged with that feature instead.

The previous Fetch commit's Linux/style/Pages are green. Its optimized macOS
run 42226301275 / job 127927667217 is authoritatively in progress in Release
compile/test/package; preserve it. Fresh hosted Windows acceptance is still
required after this harness correction. No local GUI tests are launched.

## Immutable Blob/File and multipart FormData (2026-11-06)

Add Blob/File/FormData globals and original Blob/File aliases in node:buffer for
CJS/ESM. Reuse pinned fetch-blob 3.2.0 immutable bytes with authored ESM adapters
for MIME/scalar normalization, native endings, File metadata, modern bytes and
bounded captured byte streams. Internal provider slices use that same BYOB/EOF/
cancel adapter. Object APIs retain original stream/codec behavior after global
replacement. FormData owns ordered scalar-string/File entries and validated
filename overloads; it requires no DOM or filesystem privileges.

Fetch extracts Blob/File or snapshots FormData into immutable multipart Blob parts.
Native crypto provides boundaries; encode names/filenames safely, normalize text
line breaks, compute exact byte lengths and replay the same body/boundary without
whole-file concatenation. Request/Response blob/formData consumers now work;
unsupported Fetch-policy count drops from 11 to seven. Foreign provider bodies
still reject explicitly. Lazy pinned busboy 1.6.0 / streamsearch 1.1.0 / WHATWG MIME
4.0.0 supply multipart/MIME decoding with preserved order/UTF-8 paths, base64 Files,
URL-encoded semantics, no silent field/file truncation and explicit parse failures.
Four strict Buffer codec slice helpers satisfy real parser dependencies.

Preserve package sources and complete MIT wording/integrities. The owned bundle
resolver replaces fetch-blob's optional old Node-stream shim rather than patching
the package. Assert only four intended packages ship: no second stream polyfill or
node-domexception fallback. A first complete run exposed CRLF license bytes that
Git's LF checkout changed; canonicalize license line endings at generation while
retaining all wording, regenerate/stage and rerun the full source/runtime gates.
This introduces no Node/V8/LibWeb runtime, native dependency or Ladybird edit.

Validation:
- All 57 CTests pass in 51.78 seconds (fetch-data-all-tests.log), including native
  TLS/HTTPS/Fetch, GC/embedding, Buffer/fs/process/stream, reproduced checkouts and
  notice distribution/tamper gates. The prior failed run's sole failure was the
  newly detected license line-ending mismatch, now fixed and verified with zero
  CR bytes in generated data code/notices.
- New actual Node 24.15/Mite suite covers 13 object workflows: global/node:buffer
  identities/public getters, byte/view snapshots and owned outputs, BOM/scalars/
  native endings, slices, repeatable BYOB/EOF/cancel streams, File metadata,
  ordered FormData/filename overloads, body clone/Blob MIME, URL form decoding,
  multipart snapshots/File order/base64/malformed errors and captured constructors.
- Forty-eight additional actual Node comparisons cover parser Buffer codec slices
  and coercion/fractional/NaN/default/out-of-range bounds.
- Five independent HTTP/MIME workflows inspect exact Blob/File/empty lengths,
  262145-byte file/hash and field snapshots, Blob replay, 1.1-million-byte fields
  without truncation, base64/UTF-8-path Files, incoming Blob/URL forms and the modern
  multipart sample. Python's independent MIME parser validates the wire.
- Separate 307 diagnostic preserves a measured Node reference defect: it changes
  multipart body boundary while keeping the original header. The independent peer
  rejects that body; Mite replays parseable byte-identical body/header and the same
  decoded entries. Do not normalize this into claimed Node parity.
- Modern ESM examples/mite-fetch/multipart.mjs executes under both runtimes against
  the same independent MIME peer. Update detailed Fetch/data references, API/
  compatibility/example links and ADR 0084; no complete File/WebIDL/WPT claim.
- Final Firefly compile passes (fetch-data-firefly-build.log); existing local
  OpenSSL deployment-target warnings persist. No local GUI apps/tests are launched.
- Both generators reproduce; runtime/Firefly/upstream integrity and actionlint
  pass. Final source style passes 111 Python, 510 JS/web/config, 133 native and
  14 CMake sources plus Rust. Site validates 165 pages / 10539 links before this
  entry; final site validation is run before committing.
- Fetch 5c5d64c Linux/style/Pages pass; Windows's sole failure was the executable-name
  harness bug fixed and pushed separately as 5ae0839 above. That corrective run's
  native Linux/Windows are still in progress, with style/Pages green. This newer
  data slice still needs fresh hosted acceptance. Optimized macOS 42226301275 is
  in progress on 5c5d64c; preserve it. No new alpha publication in this step.

Remaining: decompression, native trust/proxy/pooling configuration, complete data
coercion/charset/disposition/transfer-encoding/GC and File/WebIDL/WPT coverage,
filesystem-backed files, Blob URL registry/IPC transfer, further Fetch policies
and broader runtime/npm APIs. Continue the remaining desktop IPC/services and
native Windows/Linux backend/distribution/security work as well. Overall goal
remains active; keep local GUI acceptance infrequent and batched.

## Bounded native decompression and Fetch content codings (2026-11-06)

Add an owned native zlib inflater under platform/compression with an ESM
DecompressionStream coordinator. Pin standalone zlib 1.3.2#2 to the exact version
already used by the renderer. Reuse the provider without modifying Ladybird/zlib,
embedding Node/V8/LibWeb or adding a second event loop. Native package notices
include the installed zlib copyright through existing distribution tooling.

Each native step owns its input snapshot (at most one MiB), returns at most 64 KiB,
reports consumed bytes and clears zlib pointers before JS allocation. RAII/shared
ownership and explicit roots protect factory, context and output lifetime. The
readable pulls one output block at a time rather than eagerly enqueueing an entire
expansion. Writable completion tracks consumed input. Handle checksums/truncation,
concatenated gzip, raw/zlib headers, BYOB EOF, explicit abort reasons and cleanup.
Do not externally error the writable controller while its own abort signal is
being dispatched: a new shared abort test caught and fixed that Promise ordering.

Expose DecompressionStream globally and through node:stream/web for gzip, deflate
and deflate-raw. Private HTTP modes add raw-deflate sniffing and gzip zero padding.
Fetch now advertises gzip/deflate, reverses coding stacks, preserves original wire
Content-Encoding/Length and decodes incrementally. Null bodies and explicitly empty
framing are handled without a decoder. Unsupported algorithms reject explicitly;
compression encoders, Brotli/zstd and node:zlib remain unavailable.

Validation:
- All 58 CTests pass in 57.86 seconds (compression-all-tests.log), including the
  prior runtime/data/crypto/network, forced-GC/embedding, checkout and license gates.
- Eight actual Node 24.15/Mite decoder workflows cover shared identity/format
  rejection, one-byte fragmented headers, all three formats and one-MiB expansion,
  concatenated members, checksum/truncation, Web trailing-padding policy and
  writer abort. Five native-only invalid BufferSource/private-format guards also
  fail explicitly. Separate exact outputs retain Node's early write acceptance
  versus Mite's stronger demand/pressure and pending-write cancellation.
- Four independent HTTP workflows prove gzip/zlib/raw fallback, stacked/concatenated
  codings and HTTP padding, encoded headers vs decoded lengths, first flushed bytes
  before peer release, checksum/empty framing, cancel/abort reason and peer-observed
  closure. An unavailable HTTP coding also rejects explicitly. A strict gzip
  trailer fixture preserves Node's successful truncated-body decode and Mite's
  Z_BUF_ERROR rejection separately rather than claiming equal integrity policy.
- Real collections between inflate steps pass under actual Node --expose-gc and
  the non-GUI embedded host. The modern ESM file example runs under both runtimes
  with identical decoded byte count. Existing Fetch differential gate also passes
  with shared gzip decoding and its remaining explicit abort/upload differences.
- Final Firefly compile succeeds (compression-firefly-build.log), reusing the same
  renderer zlib. Existing local OpenSSL deployment-target warnings persist; older
  macOS deployment is not established. No local GUI apps/tests are launched.
- Both generators reproduce; runtime/Firefly/upstream integrity and actionlint
  pass. Final style passes 112 Python, 519 JS/web/config, 135 native and 14 CMake
  sources plus Rust. Site validates 167 pages / 10665 links before this entry;
  final site validation runs before commit. Add detailed decoder API, modern
  example and ADR 0085; update Fetch/data/compatibility references honestly.
- Previous Fetch/data 20555f6 is now hosted green on Linux x64 (run 42231739039,
  job 127939452215, all 57 suites in 60.56 seconds) and native Windows x64
  (run 42231739218, job 127939718690, all 57 in 89.78 seconds), including the new
  data/MIME suite. The executable-name correction 5ae0839 Windows gate also passes.
  Update scoped baseline docs. This newer native zlib/decoder slice still needs
  fresh hosted acceptance. Optimized macOS 42226301275 remains authoritatively
  in progress on 5c5d64c; preserve it. No new alpha publication in this commit.

Limits: VM-thread synchronous steps, stronger pressure and strict HTTP trailers
are deliberate differences; no worker-pool or total decoded-memory quota is
claimed. Complete format/dictionary/coercion/flush/provider/race/WPT coverage,
encoders, additional codecs, TLS trust/proxy/pooling configuration and further
Fetch/browser policies remain open. Continue broader runtime/npm and desktop
IPC/services, native Windows/Linux backend/distribution/security requirements.
Overall specification remains incomplete and goal active. Keep local GUI
acceptance infrequent/batched.

## 2026-11-06: Native macOS screen snapshots and windowless policy acceptance

- Add an owned, commented ESM screen policy module and separate native screen
  folder. Public Firefly ESM/CommonJS exports include the screen EventEmitter;
  standalone Mite remains independent of Firefly/Qt/LibWeb. Ladybird remains
  unmodified at 97ad425b5125f20c516e1a8d3b5bb02f391fc975.
- AppKit/CoreGraphics provide actual display identity, bounds/work area in
  primary-top-left DIP, backing scale, rotation, internal/label/frequency and
  cursor position. No fabricated color/HDR metadata; unknown hardware capability
  fields stay unknown. Unsupported pixel conversions fail explicitly.
- Implement readiness, cloned snapshots, nearest/largest-intersection policies,
  explicit no-display failures and queued display-added/removed/metrics events.
  Qt notifications share the Bridge context/VM queue and teardown ownership.
  Snapshot baselines commit before callbacks; unchanged observations coalesce.
- A QCoreApplication-only probe validates actual provider records against
  independent CoreGraphics primary/bounds data and verifies work areas/scales.
  It creates no QApplication, window, renderer or cursor change. Actual Node and
  Mite API-factory tests pass with this OS data and synthetic negative-origin
  layouts, intersections/ties, validation, mutation, event reentry and empty
  desktop fixtures. New CTest runs policy comparisons without GUI on all hosts.
- Firefly and screen probe compile successfully (screen-build.log); existing
  local OpenSSL deployment-target warnings persist. All 59 standalone CTests
  pass in 19.51 seconds with three parallel workers (screen-ctest.log).
  No local GUI application or desktop suite was launched for this slice.
- Add a modern ESM display example, detailed screen API reference and ADR 0086.
  Add a shared actual Electron/Firefly export/cursor/geometry fixture to the
  existing batched desktop suite and native probe to check-firefly. These public
  desktop checks are pending hosted execution; compilation and factory/provider
  acceptance alone do not establish public desktop parity or hotplug behavior.
- Source style passes 113 Python, 523 JS/web/config, 139 native and 14 CMake
  sources plus Rust; both generators, runtime/Firefly/upstream integrity and
  actionlint pass. Site validation passes 169 pages / 10957 links before this
  entry; regenerate/check after final progress update before commit.
- Previous compression 84c3b2b is now hosted green: Linux x64 run 42236253791,
  job 127954519545 (all 58 suites, 72.49 seconds); Windows x64 run 42236253891,
  job 127954604439 (all 58 suites, 78.85 seconds). Update scoped API baseline.
  Optimized macOS 42226301275 on 5c5d64c remains authoritatively in progress;
  preserve it. No new alpha publication is claimed.

Limits: actual mixed-DPI/mirroring/rotation, physical hotplug and full notification
coverage need native hardware acceptance. Further Display fields/conversions and
Windows/Linux desktop integration remain open. Continue broader runtime/npm,
desktop services/IPC, distribution/security and specification work. Goal remains
active. Keep local UI acceptance occasional and batched, per user clarification.

## 2026-11-06: Native asynchronous clipboard and ClipboardItem text slice

- Add commented ESM clipboard/ClipboardItem policy and isolated Objective-C++
  native clipboard folder. Follow the pinned Electron 45.2.3 async MIME-item API,
  verified against its versioned public documentation rather than implementing
  the older synchronous writeHTML/readRTF/write-object API as current behavior.
- Public Firefly ESM/CommonJS exports include clipboard and ClipboardItem;
  standalone Mite remains independent of GUI/Firefly/LibWeb. Async readText,
  writeText, read, write and has support text/plain, text/html and text/rtf.
  Clear remains synchronous. Snapshot item keys/bytes, resolve string/Blob/promise
  data before publication, retain rejection reasons and reject unsupported
  formats, multi-item writes and combined data beyond eight MiB explicitly.
- AppKit stores actual native bytes. Normalize/deduplicate its automatically
  generated external UTF-16 plain-text alias; accept external UTF-16-only writers.
  Multi-format reads check generation changes. Complete preparation precedes
  ownership change; native publication errors report failure without a rollback
  promise. OS provider work remains on the application thread.
- QCoreApplication-only probe uses a unique private pasteboard and releases it;
  never accesses the developer's general clipboard or creates a GUI application.
  Native acceptance covers independent AppKit reads of Unicode/NUL, HTML/RTF
  bytes, MIME metadata, external UTF-16, unsupported/duplicate/malformed/oversized
  writes without ownership changes, unknown-format read rejection and clear.
- Actual Node/Mite API factory tests pass for async operations, item identity,
  MIME/Blob bytes, snapshots, deferred/rejected payloads, complete preparation,
  validation/budgets, native failure propagation and empty/clear behavior.
  Both Firefly and native probe compile (clipboard-build.log). All 60 CTests pass
  in 19.08 seconds with three workers (clipboard-ctest.log).
- Add a shared public Electron/Firefly fixture to batched desktop CI. It modifies
  the general clipboard only with explicit CI opt-in; local suites report that
  fixture as not run. Public desktop parity is pending hosted execution. No local
  GUI/desktop suite was launched. Add modern example, detailed clipboard API and
  ADR 0087, compatibility row and website navigation.
- Source style passes 114 Python, 527 JS/web/config, 143 native and 14 CMake
  sources plus Rust; manually run the same pinned native formatter on new .mm
  files (the selector currently omits that suffix). Both generators, runtime/
  Firefly/upstream integrity, actionlint and 171-page/11256-link site gate pass
  before this entry; regenerate/check final progress links before commit.
- Release observation changed authoritatively: older optimized macOS 42226301275
  (5c5d64c) job 127927667217 completed with failure at 15:05:03 UTC. Compilation,
  existing lifecycle/preload/module checks passed, but public IPC security-main
  failed with FAIL:Firefly bridge promise rejected. Package/upload was not reached.
  Inspect/fix that exact failure next; no new alpha or successful package claimed.

Limits: images/files/custom/native formats, bookmarks/find, multi-item writes,
Windows/Linux desktop, full class/coercion/provider/race behavior and broader
cross-app acceptance remain unfinished. Preserve unmodified Ladybird pin and
Mite independence. Continue release blocker plus broader specification work;
goal active, local UI acceptance occasional and batched.

## 2026-11-06: Preserve actionable IPC and HTTPS CI failure diagnostics

- Older optimized macOS 42226301275 failed public IPC security acceptance, with
  the deliberately sanitized contextBridge error hiding the actual assertion.
  Keep production exception sanitization unchanged. Fixture-owned security/fresh
  actions now return bounded plain failure records with phase and assertion
  message; renderer fixtures require the exact true result before readiness.
  Failed assertions remain failures. No retries or weakened expected results.
- Run one targeted local security-main acceptance, using hidden test windows,
  after the previous windowless build/policy gates. It passes all serialization,
  zero-trap, raw-capability denial, 16-slot, three-navigation/same-document and
  destruction assertions with empty stderr. The old hosted failure is not
  reproduced or claimed fixed. New diagnostic CI must identify its source.
  No complete local desktop suite or repeated UI loops are run.
- Screen da00ed4 Windows run 42239236689/job 127964907545 fails one of 59 suites:
  HTTPS's first positive workflow reports zero peer requests, before printing
  actual HTTPS:get. The failure lacks client/runtime context, so its cause is
  unproven. Its independent native-only follow-up passes all 22 HTTPS workflows;
  the screen policy also passes. Preserve this distinction, not a green full gate.
- Positive HTTPS fixtures now throw unexpected TLS/socket errors instead of
  quietly exiting with no response/output. Check client status before peer
  counters and include mode, executable and complete client result in assertions.
  Existing expected abort/certificate rejections retain their exact behavior.
  Full targeted HTTPS differential suite passes locally against actual Node/Mite
  plus independent peers, fatal certificate and embedded-host GC checks.
- Source style, generated Firefly integrity, unmodified upstream and whitespace
  checks pass before commit. Broader unchanged sources retain the clipboard
  commit's 60/60 result; no duplicate full-suite claim. macOS da00ed4 remains live
  while newer commits queue; do not interrupt it. No new package/alpha claimed.

Continue exact hosted IPC/HTTPS diagnostics, release acceptance and remaining
specification. Goal active. Local GUI tests remain occasional, targeted/batched.

## 2026-11-06: Native appearance policy and real renderer color preferences

- Add commented ESM nativeTheme EventEmitter and separated native theme sources.
  Public Firefly ESM/CommonJS share system/light/dark selection and read-only
  effective dark/integrated-UI, native contrast/invert/reduced-transparency and
  differentiate-without-color properties. No Mite GUI dependency or renderer
  native capability is added; Ladybird remains unmodified at the synchronized pin.
- Reuse public upstream application interface/website appearance settings and Qt
  SystemAppearance controls. Actual renderer color-preference IPC reaches existing
  views and new views inherit the choice; no DOM class or stylesheet substitution.
  The temporary application profile stores changes, never global macOS defaults.
- Queue actual system/accessibility notifications through the Bridge owner;
  detach NSWorkspace observers at QObject destruction and guard queued blocks
  with QPointer. Compare/commit JS baselines before reentrant listeners, coalesce
  unchanged observations, reject invalid sources and keep properties getter-only.
- Windowless native probe reads actual OS flags and verifies notification
  registration/removal with an in-process notification, without changing an OS
  setting. The actual API factory passes in Node/Mite with both synthetic and
  native records: source selection, fresh system/accessibility flags, queued/
  coalesced updates, reentry, invalid/read-only values and native setter failure.
- Firefly and native probe compile (theme-build.log). All 61 CTests pass in
  19.30 seconds with three workers (theme-ctest.log). Source style passes 115
  Python, 532 JS/web/config, 146 native and 14 CMake sources plus Rust; separately
  check both new .mm files with the same pinned formatter. Both generators,
  runtime/Firefly/upstream integrity, actionlint and whitespace checks pass.
- Targeted Firefly native acceptance passes explicit override/current CSS queries
  through system/light/dark transitions in existing and newly created hidden
  windows, with empty stderr. Event-only hidden-view acceptance initially missed
  a light transition; fresh matchMedia queries independently prove preference
  delivery. Hidden rendering-driven media change-event timing remains unverified,
  not reported as passed. No full local desktop suite is launched.
- Actual pinned Electron 45.2.3 returns the same complete theme/CSS fixture result
  after correcting the shared ESM harness. Top-level-await readiness caused a
  40-second Electron startup wait; finish module evaluation and schedule assertions
  in whenReady().then instead. Stage static desktop imports, retaining the exact
  assertion results. Apply the same bootstrap correction to screen/clipboard
  fixtures, whose new callback forms still need hosted public acceptance.
- Add detailed nativeTheme API and ADR 0088; connect site navigation and update
  the Dark mode example/catalog to partial. Button/visible-chrome acceptance
  remains manual (smoke stays false); do not promote it from direct query evidence.
  Site validates 173 pages / 11564 links before this entry; check final progress
  links before commit.
- Clipboard bcba2f8 Windows run 42241691687 is now green, including native job
  127973834173 and both screen/clipboard factory suites. Earlier da00ed4's lone
  HTTPS failure is retained as a separate observation, not erased by the later pass.
  macOS da00ed4 optimized run 42239251129 remains live and is not interrupted.
  No new release package or alpha publication is claimed.

Limits: physical system/accessibility changes, visible native chrome, exact
Electron event/coercion behavior, hidden media-event timing, accessibility
rendering and Windows/Linux desktop remain open. Continue pending release/IPC
acceptance and broader specification. Goal active; local GUI tests occasional
and targeted/batched, windowless/hosted checks preferred.

## 2026-11-06: Include Objective-C adapters in automatic source formatting

- Close the native formatter's selector gap: .m/.mm now join .c/.cpp/.h in the
  existing pinned clang-format/definition-spacing gate. This automatically covers
  all five maintained macOS Objective-C++ files, instead of relying on manual
  formatting for new adapters. Generated/vendor/build files stay excluded.
- Apply the gate to existing sources; only ScreenMac.mm needs whitespace/layout
  normalization. No runtime behavior or upstream source changes. Document the
  covered suffixes in the source-style tooling guide. The same existing style CI
  command now checks 151 native files rather than 146.
- Full source style, upstream/Firefly integrity and whitespace gates pass before
  commit. Native behavior retains the preceding 61-suite acceptance; this
  formatting-only step does not repeat a desktop suite or claim fresh native
  behavior coverage. Regenerate/check site after this progress entry.

Continue queued release/IPC validation and remaining runtime, desktop/platform,
distribution/security requirements. Goal remains active.

## 2026-11-06: Native BrowserWindow outer/content geometry and positioning

- Extract commented geometry policy into core/window and checked native frame
  helpers into native/window. Preserve BrowserWindow/EventEmitter ownership and
  destroyed guards. Add x/y and useContentSize constructor options; get/setBounds,
  get/setContentBounds, get/setPosition, get/setContentSize and native center.
  Reconcile existing get/setSize with true outer dimensions instead of Qt client
  dimensions. Expose queued native move notifications without renderer privilege.
- Query actual QWindow frame margins and use primary-top-left DIP coordinates.
  Preserve omitted outer rectangle fields, require complete content rectangles,
  and reject unknown fields/animation and invalid/overflowed dimensions. Native
  conversion uses checked 64-bit arithmetic. Tiny outer frames retain one client
  DIP; unspecified constructor coordinates preserve native placement behavior.
- A targeted native test exposed Qt's deferred hidden QWidget resize: immediate
  native queries still included a 32-DIP excess frame. Synchronize QWidget and
  QWindow sizes, rather than guessing a title-bar constant. Native QWindow x/y
  signals also cover hidden moves, unlike deferred QWidget move events.
- Actual Electron acceptance establishes complete setContentBounds rectangles,
  versus partial setBounds. It also disproves a mathematical midpoint assertion
  for center: pinned Electron delegates to AppKit NSWindow.center. Use that same
  native operation, retaining macOS's higher vertical placement. Compare center
  coordinates against the actual reference, not a synthetic work-area midpoint.
- Final shared actual Firefly/Electron fixture passes with empty stderr:
  decorated construction/outer bounds, content margins/size, partial outer/full
  content setters, native positions, content-sized and
  frameless construction, queued move and matching center [564,150] relative to
  the tested work area. Windows remain hidden; load test documents before closing
  so helper startup is complete. Compare rectangle values independently of JSON
  key order. No full desktop suite is run locally.
- The windowless native geometry probe covers checked decorated/frameless frame
  translation, negative origins, minima and integer limits. Actual Node/Mite API
  factory tests cover synthetic frame snapshots, constructor flags, ownership,
  partial updates, content-size conversion, native center delegation, move events,
  validation, animation and destroyed guards. These are distinguished from native
  window acceptance. Add both policy CTest and hosted desktop fixture.
- Firefly and geometry probe build (window-geometry-build.log); all final 62
  CTests pass in 18.67 seconds with three workers (window-geometry-ctest.log).
  Source style passes 116 Python, 535 JS/web/config, 156 native including .mm,
  and 14 CMake sources plus Rust. Both generators, runtime/Firefly/upstream
  integrity, actionlint and whitespace gates pass. Add detailed geometry API
  contract/compatibility and ADR 0089; site validates 174 pages / 11632 links
  before this entry, then regenerate/check final progress links before commit.
- Mite remains independent of Qt/Firefly/LibWeb; Ladybird unmodified at the pin.
  macOS da00ed4 optimized run 42239251129/job 127966455150 remains live on the
  release compilation/test step; preserve it. New optimized master builds queue
  behind it. No new package/alpha publication is claimed in this slice.

Limits: OS constraints, exact minimum/coercion/event parity, visible resize/drag,
maximized/fullscreen restore/normal bounds, physical mixed-DPI/multi-display and
Windows/Linux desktop acceptance remain open. Continue hosted IPC/release and
broader runtime, platform/distribution/security requirements. Goal active; keep
local UI checks occasional and targeted/batched.

## 2026-11-06: Native process clocks, CPU accounting and real timing npm workflow

- Add owned, commented process-timing ESM beside the process initializer and
  native platform/process/Timing.cpp. Global/CJS/named-ESM process exports share
  hrtime, hrtime.bigint, uptime and cpuUsage. Native samples retain no JS state,
  asynchronous request or event-loop handle; Mite gains no GUI/Firefly/LibWeb
  dependency and Ladybird remains unmodified at the synchronized pin.
- uv_hrtime returns exact second/remainder samples; reconstruct BigInt without
  rounding a u64 nanosecond total through Number. Legacy tuple differences retain
  borrow/coercion semantics. Native steady-clock uptime starts once at runtime
  load, not per realm; document late-loaded embedder lifetime separately from
  earlier host process lifetime. uv_getrusage provides process-wide user/system
  microseconds, validation/differences and native failure metadata.
- Actual Node/Mite native tests cover identities, normalized samples bracketed
  by BigInt samples, timer advancement, fractional uptime, Date.now replacement,
  actual CPU work/counters, unclamped differences and ordinary validation/getter
  contracts. Actual Node --expose-gc and the non-GUI embedded host retain samples
  across collection. Separate labeled owned-policy fixtures cover u64 maximum,
  second borrow, native fractions and failure propagation; no fake OS probe claim.
- Lock actual time-span 5.1.0 and convert-hrtime 5.0.0 with integrity records and
  preserved installed MIT notices. Published ESM source runs unchanged through
  ordinary npm resolution. A counting wrapper delegates all five clock calls to
  the original native hrtime.bigint. All eleven real-package application fixtures
  pass against actual Node/Mite; full upstream package suites remain unmeasured.
- Add modern ESM process-metrics example; actual Node/Mite reports have an
  independently computed Python SHA-256 result and valid timing/CPU records.
  Add detailed process API, ADR 0090, source map, compatibility rows and site
  navigation; correct the stale claim that child_process/streams are absent.
- Standalone Release Mite and Firefly compile (process-timing-build.log and
  process-timing-firefly-build.log). All 63 CTests pass in 20.34 seconds with three
  workers (process-timing-ctest.log). Existing local OpenSSL deployment-target
  warnings persist; no older-macOS support claim. No local GUI app/suite is run.
- Source style passes 117 Python, 542 JS/web/config, 158 native and 14 CMake
  sources plus Rust. Both generators, runtime/Firefly/upstream integrity,
  actionlint and whitespace gates pass. Site validates 176 pages / 11945 links
  before this entry; regenerate/check final progress links before commit.
- Old optimized macOS da00ed4 run 42239251129/job 127966455150 is now terminal
  failure: native compilation and screen probe passed, but Electron screen.mjs
  timed out after 40 seconds during top-level-await ready bootstrap. The shared
  ESM callback/static-import correction already shipped in cb180a9; this older
  source did not contain it. Packaging/upload was not reached. Newer optimized
  run 42248311164/job 128002407659 is authoritatively queued; preserve it, rather
  than restarting the completed old source or claiming a new alpha.

Limits: new native Windows/Linux timing acceptance, physical long-uptime/provider
failure, late-loaded/repeated embedding and full proxy/reentrant/error-message
behavior remain open. Process events, stdio/signals, thread/memory/resource APIs
and perf_hooks still need work. Continue newer hosted desktop/release acceptance
and remaining runtime/platform/distribution/security requirements. Goal active.

## 2026-11-06: Native Linux renderer/helper build and acceptance path

- Add a Linux development layout for the real owned WebContent helper, preserving
  macOS bundle paths. Select upstream RendererSandboxLinux.cpp and its sanitizer
  source; keep Ladybird unmodified. Linux resources use share/Lagom and sibling
  real helper executables. Non-macOS desktop runtime targets still refuse.
- Generate an owned standalone Linux renderer manifest using upstream's exact
  baseline/Qt 6.11.2 overrides, original dependencies and absolute overlays. Add
  Linux Qt features without introducing Mite's I/O graph into this renderer.
  Manifest tests verify preservation and reject stale generated data.
- Factor the existing pinned native Linux environment into a shared action. Add
  a dedicated Linux renderer workflow that builds actual LibWeb/Compositor and
  owned-helper probes, executes DOM/screenshot/process/realm/callback gates and
  retains native diagnostics. No disable-sandbox fallback is used.
- Linux process acceptance requires the actual reported renderer PID, its parent,
  NoNewPrivs=1, Seccomp=2 and more Seccomp_filters than the parent frontend.
  These kernel observations establish activation, not full sandbox containment.
- Native Linux compilation/execution of this new renderer path is pending CI;
  adapter tests are not represented as Linux acceptance. Linux BrowserWindow,
  desktop services and relocatable distribution remain unimplemented. Add ADR
  0091 and explicit build/status documentation and site navigation.
- Local Release CLI suite passes all 64 CTests in 19.18 seconds, including new
  manifest and actual CMake platform-layout tests. Existing macOS Firefly and
  owned-helper targets compile successfully. No local GUI app/suite is launched.
  Both generated-source checks, runtime/Firefly/upstream integrity, source style,
  actionlint and whitespace checks pass. Final site is regenerated before commit.
- Previous 02ed03f hosted native Release acceptance is now verified: Windows run
  42251043181/job 128006815798 passes all 63 suites in 80.09 seconds; Linux run
  42251043146/job 128005890982 passes all 63 in 68.80 seconds. Both include native
  process timing/GC and all eleven locked npm application workflows. Update
  scoped API/package compatibility evidence; full upstream package suites remain
  unmeasured. These results do not establish Firefly desktop support there.
- Optimized macOS 3934e49 run 42248311164 was cancelled before build steps ran;
  02ed03f run 42251097845 was queued at last observation. Preserve queued/live
  builds. No new alpha or optimized package result is claimed in this entry.

Limits: the new hosted Linux renderer gate must compile and execute before any
Linux renderer support claim. Native Linux/Windows desktop integration, hostile
sandbox policy acceptance, broader API compatibility and updated release
publication remain open. Goal active; local UI tests stay occasional and targeted.

## 2026-11-06: Native Linux Qt window build path and painted-capture gate

- Separate curated Qt presentation sources, macOS AppKit probes and full desktop
  runtime wiring into commented CMake modules. Add Linux X11/accessibility/native
  window-container sources; reuse upstream Vulkan view/shader outputs when its
  DMA-BUF path is enabled. Keep the upstream tree untouched. Full non-macOS
  Firefly desktop runtime still refuses explicitly.
- Enable the native Linux window-probe target and add an Xvfb/Openbox CI gate
  using actual xcb Qt windows, LibWeb input/resize/navigation, native capture and
  observed WebContent child termination. This is toolkit-event acceptance, not
  physical-input, Wayland, GPU-driver or public Linux BrowserWindow acceptance.
- Add two page-owned color swatches and inspect channel contrast/brightness in
  the native window image. Exact RGB comparison initially failed on this display's
  color transformation; contrasting bounded signatures reject blank/neutral/
  monochrome captures while tolerating that transformation. Capture PNG alone is
  insufficient. Document the actual scope and remaining platform work.
- The extra platform diagnostic exposed a buffered TextIOWrapper/select race in
  the crash harness. Read descriptor bytes and split buffered lines explicitly;
  multiple lines in a single read no longer hide the actual renderer PID.
- Actual macOS window gate passes input, resize, navigation, close, painted
  screenshot and reported renderer crash. Local Firefly, preload and window
  targets compile after the CMake split (linux-window-adapter-macos-build.log).
  Local UI runs were limited to this targeted gate and its observed fixes; no
  general desktop suite was launched. All 64 Release CLI CTests pass in 19.04
  seconds. Source style (119 Python, 544 JS/web/config, 158 native, 19 CMake),
  actual manifest/CMake layout tests, actionlint, upstream integrity and whitespace
  checks pass. Site validates 178 pages and 12260 links/resources/fragments.
- Hosted 74bce40 Mite Windows/Linux runs 42254474474/42254474551 are successful.
  Native Linux renderer run 42254474478/job 128017893815 reached real compilation
  but failed linking liblagom-mediacodecs: static FFmpeg assembly PC32 relocations
  reference preemptible ff_tx_tab_* data. Linux execution remains unverified;
  fix this in owned build adapters, never by modifying Ladybird or masking errors.
- Both macOS package runs are terminal failures, not live: optimized 02ed03f run
  42251097845/job 128010696465 reached desktop IPC but failed fresh-document sender
  URL validation. Default 74bce40 run 42254474684/job 128036394217 failed the CLI
  child-process suite on shutdown ENOTCONN. Preserve these diagnostics and fix the
  actual races before release; no new alpha/package result is claimed.

Limits: Linux compilation/runtime acceptance is still blocked by the recorded
native link failure. Next resolve this plus both release failures, then continue
Linux desktop services/IPC/packaging and required Windows desktop integration.
Goal active; keep local UI tests infrequent and targeted/batched.

## 2026-11-06: Bind bundled FFmpeg archive internals in the Linux media component

- Address the actual 74bce40 Linux x64 ELF link failure in an owned CMake adapter:
  --exclude-libs hides only the three bundled static FFmpeg codec/util/resample
  archives from LibMediaCodecs exports, allowing its direct PC32 internal-table
  references to bind locally. Keep the public component ABI and separately
  dlopened system provider; do not edit pinned Ladybird/FFmpeg, remove assembly,
  disable media, or suppress linker errors.
- Add a native Linux x64 ELF regression ahead of the large CI build. It requires
  an actual failing unadapted PC32 control, successful adapted shared-library
  link, executed public function returning the table value and readelf evidence
  that private archive symbols are not dynamic exports. This is a link-boundary
  fixture, not an FFmpeg decoder or native Linux renderer acceptance claim.
- Source style, actionlint, whitespace and upstream integrity pass. Existing
  macOS target configuration/build checks the Linux-only adapter remains inert.
  Native ELF fixture and complete Linux graph still require the next hosted run.

Limits: preserve failing Linux CI evidence until the real graph passes. Resolve
macOS child-pipe shutdown and IPC sender-URL failures before optimized release
publication. Goal active; no additional local desktop suite is run in this slice.

## 2026-11-06: Finish disconnected child stdin without losing native write errors

- Reproduce the hosted macOS release failure with an actual Python child closing
  descriptor 0 before acknowledging readiness. Node succeeds; old Mite reports
  shutdown ENOTCONN. Match Node 24.15 Socket finalization in owned readable ESM:
  disconnected immediate shutdown and asynchronous completion finish stdin;
  unrelated immediate failures retain native error metadata.
- The new positive/negative native test exposed another actual failure: writing
  to that closed peer killed the CLI with SIGPIPE (-13). Add an executable-owned
  POSIX signal-policy helper and opt in at Mite CLI/Firefly startup. Mite::run
  preserves embedder signal policy. Windows remains a no-op. Do not suppress
  native write errors: the real closed-peer write must still report EPIPE.
- Native Node/Mite regression passes four disconnected finishes plus preserved
  write EPIPE. Separate clearly labeled owned-policy injection tests cover
  ENOTCONN, async completion/cancellation and immediate EIO propagation. Existing
  full spawn/execFile/one-MiB pressure/exit/close/kill/timeout/inheritance suites
  pass. All 64 Release CLI CTests pass in 20.12 seconds. Mite and Firefly compile;
  both generated builtin provenance records update reproducibly. No GUI test is
  run for this correction. Document API, startup scope and embedder responsibility.
- Native Linux ELF regression in bd1a39b run 42293973172/job 128154307621 has now
  actually passed: failing PC32 control, adapted shared link, executed public API
  and hidden dynamic internals. Full real renderer compilation is in progress;
  this focused result does not yet prove LibWeb/media graph acceptance.

Limits: fresh hosted Windows/Linux/macOS shutdown acceptance remains pending.
The optimized macOS IPC sender-URL/history-ordering failure still needs its own
fix and native acceptance before packaging/publication. Goal active.

## 2026-11-06: Order native IPC sender URL behind canonical history updates

- Inspect the recorded optimized macOS sender-URL failure and actual upstream
  history queue. A history message can be received before an IPC envelope while
  its native canonical URL update is still queued. Preserve native page/document/
  realm/request authentication and reserve existing bounded pending slots first;
  append URL sampling to the actual canonical session-history queue.
- Revalidate ownership at that position, sample canonical native URL, and deliver
  through existing VM-busy/revocation checks. A Qt lifetime guard prevents stale
  host dereferences. Complete the history step before application handler work;
  no renderer-supplied URL, guessed event-loop delay, sleep or retry is used.
- Expand the real security fixture to 24 immediate ping requests across three
  fresh documents and alternating same-document updates. Failure diagnostics now
  include ping index, expected suffix and actual native URL. Native macOS hidden-
  window fixture passes with no stderr, retaining serialization/trap/capability,
  request-budget, navigation revocation and window-destruction assertions.
  Default baseline also passed locally; the original failure is authoritative
  hosted optimized evidence, not a claimed deterministic local reproduction.
- Firefly compiles (ipc-history-build.log); targeted actual fixture evidence is
  ipc-history-acceptance.log. No general desktop suite is launched. Add detailed
  API behavior and ADR 0092; maintain upstream integrity and fresh hosted status.

Limits: optimized hosted release acceptance is still pending, as are exhaustive
history/redirect/subframe and hostile platform-policy cases. Continue Linux real
renderer build/runtime and Windows desktop/platform distribution work. Goal active.

## 2026-11-06: Share bounded native clipboard preparation across platform adapters

- Extract MIME selection, wire/item shape, duplicate detection, canonical Base64
  decoding and aggregate byte budgets into commented clipboard/Protocol.cpp/.h.
  The prepared result owns plain bytes and no OS/VM objects; any failure discards
  partial preparation before platform clipboard ownership can change.
- macOS AppKit now consumes this shared result and retains its native read/type/
  provider/publication logic. Build the parser as one QtCore-only static library
  used by the runtime and native probes; standalone Mite gains no Qt dependency.
- Add an actual non-GUI native protocol probe covering Unicode/embedded NUL,
  three representations, clear, malformed shapes/MIME/duplicates/noncanonical
  Base64, exact eight-MiB acceptance and combined-representation overflow. It runs
  before desktop gates on macOS and before the large Linux renderer compilation.
- Local parser probe passes. Existing actual private-pasteboard probe also passes
  byte/type fidelity, invalid-write ownership preservation, external UTF-16,
  unknown-format refusal and clear. Firefly and both probes compile successfully
  (clipboard-protocol-build.log); no window or general clipboard is accessed.
- Source style (120 Python, 546 JS/web/config, 162 native, 21 CMake), actionlint,
  Firefly/upstream integrity and whitespace gates pass. Linux-native protocol
  acceptance remains pending its dedicated workflow; parser portability is not
  represented as a working Linux clipboard service or desktop runtime.

Limits: next connect this shared protocol to the native Linux clipboard provider
and verify it with a second OS process in isolated X11 CI. Current real Linux
renderer and macOS release builds remain live; no completion/alpha claim is made.
Goal active; no local GUI suite is run in this slice.

## 2026-11-06: Preserve explicitly queued macOS release-profile requests

- The previous macOS concurrency key used only ref/tag. GitHub's default single
  pending slot lets later push snapshots replace an explicitly requested queued
  size/shared-provider run, despite cancel-in-progress=false. Current optimized
  1e044e9 run 42295091789 remains queued behind the live bd1a39b package build;
  preserve it rather than cancelling/restarting it while pushing new work.
- Include event and size/shared-provider profile in the concurrency group. New
  ordinary pushes can replace stale ordinary pending snapshots without replacing
  a manual profile. Keep cancel-in-progress=false. Different profiles may run
  concurrently when runners permit; compilation/tests/publication gates are
  unchanged. This is queue configuration, not a new successful package claim.
- Validate workflow with actionlint, source style and whitespace checks. Document
  queue semantics with the current GitHub primary reference. No local build or
  GUI test is needed for this reversible workflow-only change. Hosted scheduling
  acceptance remains observable after push; do not infer it from static lint.

Limits: the original pending run retains its old group until GitHub schedules it.
Native Linux renderer and macOS release results remain pending; goal active.

## 2026-11-06: Native X11 clipboard provider and isolated cross-process acceptance gate

- Implement ClipboardX11.cpp using actual Qt xcb clipboard ownership, byte MIME
  records and the shared preparation rules. Require the native GUI application
  thread and verify write ownership; other backends refuse explicitly. Keep the
  existing AppKit runtime and independent standalone Mite graph unchanged.
- Read supported text/HTML/RTF, distinguish X11 transport metadata from content,
  reject unknown content and track provider lifetime/dataChanged during native
  transfers. The pinned Qt XCB implementation maps text aliases to text/plain.
  Document ownership/event-loop requirements and Qt's empty-transfer/error limits;
  do not claim Wayland/PRIMARY/Windows or hostile-provider containment acceptance.
- Add a native probe that refuses before GUI initialization without an explicit
  isolated-X11 opt-in. Its actual OS peers bypass the owned backend on one side:
  owned writer/independent reader, independent writer/owned reader and external
  unknown-format refusal. Require Unicode/NUL/HTML/RTF bytes, metadata, live owner,
  failed-write preservation and clear. The Python harness runs only on Linux xcb
  with DISPLAY/explicit isolation, bounds deadlines and kills owner process groups.
- Add this small native target/gate under a dedicated Xvfb server immediately
  after Linux configuration, before the full renderer build. No missing-binary,
  offscreen, mock or silent-skip fallback is used. Public Linux Firefly linkage
  remains unfinished; implementing the native component is not a support claim.
- Local C++ adapter/probe compile succeeds (x11-clipboard-macos-build.log). Actual
  guard check exits 77 before QGuiApplication; no local GUI/clipboard access.
  Shared protocol and actual private AppKit probes still pass byte fidelity,
  preparation refusal/provider/clear cases. Native Linux provider execution is
  pending hosted CI. Source style, actionlint, upstream and whitespace gates pass.

Limits: next observe the real X11 CI gate and integrate remaining Linux screen,
theme, window/runtime/preload services before public Linux packaging. Existing
Linux renderer and macOS release jobs remain live; no alpha result is claimed.
Goal active; local desktop suites remain infrequent and targeted.

## 2026-11-06: Independent native OS-service graph and Linux X11 CI

- Add firefly/native/CMakeLists.txt as a standalone Qt service acceptance graph,
  with no LibWeb/LibJS/Mite or renderer-process linkage. Keep real renderer/window/
  sandbox/preload gates separately mandatory; this accelerates feedback without
  narrowing the required complete Linux desktop implementation.
- Extend the owned Linux dependency generator with explicit --prepare/--check
  and --services selection. Reuse the exact renderer Qtbase features, original
  baseline/overrides and absolute overlays; the full renderer manifest remains
  unchanged. Tests prove graph/provider preservation and refusal to accept the
  services manifest as a full renderer manifest. No upstream bytes are modified.
- Add an independent Linux OS-services workflow/concurrency group with pinned
  toolchain/Qt packages, matching renderer cache paths and native diagnostics.
  It builds only the small owned service targets and requires protocol, isolation
  guard and actual cross-process clipboard peers under its own Xvfb server.
  Native Linux results remain pending until this new hosted job executes.
- Make the guard a reusable --check-guard CLI test, stripping isolation opt-in
  before launching the native executable. It must refuse before GUI setup.
  Standalone macOS CMake build succeeds: eight small compile/link steps; protocol
  and guard CTests pass 2/2 in 1.03 seconds. No GUI application/clipboard is opened.
- All 64 standalone Release CLI CTests pass in 20.41 seconds, including updated
  manifest/layout guards. Document local native Linux commands, separate graph
  boundaries and deadlines; preserve all full-stack acceptance requirements.

Limits: native Linux clipboard, full renderer and optimized macOS package results
remain pending. Current live jobs are preserved. Next resolve observed platform
failures and finish remaining Linux/Windows native desktop/distribution services.
Goal active; no local desktop suite is launched in this slice.

## 2026-11-06: Make the independent child-stdin acknowledgement portable bytes

- Hosted native Windows 186feef run 42294694385/job 128161062260 is terminal
  failure in the new shutdown fixture. Both actual Node and Mite report the same
  Python stdout CLOSED\r\n against the fixture's CLOSED\n byte contract. This
  is a platform-specific fixture error, not evidence of a Mite/Node difference.
- Have the independent Python peer write/flush sys.stdout.buffer explicitly.
  Keep the parent-side byte/ordering assertions intact rather than normalizing
  native pipe output or suppressing errors. No runtime API/implementation changes.
- The actual Node/Mite child-process runner now passes native peer shutdown,
  preserved EPIPE writes, owned-policy injected failures and existing complete
  spawn/execFile/pressure/lifetime/inheritance workflows locally
  (child-shutdown-binary-ack.log). Source style and whitespace checks pass;
  fresh hosted Windows acceptance is required before calling that gate green.

Limits: Windows may reveal further native differences after reaching the later
write branch; preserve them as failures until actually understood. Independent
Linux OS-service job 42298304152/job 128169184405 is now live, while the existing
full Linux renderer/macOS package jobs remain active. Goal active.

## 2026-11-06: Supply the required Linux libtool development files

- Independent Linux service run 42298304152/job 128169184405 is now terminal
  failure during native dependency preparation. It successfully restores the
  renderer cache (29 native packages), then libxcrypt explicitly requires the
  system ltdl development headers. This is not a missing Ninja/compiler failure;
  the later CMake errors follow the failed vcpkg toolchain initialization.
- Add Ubuntu libltdl-dev to the shared Linux environment alongside libtool.
  Preserve original dependency versions, ports and native acceptance gates; do
  not update upstream or skip the failing provider. Actionlint/style/whitespace
  checks pass. Native execution still awaits a fresh hosted service run.
- Vcpkg may rebuild a dependency when removing the browser's other roots changes
  transitive feature unions/ABIs, despite the same Qt version/features. Cache
  reuse is validated per package rather than assumed to cover every dependency.

Limits: the X11 clipboard gate has not run yet. Keep the full renderer and macOS
package builds live while the corrected service prerequisites are checked.
Goal active; no local GUI test is run.

## 2026-11-06: Reproducible promotion of verified original macOS CI artifacts

- Add a tracked offline artifact verifier and manual promotion workflow so a
  successful current native/package snapshot can become a macOS-only alpha
  without rebuilding or rewriting its tested bytes. Verification requires the
  successful same-repository native macOS workflow, trusted event, exact source
  commit/version/platform/signing metadata across three archives, four payload
  checksums and measured compressed sizes; reject extra/symlink/mixed assets.
- Stream hashes, bound JSON/archived metadata and reject duplicate JSON keys or
  ambiguous root BUILD entries. Never extract/repack archives or run downloaded
  code. Record reviewable run/commit/version/size provenance and notes, retaining
  original snapshot filenames/embedded versions explicitly.
- Default dry-run verification has read permissions and no tag/release mutation.
  A separately gated write job reverifies transferred assets, refuses tag moves
  and published-release overwrite, uploads to a draft, and requires every remote
  asset state/size/server SHA-256 before publication. It cannot turn a failing
  native build into an alpha; all original full-stack gates remain mandatory.
- Owned policy tests use explicitly synthetic ZIP fixtures, covering source-run
  refusal, hashes, duplicate/oversized metadata, mixed commit/version/platform/
  signing/provenance, extra/symlink assets and remote-draft/digest refusal. Actual
  original alpha.1 CI archives also verify against authoritative successful run
  42155631670/source 745c7af/version 0.1.0-dev.34. This validates the tool against
  real existing artifacts, not newer source/native acceptance or a new release.
- All 65 local Release CLI CTests pass in 20.57 seconds with the new verifier
  suite; focused policy and actual-archive checks pass after signing-policy
  validation. Source style, actionlint, upstream integrity and whitespace gates
  pass. No native desktop/GUI suite is launched for this publication tooling.

Limits: fresh hosted workflow dry-run/remote-upload acceptance is pending. Current
Linux services/renderer and macOS candidate gates remain separate/live; do not
publish older easy-to-pass source as a substitute for the requested current
candidate. Goal active; Windows/Linux desktop packages remain incomplete.

## 2026-11-06: Hosted alpha promotion dry-run acceptance

- Manual dry-run 42300365797 at d4bf7b0 completes verification job 128176254101
  successfully. It actually downloads source run 42155631670's original native
  macOS artifact and validates all five assets against source 745c7af/snapshot
  0.1.0-dev.34. The reviewable eight-file verification bundle uploads successfully
  as artifact 13543247981 (252533973 bytes), preserving the five original assets
  plus authoritative run metadata, verification report and generated notes.
- Publication job 128176345340 is explicitly skipped because publish=false.
  No tag, draft, published release or existing asset is changed. This is actual
  hosted cross-run download/verification/transfer acceptance, not a claim of a
  tested real remote-upload publication path or a newly accepted native source.
- Document the exact hosted evidence. Current Linux service dependency build
  42299888412/job 128174620346 and full renderer/macOS candidates remain live;
  preserve their handles and wait for actual outcomes before publication.

Limits: only a successful current native/package source can qualify for the next
alpha. Real draft upload/server-digest/publication acceptance and native Windows/
Linux desktop delivery remain open. Goal active; no local GUI test is run.

## 2026-11-06: Fresh native Windows/Linux acceptance of child-pipe correction

- Actual 2c594a3 native Windows run 42298513503/job 128170703077 now succeeds:
  all 64 Release suites pass in 114.29 seconds, with mite-child-process passing
  in 8.47 seconds. This reaches the corrected binary acknowledgement and later
  native closed-peer write branch; Node/Mite results match without parent-side
  newline normalization or suppressed write errors.
- Actual native Linux run 42298512805/job 128170689712 also passes all 64 suites
  in 75.57 seconds, with child-process acceptance in 7.41 seconds. Together with
  local native macOS acceptance this verifies the scoped correction on all three
  runtime platforms. Update child-process API and runtime compatibility baseline.
- These runs predate the new offline release-artifact verifier CTest; the current
  local suite has 65 tests and newer hosted runs must prove that source separately.
  Do not confuse CLI/runtime acceptance with Firefly Windows/Linux desktop support,
  native X11 service acceptance or a completed macOS package/alpha build.

Limits: remaining Linux OS-service/renderer and macOS release candidates are still
active. Continue their actual gates and the full native desktop/distribution
requirements. Goal active; no local GUI test is needed for this evidence update.

## 2026-11-06: Checked C/C++ object reuse for long native renderer builds

- Add an explicit --ccache renderer-driver option using the pinned upstream
  setup hook and owned C/C++ launcher settings. Missing tools and unsupported
  native platforms fail explicitly. Audit actual generated Ninja compile commands
  for the selected target/dependencies so an overridden launcher cannot silently
  claim caching. Opt-out clears saved launchers in an existing build directory.
- Enable the option in future Linux renderer CI, install ccache with its native
  prerequisites and keep object-cache storage separate from vcpkg. Use a strict
  owned config: compiler-content identity, no sloppiness and 2G cache setting.
  Preserve completed objects on failure and retain actual statistics. Fresh
  linking and all native process/security/window/preload gates stay required;
  no live job is cancelled/restarted to apply this change retroactively.
- Native acceptance builds and runs actual C/C++ binaries with an isolated cache
  and the actual upstream hook. Identical recompilation records real hits;
  header/source/flag changes execute changed results. Explicit opt-out stops
  cache activity and per-target launcher bypass is rejected. Local ccache 4.13.6
  acceptance passes. This is build-cache correctness, not a native Linux renderer
  or measured hosted speedup claim. No GUI app/test is launched.
- Source style, actionlint, driver help, whitespace and upstream integrity checks
  pass. Document configuration/reuse scope and required independent acceptance.
  The cache test runs explicitly in renderer CI; it does not make ccache a new
  dependency of the standalone Mite CLI/runtime or its ordinary 65-test suite.

Limits: hosted cold/hot compiler statistics and Linux renderer execution remain
pending. Existing Linux services/renderer and macOS package candidates are live;
continue their actual results and full required platform/distribution work.
Goal active.

## 2026-11-06: Match pinned Linux Qt Wayland interface requirements

- Full native Linux renderer bd1a39b run 42293973172/job 128154307621 is terminal
  compile failure in upstream ExternalURLActivationToken.cpp: QWaylandApplication
  and private QWaylandWindow are absent from the selected Qt headers. Inspect
  pinned Qt/vcpkg sources: both declarations are gated on QT_CONFIG(wayland),
  while our Linux feature list enabled X11 but omitted that Qtbase feature.
- Enable wayland in the owned Qtbase feature record and include native Wayland
  protocol prerequisites. Keep the exact upstream Qt version/baseline/overrides
  and unmodified Ladybird source. Do not omit the companion browser/resource
  target or patch the activation source to bypass this real SDK contract.
- Add a small SDK requirement module shared by Linux frontend and standalone
  native services: inspect the actual imported Qt GUI enabled features and fail
  configuration when Wayland interfaces cannot be enabled in this pinned SDK.
  This checks the SDK contract, not runtime Wayland support; current gates remain
  X11. Guard reads fresh imported metadata, without a stale cached probe result.
- Actual local Qt SDK with Wayland disabled is rejected during configuration
  with the owned diagnostic (linux-qt-requirement-negative.log). Native macOS
  service configuration/build and both non-GUI protocol/guard tests still pass.
  All 65 Release CLI CTests pass in 21.06 seconds, including updated manifest
  invariants. Source style, actionlint, upstream and whitespace gates pass.

Limits: rebuilding the pinned Linux Qt provider and real graph must establish
positive native acceptance. Existing independent service/macOS candidates remain
live and are not restarted on observation timeouts. Public Linux Wayland/desktop
and Windows desktop/distribution work remain unfinished. Goal active.

## 2026-11-06: Isolate the pinned CI formatter toolchain from runner installation state

- Source-style 4b3a83d run 42303495798/job 128186935858 fails before source checks:
  rustup reports a clippy-preview conflict at bin/cargo-clippy while preparing the
  already-present 1.98.0 toolchain. This is recorded tool-installation evidence;
  do not mark formatting/generated checks successful when they were skipped.
- Give only the authoring-tools job a fresh runner-temporary RUSTUP_HOME. Keep
  Cargo home/registry locations and other running jobs unchanged. Install/select
  exact Rust 1.98.0 with both rustfmt/clippy matching rust-toolchain.toml, and log
  the actual selected formatter version before checking sources.
- Actionlint, maintained source style, whitespace and upstream checks pass locally.
  Hosted clean-home tool installation/source reproduction needs its next run;
  no local toolchain installation or GUI test is performed.

Limits: the separate native Linux service and optimized macOS package jobs remain
live. Continue their actual acceptance; no new alpha or platform result is inferred
from this tool-bootstrap correction. Goal active.

## 2026-11-06: Independent X11 geometry component and native clipboard acceptance

- Corrected standalone Linux-service f05a24b run 42299888412/job 128174620346
  is terminal success. Inspect actual logs: pinned Qt 6.11.2 compiles, all three
  registered tests pass in 0.44 seconds, including real Xvfb cross-process
  clipboard ownership/readers/writers. This establishes the earlier X11 provider,
  not public Linux Firefly integration or the newer Wayland-enabled SDK graph.
- Source-style f0c997f run 42304728262/job 128191058320 is terminal success:
  the isolated pinned Rust toolchain fix reaches and passes its actual source/
  generated-adapter gates. Linux CLI 4b3a83d run 42303495838 is also successful.
- Add an owned Linux window geometry library using the existing checked native
  frame/content adapter plus screen-work-area centering. Preserve the AppKit
  runtime provider and unmodified Ladybird. Native-service acceptance now builds
  Widgets-based geometry independently of LibWeb, LibJS or Mite.
- Add a dedicated X11 probe and independent xwininfo/xprop harness: decorate/map
  framed and frameless windows under owned Openbox on the isolated Xvfb display;
  compare native root coordinates, sizes and frame extents with the owned API;
  require outer/content updates, size-only origin preservation, rejected-content
  size preservation and actual desktop-work-area centering. Poll native configure
  results with bounded deadlines, retain last native/owned observations on failure,
  and terminate owned process groups. Install explicit X11 query prerequisites.
- Guard before QApplication construction requires the opt-in, xcb and DISPLAY.
  Local native build passes; all four non-GUI service tests pass in 0.97 seconds.
  The guard checks missing opt-in, wrong plugin and missing display without
  connecting to a desktop or creating any GUI application. No local GUI is opened.
- Source formatting, actionlint, upstream/whitespace and 179-page site checks pass.

Limits: actual Linux geometry execution and the new Qt Wayland feature selection
still need hosted acceptance. Hidden/unmapped X11 decorations, fractional scale,
multiple screens, reserved panels, oversized windows, other window managers and
Wayland are not verified. Public native Linux/Windows desktop and packaging remain
unfinished. Existing macOS candidates remain live; no new alpha is claimed.
Goal active.

## 2026-11-06: Cancellable EventEmitter Promise waits and actual release of retained state

- Implement events.once(emitter, name, { signal }) through the existing private
  branded AbortSignal hooks; inject those hooks only into the trusted builtin
  factory. Invalid options/signals reject a Promise. Pre-abort or later abort
  preserves AbortError/ABORT_ERR and original reason identity. Success/error/abort
  remove only the owned event/error listeners and dispose cancellation hooks.
- Handle registration exceptions and reentrant cancellation without stranding a
  listener. Keep captureRejections explicitly unsupported; EventTarget sources
  and async event iterators remain unimplemented rather than implied by this API.
- Actual Node 24.15.0/Mite fixtures cover validation, pre/later abort, reasons,
  public propagation stops, event/error/symbol results, immediate late abort,
  independent concurrent waits and initial registration failure. Eighteen actual
  cancellation comparisons pass. Legacy unsupported checks retain the two
  captureRejections refusals; invalid signals now have the tested type error.
- Record actual differential policies separately: Node cancels synthetic public
  abort while Mite requires real signal state; Node can leave a pending wait when
  newListener aborts during registration, or retain an event listener when error
  listener registration throws. Mite cancels/cleans up those cases. The fixture
  asserts exact distinct results; do not claim differential equality for them.
- Native forced-GC Node/Mite fixture keeps 48 controllers alive while all 96 weak
  emitter/payload targets from successful/error/aborted waits become collectible.
  This proves release beyond an emitter listener-count assertion. The non-GUI
  embedded host and its complete lifetime suite pass.
- Regenerate the embedding bundle/manifest reproducibly from commented ESM. Add
  a detailed events API reference and update cancellation/runtime/compatibility
  links. Mark fresh native Linux/Windows acceptance of this addition pending.
- All 65 Release CLI CTests pass in 20.56 seconds. Source formatting, generated
  reproduction, actionlint, upstream integrity, whitespace and site checks pass.
  No local GUI application is created or opened.

Limits: ongoing native renderer/service and macOS package candidates remain live.
This runtime API addition does not establish new desktop/platform or release
acceptance. Continue the complete specification and required platform work.
Goal active.

## 2026-11-06: Bound manual examples after the native macOS API gates pass

- Standard macOS 4dc3cab run 42296377267/job 128162578490 reaches terminal failure
  in test_electron_examples.py: dark-mode is marked manual, but the harness starts
  it interactively without --smoke and waits until its 40-second process deadline.
  Its nativeTheme capability now exists, so the old expectation of an unsupported
  API error no longer stops the app. This is an example-harness failure, not a
  successful package/release result or a reason to increase the timeout.
- The same hosted logs establish passing actual Electron/Firefly screen, outer/
  content geometry, nativeTheme/CSS, async clipboard, public preload and IPC
  gates, including the prior canonical-sender correction. Preserve that scoped
  evidence while keeping the overall package run failed and alpha unpublished.
- Pass --smoke to non-smoke examples as well. For a partial/manual catalog entry,
  require the existing ERR_EXAMPLE_MANUAL_VALIDATION exit; for unsupported entries,
  still require an API/platform rejection. Label manual and blocked coverage
  separately, without promoting either to feature acceptance or silently skipping.
- Targeted native local verification stages all current ESM examples but runs
  only dark-mode once with the existing compiled Firefly binary and --smoke.
  It exits 1 with the exact manual-validation marker and no success JSON. No
  buttons, theme changes or complete desktop suite are invoked. This is one
  narrowly scoped GUI run after the preceding non-GUI development, not repeated
  desktop polling. Normal interactive behavior is unchanged.
- Source style, actionlint, upstream integrity, whitespace and 180-page site checks
  pass. Update the catalog's coverage summary and public clipboard acceptance
  without claiming that the failed overall package run produced an alpha.

Limits: the optimized 1e044e9 candidate is still live and contains this exact old
harness (git comparison with 4dc3cab is identical for the runner/catalog/example).
It cannot establish a corrected end-to-end release without the new source. Build
the corrected current candidate and require all gates before alpha promotion.
No missing/manual feature is advertised as working. Goal active.

## 2026-11-06: Run the corrected optimized macOS release candidate

- Dispatch the macOS workflow on 57c2675d925981728c2c18bbf4ec62f3f4961fc7 with
  optimize_size=true, shared_openssl=true and publish_alpha=false. Actual run
  42306965867/job 128198633168 starts checkout/build preparation. It includes the
  fixed bounded example harness and the new cancellable events.once consumer.
  Require its native API/security/renderer/example/package gates and measured
  archive/provider evidence before using the existing alpha promotion workflow.
- Verify the old optimized 1e044e9 run has the identical failing example runner,
  catalog and dark-mode sources as terminal failed standard 4dc3cab. Request
  ordinary cancellation because of that demonstrated source defect, not an
  observation timeout. Its compile/test step is confirmed cancelled; the
  always-run completed-dependency cache preservation step remains live. Avoid
  spending another full test pass on the proven interactive-harness failure.
- Existing Linux renderer/native-service candidates continue independently.
  No successful package, new size measurement, tag or alpha is inferred from
  starting a replacement candidate. Goal active.

## 2026-11-06: Opt-in listener rejection capture on native nextTick

- Implement EventEmitter captureRejections with an authored ESM helper for
  returned Promises/thenables. Read then once, preserve its receiver and original
  event arguments, and deliver captured failures through the existing native
  process.nextTick queue. A custom captureRejectionSymbol method receives the
  original error/name/arguments; otherwise emit error through errorMonitor and
  ordinary handlers. Synchronous listener/thenable failures retain their tested
  contracts. Disabled emitters never inspect the returned then property.
- Disable capture while delivering the fallback error, restore the saved state
  afterward, and leave async error-handler failures unhandled rather than
  recursively capturing them. Actual Node/Mite subprocesses prove fatal missing
  error handlers and async error-handler rejection; no swallowed failure or
  weakened exit assertion is introduced.
- Implement the validated boolean static default, saved per instance. Preserve
  the pinned Node 24.15.0 behavior of inheriting a true default for falsey
  constructor options, including explicit false. Reject truthy invalid options
  and non-boolean static assignments. Remove the obsolete unsupported fixture.
- Actual Node/Mite fixtures pass for listener/microtask/nextTick/monitor/error
  order, once hooks, symbol events, identities, thenable getters/calls, ordinary
  values, hostile synchronous errors, disabled mode, defaults, state restoration
  and async monitor failure. Native forced-GC tests retain 32 pending receivers
  and payloads, then collect all 64 targets after rejection delivery. The full
  lifetime suite and all 65 Release CLI CTests pass (20.45 seconds).
- Generated events embedding/manifest reproduce from the commented ESM helper.
  Update detailed events/runtime/compatibility documentation and clarify which
  original ADR limitations have since been implemented. Formatting, actionlint,
  upstream integrity, whitespace and 180-page site checks pass. No local GUI test
  or app is launched. Fresh native Linux/Windows acceptance remains required.

## 2026-11-06: Positive Wayland-enabled Qt provider and retire proven stale build sources

- Linux-service 4b3a83d run 42303495733/job 128192217506 is terminal success.
  Inspect logs: actual pinned Qtbase 6.11.2 includes wayland and the imported SDK
  feature check passes. All three native Xvfb clipboard tests pass in 0.29 seconds.
  This verifies the corrected provider/X11 service, not Wayland runtime behavior,
  full LibWeb rendering or the later geometry gate (368bd1c job 128201839164).
- Reinspect terminal renderer bd1a39b job 128154307621: ExternalURLActivationToken
  fails with missing native QWaylandApplication/private QWaylandWindow. Execute
  both manifest generators from failed bd1a39b and live d4bf7b0 against the same
  unmodified pin; their entire generated renderer manifests are identical and
  both omit the required feature. Cancel live renderer 42300216473 for that
  proven configuration defect. Its compile step is confirmed cancelled and
  completed-dependency preservation remains live; corrected queued sources can
  now use the fixed provider and required native gates.
- Also confirm active default macOS 368bd1c run 42305462916 has the identical
  failing example-runner blob as terminal failed 4dc3cab. Request cancellation
  rather than repeat that known manual-app timeout. The corrected optimized
  57c2675 run 42306965867 remains live and is preserved. These cancellations follow
  demonstrated source defects, not missing live logs or observation timeouts.

Limits: no new package, size report, Linux geometry/renderer or alpha acceptance
is inferred. Continue those exact live/queued gates and required public native
Linux/Windows desktop integration/distribution. Goal active.

## 2026-11-07: Wire the actual Mite-driven Linux X11 application candidate

- Enable the owned firefly target on native Linux with the common LibJS/Mite
  main process, Qt BrowserWindow and validated preload/contextBridge/IPC bridge.
  Select the actual X11 clipboard and Linux geometry/centering providers; keep
  native Windows desktop explicitly unsupported. This is source/build integration, not
  a claim of completed Linux runtime acceptance.
- Introduce one desktop service installer. macOS forwards to its existing native
  display/theme bindings and QObject-scoped watchers; Linux registers explicit
  unsupported errors for unfinished display/preferences services. The private
  unavailable-theme flag also rejects source reads and same-value assignments,
  preventing a silent default/success for an absent native provider. Real
  Node/Mite policy comparisons pass for all those refusals.
- Generate the full Linux graph from the unchanged renderer/Qt manifest plus
  Mite's existing libuv/llhttp pins. Extract the shared upstream input reader to
  avoid circular imports. Actual generator/layout tests retain original pins,
  overlays and Mac paths, and reject renderer/service/Mac manifests as the full
  Linux runtime. Owned runtime helpers use Linux sandbox/LeakSanitizer sources
  and firefly-runtime/bin plus share/Lagom resources.
- Keep Linux raw-build helper lookup explicitly configured. The upstream build
  also places an unadapted WebContent in adjacent bin/, so treating that location
  as a packaged runtime would bypass the owned preload session. The independent
  peer requires /proc/renderer/exe to resolve to the actual owned runtime helper.
  Native Linux packaged discovery/distribution remains required future work.
- Add a modern ESM-authored app/preload/document fixture and isolated Xvfb/Openbox
  gate: real Mite HTTP assets, LibWeb document, early frozen preload API, native
  sender URL/ownership and IPC reply, clipboard bytes, window sizing/centering and explicit
  unfinished-service refusals. An independent HTTP peer reads the live renderer's
  executable/PID/parent, NoNewPrivs and seccomp/filter state before authorizing
  the app's success exit. Preserve actual logs/kernel records, including failures.
  No JS marker substitutes for the native process check; activation alone is not
  complete containment. Existing probe/isolation/window gates remain mandatory.
- Local current macOS application, Linux service translation unit and environment
  probe compile/link. The actual environment probe rejects wrong plugin/missing
  display without GUI initialization; firefly --help and geometry opt-in guards
  pass. Bundle/syntax-check the final ESM fixture without running a GUI. The full
  65-test Release CLI suite passes in 22.23 seconds. Source/generated/style,
  actionlint, upstream, whitespace and 181-page site checks pass. No local window
  or desktop suite is opened in this step.

## 2026-11-07: Preserve the actual X11 geometry failure and improve its evidence

- Native-service 368bd1c run 42305462825/job 128201839164 is terminal failure:
  five of six tests pass, but geometry-native-x11 times out waiting for the
  framed window's _NET_FRAME_EXTENTS. This does not prove geometry failure or
  success beyond that observation; do not label the component/runtime accepted.
- Establish a real probe event-loop/API response before the independent WM
  metadata wait. On failure capture full xwininfo/xprop output and Openbox's
  pre-cleanup process status, rather than only an empty observation map. Keep
  required native extents/frame assertions and the existing deadline; do not
  add guessed margins, skip the gate or promote its negative guard to success.
- Include the same actual native geometry gate before the renderer workflow's
  large compile/runtime gates, with explicit X11 tools and fresh isolated display.
  Positive geometry execution and root-cause diagnosis still need the next run.

Limits: Linux native end-to-end compilation/execution, geometry acceptance,
display/theme and further OS services, Linux package/signing/physical displays,
Wayland and native Windows desktop remain unfinished. Existing corrected
optimized macOS 57c2675 run 42306965867 remains live; no new alpha or package
success is inferred from this integration commit. Goal active.

## 2026-11-07: Release captured delivery data despite conservative callback reachability

- Standard macOS ae6a3fe run 42308089209/job 128202577034 fails the unchanged
  event-rejection-release fixture: Node collects every target, while Mite reports
  capture released false after all 32 rejections were delivered. Keep that hosted
  failure distinct from the earlier successful local size-profile suite.
- Reproduce locally without rebuilding or modifying Ladybird: compile copies of
  the embedding host, EventLoop.cpp and ImmediateQueue.cpp with -O3, reuse the
  existing engine/dependencies, and link a separate ignored native executable.
  The original fixture fails all three independent processes in this partial
  O3 reproduction. This is not a full-profile/hosted acceptance claim.
- Instrument only an ignored host copy through the pinned GC retaining-path API.
  After delivery one payload remains reachable from a conservative StackPointer
  to a completed ECMAScript callback, its environments and the argument array.
  Pending targets have the expected precise Promise/VM roots. No upstream source
  is changed; inspect capture-gc-o3-probe/{retention,retention-fixed}.log.
- Give each queued rejection a private delivery record in a separate function
  scope and clear every field in finally. A completed callback retained by a
  native stack slot therefore cannot extend ownership through that record.
  Keep the original thenable observer alive when its producer retains it;
  repeated early/later rejection calls preserve the original event arguments.
- The same partial O3 executables now pass the unchanged all-64-target collection
  assertion in three independent native processes. Instrumented execution finds
  zero tagged payload retainers after delivery. No retry, additional GC turn or
  weakened collection assertion is used. Add actual Node/Mite coverage for
  repeated/later thenable rejections and per-delivery argument independence.
- Rebuild the real Release CLI and run all 65 CTests: pass in 20.82 seconds.
  Regenerate the events embedding/manifest and document the delivery lifetime.
  No GUI app is launched. Fresh native/profile CI still must establish acceptance.

Limits: optimized macOS 57c2675 run 42306965867 remains live and predates the
capture addition. The Linux geometry gate now exposes an unmapped native window
while Openbox is live; its root cause and native application acceptance remain
unfinished. Continue those gates and all required desktop/platform work.
Goal active.

## 2026-11-07: Align the isolated X11 probe with native application window creation

- c2c2db5 native-service job 128214890032 (run 42311835475) fails the same frame
  metadata wait. Its newly retained diagnostics now establish actual server
  state: the observed window is IsUnMapped, with root parent/no WM_STATE or frame
  extents, while Openbox's process remains live. The probe can answer the startup
  API request, so an unresponsive event loop is not inferred from that timeout.
- Match the application's native creation sequence: set the same native-sibling
  policy, materialize the widget's native window before show(), and emit readiness
  from the running Qt event loop with the actual QWindow ID. Keep visibility,
  widget/native IDs and offscreen state in failure evidence. Require visible
  widget/native state before the independent server frame assertions; retain
  native extents/nonzero decorations and the unchanged deadline.
- This corrects probe initialization/identity; positive native mapping/geometry
  acceptance still needs the next isolated Linux run. Do not mark the runtime
  accepted or replace server evidence with the Qt visibility flags. Local current
  source compiles; the four non-GUI service/guard tests pass. No local GUI opens.

Limits: corrected GC source 87845c8 needs fresh profile CI; current Linux geometry
and native application gates remain unverified. The optimized older macOS release
candidate is separate and remains under its original native/package gates.
Goal active.

## 2026-11-07: Establish independent X11 manager readiness and fresh native CLI acceptance

- Corrected source 87845c8 passes all 65 native Release runtime suites on Linux
  x64 (run 42313640601/job 128220859618, 89.02 seconds) and Windows x64
  (run 42313640611/job 128220914189, 121.07 seconds). The unchanged forced-GC
  capture lifetime assertions pass. Update the events/API acceptance scope;
  this does not establish either platform's desktop runtime.
- Geometry source 54dadb4 still fails isolated Linux frame acceptance in run
  42314465500/job 128223587710: Qt reports visibility, but X11 reports the same
  window unmapped without frame metadata. Native IDs agree. Keep this failure
  explicit; probe initialization alone did not fix mapping.
- Extract the shared isolated-X11 manager harness. Before any Qt client runs,
  require an independent xmessage control window to become server-viewable with
  actual nonzero WM frame extents. The existing x11-utils dependency provides
  this client. Keep the control alive until acceptance; remove/reap it before
  the application starts. Preserve the ten-second readiness deadline and all
  existing independent geometry, ownership and sandbox assertions.
- On failure retain the control's server observations plus manager/client exit
  status and Linux /proc wait/process state. Both app/geometry harnesses use the
  same lifecycle; always kill/reap their own process groups. Refuse non-Linux,
  missing display, non-xcb and missing isolated-display opt-in before launching
  any client. This is a diagnostic/readiness improvement, not native acceptance.
- Optimized/shared-OpenSSL macOS 57c2675 run 42306965867 is now terminal failure:
  all 65 CLI suites pass, then public IPC security-main reports invalid handler
  inputs or lost pending budget. No verified release artifacts were uploaded;
  do not promote this run or claim alpha. Diagnose this security gate next.

Validation: Python syntax checks and all four unsafe-display guard cases pass
before any query/process launch. The actual native probe isolation guard and
all four non-GUI service CTests pass (0.39 seconds). Repository formatting,
actionlint, the unmodified upstream pin and site links pass. No local GUI starts.
Positive X11 acceptance and the macOS release/security repair remain outstanding.
Goal active.

## 2026-11-07: Synchronize IPC security assertions with actual main-handler receipt

- Investigate the optimized 57c2675 IPC failure with a focused native run instead
  of repeating the desktop suite. The original fixture with detailed counters
  passes locally, so the hosted failure is not locally reproduced.
- The source allows a renderer-local pending-limit rejection/title update to
  precede main-process handler dispatch (which is deliberately queued after
  native history processing). Treat this ordering gap as a fixture issue;
  it remains an inference about the hosted failure, not a proven root cause.
- Register a main-side batch Promise before loading each security document.
  Only receipt of the sixteenth held request resolves it. Require both the
  renderer's existing SECURITY_READY and this independent main acknowledgement
  before checking exact counters or navigating/destroying the window. A missing
  request still fails after a bounded five-second wait with cycle/held/ping
  diagnostics. No retries, budget changes or early settlement of held replies.
- Retain exact sixteen-handler/eight-ping-per-cycle assertions, invalid-input
  refusal, zero serialization traps, renderer limit enforcement, stale-reply
  checks and destroyed sender-frame checks. Apply the same assertions to the
  final destruction batch, previously checked only through the renderer title.
- The corrected fixture passes one focused native macOS execution with all
  original result fields true (three navigation cycles plus destruction).
  Both before/after runs are one targeted diagnostic batch using hidden windows;
  no broad local GUI suite is run. JavaScript syntax and formatting pass.

Limits: this is scoped local fixture validation, not fresh optimized CI/package
acceptance. Start a fresh size/shared-provider candidate from the committed
source and retain every existing native/security/package gate. Alpha publication
still requires verified successful artifacts. Linux geometry/app acceptance and
native Windows desktop support remain unfinished. Goal active.

## 2026-11-07: Discover relocated owned Linux runtime helpers

- The previous goal turn made concrete progress: bef1db4 added independent
  X11-manager readiness and 12f15f1 synchronized IPC fixture endpoint receipt;
  both commits are on master. Fresh optimized/shared-provider macOS candidate
  42316338587 at 12f15f1 is confirmed live, not assumed stopped after polling.
  Older corrected-GC macOS 42313640608 and isolated Linux services 42316129815
  remain live under their existing gates. Do not restart them merely for age.
- Extract runtime path selection from the Qt application into a small native
  platform component with no Qt/VM dependency. Preserve macOS Contents/MacOS
  plus Resources discovery and the configured development fallback.
- Linux first selects the owned firefly-runtime/bin plus share/Lagom tree
  relative to the actual executable. Never choose neighboring upstream
  bin/WebContent: it lacks the runtime preload session. This removes the
  absolute-build-path dependency when a complete owned tree is relocated.
- Check the exact production selector using real temporary filesystem layouts:
  fallback, a misleading upstream helper/Resources pair, missing helper or
  resources, relocation, spaces and Unicode, both macOS/Linux policies, and
  failed executable discovery. No GUI or engine starts. The test's first local
  failure exposed macOS /var versus /private/var aliases in the test paths;
  canonicalize the temporary root, leaving production assertions intact.
- Strengthen the existing hosted Linux application gate to copy the executable
  and owned runtime into a relocated temporary tree, dereference development
  service symlinks and require its actual live /proc renderer executable there.
  Keep all original preload/IPC/geometry/clipboard/kernel-sandbox assertions.
  A configured-original-helper fallback cannot satisfy this peer's path check.
- Compile/link the real current macOS Firefly and native path probe. All five
  standalone non-GUI service CTests pass (0.73 seconds), including real path
  policy execution. Existing SDK object deployment-target warnings remain;
  this does not establish an older macOS support range. No local GUI runs.

Validation: Python syntax, repository formatting, actionlint, the unmodified
upstream pin, actual platform/dependency generator checks and 181 site pages
with 12,504 local links pass. The full application --help exits without GUI
initialization. Native Linux relocation and window acceptance remain pending. This is helper discovery,
not a self-contained Linux package: dependency/plugin closure, distribution,
screen/theme/OS services and Windows desktop integration remain required.
The optimized release candidate still needs all native/security/package gates;
no alpha publication is claimed. Goal active.

## 2026-11-07: Split HTTP authoring into commented ESM subsystems

- Continue the requested source-readability priority: replace the 1,638-line HTTP
  authoring file with a small stable builtin facade and private protocol,
  message, client and server factories (each below 600 lines). Document ownership
  and the source map. Keep the public module entry and generated embedding ABI.
- Move existing behavior bodies without rewriting protocol decisions. A temporary
  Acorn comparison against the committed source verifies all 31 original top-level
  functions/classes and shared metadata ASTs unchanged. This is move evidence,
  not a substitute for actual native behavior tests.
- Keep realm-local metadata, shared message constructor/WeakMap identity, separate
  client/server connection state and the private HTTPS transport hook. Expose no
  additional helper state or injectable transport API to applications. Fix a
  missing private pairs dependency during source review before native testing.
- Regenerate the committed factory/manifest; all transitive authoring files are
  hashed, and generation reproduces exactly. Build/link standalone Mite and the
  real Firefly executable. No local GUI is launched.
- The full 65-test native Release run passes 62 suites, including HTTP/HTTPS,
  fetch, abort and forced-GC lifetime gates. The machine hibernates from 01:15:36
  to 07:02:15 local time (pmset evidence: 20,799 seconds); the three then-active
  module/CommonJS/npm suites are recorded as timeouts after 20,798 seconds.
  Preserve that failed log and rerun only those three unchanged suites: all
  pass in 0.87 seconds. Thus every suite passes on the current binary, without
  suppressing the interrupted-run failure or changing timeout/assertion policy.
- Fresh hosted source 19ad113 now has successful Linux renderer/application,
  Linux native-service, Linux/Windows CLI and macOS package workflows. Inspect
  their scoped logs and artifacts next; this is new evidence, not a reason to
  mark the overall cross-platform goal complete. Optimized 12f15f1 passed the
  corrected IPC security/budget checks but failed its subsequent crash fixture.

Validation: repository formatting, reproducible bundles/source manifests, the
unmodified upstream pin and 181 site pages/12,505 local links pass. The new
HTTP source organization still needs fresh hosted acceptance; it adds no API
coverage. Native Windows desktop, Linux distributable closure, broader OS APIs
and the remaining specification milestones are unfinished. Goal active.

## 2026-11-07: Accept the native X11 app and repair draft release verification

- Source 19ad113 native renderer run 42316853517/job 128262565703 succeeds:
  actual DOM/process/isolated-world/helper and Qt window gates; independent
  clipboard and real frame/content/size/centering checks; then the relocated
  Firefly/Mite HTTP/LibWeb/preload/IPC app, text clipboard and live /proc sandbox
  activation/ownership. The independent non-Qt manager control is mapped and
  decorated. Do not infer physical desktop parity or full sandbox containment.
- Native-service source 19ad113 run 42316853549/job 128237353747 succeeds;
  earlier bef1db4 run 42316129815/job 128229047474 passes all six original
  service tests in 1.23 seconds. This is positive geometry/manager evidence,
  superseding the earlier unmapped-window failures without hiding their history.
- Full macOS Release/package source 19ad113 run 42316853522/job 128245244531
  succeeds: all 65 runtime suites (266.88 seconds), native/public desktop,
  isolation/helper/IPC security/budget/crash/30-second deadline gates, modern
  sample boot, actual packaged Designer and verified five-file release artifact
  13555114170 (246,291,016 bytes). This is default Release: optimize_size=false,
  shared_openssl=false, not optional-profile acceptance.
- Update platform/API compatibility to this precise X11/macOS scope. Public
  Linux screen/theme, Wayland, physical/mixed-DPI displays, dependency-complete
  Linux distributables and native Windows desktop remain required.
- The previous optimized 12f15f1 run 42316338587 did pass the corrected security
  and budget fixtures, then failed the subsequent crash readiness fixture.
  Diagnose that separately; no optional-profile package is substituted here.
- Start authorized alpha.2 promotion from successful run 42316853522. Run
  42345423192 verify job 128322003883 passes actual original ZIP/metadata/hash
  checks. Publish job 128322097395 creates the exact source tag and draft,
  uploads all five assets, then stops at the REST by-tag lookup (HTTP 404).
  The draft is not public. Authoritative authenticated listing finds draft
  release 437377666 with five assets; its existence is not publication.
- Correct the workflow to resolve the CLI's draft database ID and read that
  exact REST release record before the unchanged count/name/size/state/SHA-256
  checks. Validate positive numeric ID, matching tag and draft state first.
  Retain all source/tag/no-overwrite guards, and set source_sha explicitly when
  creating future drafts. Resume the same draft only after corrected code is
  committed; do not bypass digest verification or move the source tag.

Validation: actionlint, repository formatting, 181 site pages/12,506 local links
and publication-policy refusal checks pass. An authenticated real CLI lookup
returns the expected draft ID/tag/state with the corrected field selection.
Fresh alpha publication still needs successful final server-digest
checks and observed published state. No local GUI runs. Goal active.

## 2026-11-07: Apply main-handler receipt gating to the IPC crash fixture

- Optimized 12f15f1 run 42316338587 fails at the next crash fixture after passing
  security/budget checks. Its crash harness discards the child's stderr and
  reports only "No renderer ready". The crash fixture still has the same source
  ordering gap: renderer-local SECURITY_READY need not acknowledge delivery of
  all sixteen main handlers. This is a source-level race; it is not a reproduced
  optimized failure diagnosis, since that run did not preserve child diagnostics.
- Share the existing five-second main-side receipt/deadline cleanup in a small
  ESM fixture helper. Use it in security and crash fixtures. Register the crash
  receipt before loading; acknowledge only at sixteen actual held-handler
  arrivals, then keep the exact count check before CRASH_READY/PID publication.
  Preserve owned-child verification, real SIGKILL, destroyed sender snapshots,
  stale replies, replacement-window checks, limits and every existing assertion.
- On crash startup failure, stop/reap only the owned process group, retain its
  pre-cleanup status/stdout/stderr in an artifact and include them in the failed
  assertion. Do not claim an unavailable renderer as a passed crash test.
- Actual Node and native Mite checks pass for already-received, delayed and
  repeated acknowledgements and timer cleanup. An actual non-GUI failing Python
  child verifies exit status 4 and stderr survive the harness failure path.
  JS/Python syntax and formatting pass; no local GUI opens. Positive changed
  native crash acceptance remains for fresh hosted CI.
- Corrected alpha promotion run 42345691857 succeeds. Verify job 128322822894
  and publish job 128322917304 pass; final server count/state/size/SHA-256 checks
  pass. Public GET observes release 437377666 draft=false/prerelease=true at
  2026-11-07T06:09:34Z, all five original assets. Alpha.2 points to verified
  source 19ad113 with unchanged dev.118 archive metadata. It does not contain
  the subsequent HTTP refactor or this fixture change. Update release links next.

Limits: the released default profile measures 216.74 MiB framework/250.85 MiB
Designer, not a successful optional size/shared-provider profile. A new optimized
candidate still needs every native/security/package gate. Windows desktop and
Linux distributables/broader OS services remain unfinished. Goal active.

## 2026-11-07: Publish and document macOS ARM64 Alpha 2

- Observe the completed real promotion, not merely its workflow dispatch:
  run 42345691857, verify job 128322822894 and publish job 128322917304 succeed.
  The publish log records all remote draft asset sizes/states/SHA-256 digests
  verified, then the public release URL. Public REST by-tag GET now succeeds and
  observes release 437377666, draft=false, prerelease=true, published 06:09:34 UTC.
- Publish v0.1.0-alpha.2 at verified source
  19ad11390793278c2bda829180de61ff33980d7f, from package run 42316853522.
  Preserve the three original dev.118 ZIPs, SIZE and SHA256SUMS records. Archive
  sizes: Mite 30,881,907; Firefly 105,710,830; Designer 117,703,569 bytes.
  Public server digests agree with the verifier; no source/tag/ZIP rewriting.
- Update README, documentation home, packaging/release guidance and scoped
  app/IPC/crypto/example availability to Alpha 2. Include its exact source,
  snapshot-version provenance, normal Release profile and measured framework
  216.74 MiB / Designer 250.85 MiB uncompressed. The new HTTP source organization
  and receipt/crash-test changes are later master work, not these release bytes.
- Start fresh optional size/shared-OpenSSL candidate 42345882824 from 85557be
  after the crash fixture receipt/diagnostic fix. It is confirmed in progress;
  retain all existing native/security/package gates. This candidate has not yet
  produced verified smaller artifacts and is not part of Alpha 2.
- No local GUI launches in this goal turn. Source changes are the logical HTTP
  ESM refactor, corrected draft-ID publication path, scoped acceptance records
  and crash-fixture synchronization/diagnostics; each is committed separately.

Validation: 181 generated pages and 12,508 local links/resources/fragments pass;
repository formatting and actionlint pass. git ls-remote confirms the annotated
alpha.2 tag peels to exact source 19ad11390793278c2bda829180de61ff33980d7f.
The complete original
specification remains active: native Windows desktop, Linux distributables and
broader OS services, remaining Node/Electron coverage, physical desktop/platform
acceptance and further roadmap milestones are unfinished.

## 2026-11-07: Separate native Qt bridge responsibilities without changing method bodies

- The previous goal turn made concrete progress: HTTP ESM subsystem moves,
  corrected draft release verification, actual Alpha 2 publication and the
  IPC crash receipt/diagnostic correction are committed and pushed. Continue
  the user's main source-readability priority before expanding the API surface.
- Replace the 865-line native Bridge.cpp implementation with a 23-line public
  facade and private bridge/{Lifecycle,Windows,Preload,Ipc,Host}.cpp units.
  The largest unit is now 275 lines. A private Impl header records the shared
  owner/window state and method contracts; Value.h holds internal text conversion.
- Preserve one owner, registry, QObject callback context and rooted JS handler/API.
  Document the VM/Qt owner thread, lifetime requirements, navigation/crash sender
  invalidation, pending slot ownership and teardown ordering. Keep the public
  facade and all native/public APIs unchanged. Mite stays independent of Qt,
  LibWeb and Firefly; no upstream source or platform provider changes.
- Move every original method body intact. A one-off lexical C++ comparison of
  all 18 methods verifies identical token streams including string literals,
  excluding comments/formatting and out-of-class signature qualification only.
  It is move-review evidence, not proof of complete runtime/security behavior.
- Compile/link the actual Firefly target after the move. Separate compilation
  exposed dependencies previously supplied indirectly by the monolith: add
  explicit VM definitions to the binding/preload/window units. SDK deployment
  warnings remain as before; do not infer older-macOS compatibility.
- Keep optimized/shared-provider candidate 42345882824 at 85557be alive under
  its existing native/security/package gates. Linux renderer 42345846818 is
  also confirmed live compiling the actual graph. Neither is restarted because
  polling shows a long step. These sources predate this native-only refactor.

Validation: the actual current Firefly target compiles/links; --help exits before
GUI initialization. All 65 standalone Release CTests pass in one run (21.25
seconds). The 18-method token comparison, formatting, actionlint, unchanged
upstream/generated manifests and 181 site pages/12,510 local links pass. No local
GUI is launched. Fresh hosted macOS/Linux desktop gates
must accept this source move. The overall specification remains incomplete:
native Windows desktop, Linux distributable closure/OS services, optional-profile
release acceptance and further Node/Electron/platform milestones remain open.
An additional tracked-source audit finds role/license headers throughout the
maintained runtime/module/core/native/renderer sources, except the two Qt UI
probes; add accurate role headers to those probes. Large coherent files remain
for subsequent review, especially WorldJobGuard.cpp (1,382 lines) and net.mjs
(1,000 lines); this step does not declare the whole readability priority finished.
Goal active.

## 2026-11-07: Separate world-job isolation policies under one VM-retained owner

- The preceding goal turn made concrete progress: native bridge refactor 1669947
  is committed/pushed, the actual Firefly target links and all 65 runtime suites
  pass. Keep the full specification active and continue the readability priority;
  do not expand API claims because files have been reorganized.
- Split 1,382-line WorldJobGuard.cpp into its existing public registration/diagnostic
  facade and private worlds/jobs units for callback-gate registration, principal
  frames, job/WebIDL dispatch, event values, handler maps, listeners/signals,
  traversal and timer/observer resources. The largest implementation is 270 lines.
- Preserve a single refcounted State retained by the VM hooks and queued jobs.
  Weak realm/world/native registries, native tracing, temporary cleanup roots,
  previous-hook delegation and detach ordering remain unchanged. Private header
  contracts/source maps explain these ownership and source/principal boundaries.
  No engine, public API, process, platform-provider or upstream source is changed.
- A one-off C++ lexical comparison verifies all 36 original private method-body
  token streams including string literals, the exact state fields, hook move
  initializer order and complete public facade unchanged. This is move-review
  evidence alongside actual native tests, not a complete security proof.
- Build/link the real Firefly executable, owned runtime WebContent, native isolation
  probe and wire-process probe with the new explicit CMake source list. Then run
  check-firefly-isolation: actual separate-process wire checks and actual LibWeb
  source/principal/GC/revocation/job/timer/observer/listener/handler/event-snapshot/
  NodeFilter tests pass. The probe creates in-memory documents without Qt windows;
  no local desktop UI or browser is opened.
- Optional optimized/shared-provider macOS run 42345882824 at 85557be and Linux
  renderer run 42345846818 remain confirmed live under their original native
  gates. They predate this source move; do not restart because a step is long.

Validation: all 65 standalone Release CTests pass (20.68 seconds). Formatting,
actionlint, unmodified upstream and both generated-source manifests pass; 181
site pages and 12,511 local links/resources/fragments pass. Fresh hosted desktop/helper acceptance must cover
this source split. Readability review continues with the remaining larger modules;
Windows desktop, Linux distributables/OS services and other required Node/Electron
coverage remain unfinished. Goal active.

## 2026-11-07: Separate TCP validation, native attempts, Socket streams and listener ownership

- Previous turn 309d84d made concrete progress: renderer job/callback policies
  are separated under one VM-retained owner; actual headless LibWeb security
  and all runtime suites pass locally. Keep the full specification active.
- Split the 1,000-line net.mjs into a 28-line stable builtin facade plus private
  ESM policy, connection-attempt, Socket and Server factories. Document their
  source map and private ownership seam. The Socket class keeps its existing
  methods intact; one public constructor serves outbound and accepted channels.
- Preserve the Socket WeakMap across native attempts/dispatch and keep listener
  bookkeeping in its separate factory. Accepted handles are adopted only through
  a private function, never exposed on net. DNS/address ordering, retry timers,
  generation guards, backpressure, abort disposal, errors and close/drain rules
  are moved intact rather than reimplemented.
- A one-off Acorn comparison verifies all 22 original function/class ASTs and
  exact public export expressions unchanged. Keep aliases/constructor identity
  and explicit unsupported modes. This is move evidence, not new API coverage.
- Regenerate the net embedding and manifest with all transitive private ESM
  hashes. Reproduction and native source-manifest checks pass. Build/link the
  actual standalone Mite/embedding host and Firefly executable; no local GUI.
- Audit 207 tracked production implementation/header files across Mite runtime/
  modules and Firefly core/native/renderer/bridge/helper/packaging: every file
  has a leading role/license comment, and no maintained .js/.cjs implementation
  sits outside generated outputs. Formatting still checks all maintained
  sources; this mechanical audit does not by itself prove comment quality,
  correctness or cross-platform feature acceptance. Preserve deliberate CJS
  compatibility fixtures and third-party/generated bytes.
- Keep optional size/shared-provider macOS 42345882824 at 85557be and Linux
  renderer 42345846818 alive under their existing gates. Long native steps are
  confirmed live, not grounds to restart. Latest renderer refactors still need
  their own hosted source acceptance.

Validation: all 65 native Release CTests pass (21.48 seconds), including TCP,
abort, HTTP/HTTPS, fetch and GC lifetime suites. Exact generation reproduction,
source manifests, formatting/actionlint, unchanged upstream and 181 site pages/
12,512 local links/resources/fragments pass. No local UI starts. Current source organization is much
clearer; maintainability work does not complete native Windows desktop, Linux
packages/OS services or the remaining Node/Electron/platform requirements.
Goal active.

## 2026-11-07: Implement native Linux ELF dependency-closure staging

- Previous turn 870f66c is concrete progress: TCP ownership/validation/attempts
  are separated, generated artifacts reproduce and all 65 runtime suites pass.
  Source organization/ESM/comments are established across current production
  roots. Continue toward required native distributables without introducing
  undocumented or empty public API placeholders.
- Add a private native_package ELF component for the unfinished Linux packager.
  Accept explicit staged-destination/original-source mappings. Discover native
  dependencies at originals, since existing $ORIGIN build RUNPATHs cannot
  resolve in an incomplete staged tree. Require physically separate staged
  copies with matching source hashes before any patching; reject hardlinks.
- Keep the explicit glibc/loader ABI on the host. Copy remaining transitive ELF
  dependencies once into lib/, deduplicate identical payload aliases and reject
  same-name/different-content collisions before mutation. Patch only staged
  objects with relative $ORIGIN RUNPATHs and recheck actual loader resolution
  inside the package. Record original/packaged hashes, tool versions, binary
  provenance, host ABI requirements and physical library count.
- Refuse non-native-Linux-x64 hosts, non-ELF/wrong-architecture inputs, unresolved
  or malformed loader entries, external/symlinked destinations, mismatched
  copies, hardlinks and existing lib/ staging output. Clear LD_* overrides for
  loader inspection so a developer shell cannot hide missing package paths.
- Add an actual native C/DSO fixture: two main/helper layouts share a transitive
  library chain; copy/patch/deduplicate, move the package, remove original build
  locations and run both programs. Also check source preservation, deletion of
  a required library, conflicting DSOs, hardlinks/tampering and staging refusal.
  Add this required gate in an independent Linux CI job and before the large
  renderer build, using the native system patchelf tool and recording its
  version. Preserve existing gates; component acceptance need not wait for Mite.
- Locally run only non-native platform/parser/refusal checks and Python syntax;
  pass with no loader/patcher/GUI invoked. The native compiled fixture still
  needs hosted Linux execution. Do not substitute parser checks for ELF proof.

Validation: Python syntax, formatting, actionlint, unchanged upstream and
non-native refusal/parser checks pass; 181 site pages and 12,514 local links/
resources/fragments pass. Native ELF execution remains pending hosted CI.
This component does not yet package Firefly/Designer: resource/plugin closure,
licenses, launchers/archives and real packaged-app acceptance remain necessary.
Native Windows desktop and remaining Node/Electron/OS services are still open.
The optimized older macOS and Linux renderer candidates remain confirmed live;
no timeout-based restart or additional release claim. Goal active.

## 2026-11-07: Isolate native package-component acceptance from long runtime queues

- 8a73b40 is committed/pushed with actual ELF closure staging and a compiled
  fixture. Its Linux development run 42347920082 is authoritatively pending
  behind an older workflow in the shared linux-master concurrency group. The
  component job inside it cannot start independently of that workflow queue.
- Move that small native gate into its own package-component workflow/concurrency
  group. Keep the same pinned native compiler setup, patchelf and real DSO fixture,
  plus the required renderer-side component gate. Do not cancel or restart the
  existing CLI, renderer or optimized macOS builds merely for observation age.
- Add ADR 0094 for original-location dependency discovery, separate-copy checks,
  common-lib deduplication, relative RUNPATHs, host ABI policy, native fixture
  proof and the remaining whole-application packaging requirements.

Validation: actionlint and diff checks pass; 182 site pages and 12,581 local
links/resources/fragments pass. Native compiled-component acceptance is pending; local parser/refusal checks are not a
substitute. No local desktop UI starts. Goal active.

## 2026-11-07: Prove real native ELF closure staging and relocated execution

- Independent [package-component run 42348028753](https://github.com/jogibear9988/elytra/actions/runs/42348028753)
  at 209f64e/job 128329923480 completes successfully. Authoritative logs record
  Ubuntu Clang 22.1.8 and patchelf 0.18.0, actual transitive ELF closure and
  main/helper execution after relocation with the original build paths removed.
- The unchanged native fixture asserts two physical shared libraries for both
  executables, source-build hash preservation, exact program output and packaged
  hashes. Absolute/relative original RUNPATH discovery, missing-library failure,
  content collisions, external inputs, hardlinks/tampered copies and existing
  lib/ refusal all pass. This is compiled Linux proof, not the local parser mock.
- Update the component guide, ADR 0094 and Linux bring-up guide with that scoped
  evidence. Retain unfinished whole-app resource/plugin/license/launcher/archive
  and actual Firefly/Designer package gates. Do not publish or advertise a Linux
  app release from the synthetic C/DSO fixture.
- Linux renderer/application source 85557be run 42345846818/job 128323290945
  is now terminal success, including real X11 geometry/clipboard and the relocated
  public Firefly app with live renderer/kernel ownership checks. It validates
  that earlier source, not the later native-bridge/world-policy/TCP refactors.
  Correct ADR 0093's stale pending label using already-proven 19ad113 evidence.
- Optimized/shared-provider macOS run 42345882824 remains authoritatively live
  under its original native/package gates. Keep it; no age/observation restart.

Validation: native component workflow, local guards, formatting and actionlint
pass; 182 site pages and 12,582 local links/resources/fragments pass. No local GUI or loader/patcher invocation occurs.
The original complete specification stays active: full Linux distributions/apps,
Windows desktop and remaining Node/Electron/OS/platform work are unfinished.
Goal active.

## 2026-11-07: Share pinned native dependency notices for both package backends

- Previous goal turn delivers real progress: ELF closure staging is committed
  and passes its compiled Linux relocation/deduplication/refusal gate. Continue
  toward whole-app packaging, including the mandatory dependency notices.
- Move the existing macOS notice/cache helpers to native_package/notices.py.
  Preserve the macOS import names and its two-SDK behavior, including strict
  embedded-JS notice hashes and Rust source fallback archives. Linux can pass
  the one combined runtime SDK without duplicating its vcpkg notice inventory.
  No downloaded/generated notice text or upstream source is rewritten.
- Existing runtime-notice manifest/distribution/tamper tests pass through the
  macOS compatibility imports. Run the actual shared collector against the
  current renderer SDK: project/Ladybird, embedded-JS/vcpkg and 350 pinned Rust
  package inventories are collected, with the expected single SDK notice tree.
  This is notice-data validation, not a new native app/platform acceptance.

Validation: Python syntax, notice tests and actual offline pinned inventory pass;
formatting and diff checks pass. No GUI runs. Linux system-library
notices, launcher, payload/archive staging and real packaged-app gates remain
required; optional macOS profile and latest Linux graph remain under their own
native CI. Goal active.

## 2026-11-07: Stage experimental complete Linux app candidates

- Add native package_linux.py: require actual Release Firefly and separately built
  standalone Mite; copy application, physical pinned helpers/resources, dynamic Qt
  plugins, dependency notices and one shared ELF closure. Default to production
  npm payload and stripped symbols; record hashes/provenance and create an optional
  tar archive/checksum. Reuse locked npm selection and shared notices helpers.
- Reject the trial idea of deriving a CLI from the full renderer SDK: local native
  linkage inspection proves upstream LibJS merges into LibWeb there. Remove that
  trial target/output, keep the original standalone Mite build untouched, require
  the standalone root project in CI and reject graphical/alternate-engine CLI
  dependencies. The app archive contains embedded Mite through Firefly, without
  an unused second CLI/engine. Ladybird is unchanged.
- Add native /proc/self/exe launcher with argument forwarding, exec/PID ownership
  and explicit caller platform preservation. Collect native dpkg copyright/version
  records for copied distribution DSOs and preserve referenced common licenses.
- Keep existing renderer/isolation/window/raw-app gates and add independent archive
  extraction/checksum/ELF verification plus real app/renderer/kernel checks. Inspect
  live main/renderer ELF mappings for fallback into either SDK. Add compiled Linux
  launcher/provenance fixture to the independent component workflow.
- Document exact staging inputs/layout/modes, host ABI/X11/driver requirements and
  unfinished native package/Designer/release acceptance. These are candidates;
  no Linux release asset or new platform acceptance is claimed before CI succeeds.

Validation: local payload (3), npm selection (2), notice/tamper and ELF guard/parser
checks pass without GUI. Native launcher/distribution and whole-app archive gates
are added for hosted Linux; their result is pending. Source/style/upstream/site
checks recorded with this commit. No local GUI tests. Goal active.

## 2026-11-07: Record hosted launcher and current refactor acceptance

- Commit/push da5e3e8 adds the experimental Linux app packager. Independent native
  component run 42349719941/job 128335067273 now succeeds: real compiled ELF closure,
  relocation/deduplication/refusals, three payload tests and real C++ launcher
  forwarding/PID/Unicode/platform behavior plus actual dpkg copyright provenance.
  This is component proof; whole-app archive/Designer acceptance is still pending.
- Linux renderer/app run 42348529660/job 128333001604 at 8dc5830 succeeds under all
  unchanged native engine/isolation/helper/window/raw-app and live kernel gates.
  It covers the native bridge/world-job/TCP refactors. Earlier source 209f64e
  run 42348028641/job 128330299443 failed its first headless page with only
  "Firefly WebContent crashed"; runtime sources are identical between those commits.
  Retain the unexplained failure, do not claim a diagnosed fix or relax acceptance.
- Optional size-profile/shared-OpenSSL macOS run 42345882824/job 128323403936 at
  85557be succeeds through 65 CLI suites, native desktop/isolation/IPC/crash/budget
  gates, Designer editing/Monaco/window controls, relocation and package audits.
  Alpha publication was disabled. Artifact 13564569254 is a verified workflow
  output for that source, not current master or a replacement of published alpha2.
  Downloaded artifact digest a90b2d8f...e91cf60 and all inner manifest checksums
  verify. BUILD metadata confirms clean 85557be, Release/-Oz/shared provider.
  Actual uncompressed framework/Designer: 205,745,925/241,516,362 bytes
  (196.21/230.33 MiB); ZIPs 98,262,228/110,255,070 bytes. Update size guide.

Validation: full source/style/actionlint/diff/upstream checks pass and 183 generated
site pages/12,656 links/resources/fragments pass for the preceding implementation.
No local GUI tests. Whole native Linux archives and the complete Windows desktop
remain unfinished. Goal active.

## 2026-11-07: Gate real Linux Designer archive candidates

- Previous goal turn is progress: native Linux packager/component evidence and
  optimized macOS artifact measurements are committed/pushed with exact scope.
  Recheck hosted da5e3e8 renderer run 42349719779/job 128335356488: it remains
  authoritatively in progress compiling the native graph; do not restart/cancel.
- Add test_linux_designer_package.py, requiring native Linux x64 and explicit
  dedicated X11 opt-in before staging. Require the actual compiled/bundled
  production Designer, archive/extract/checksum/ELF-verify its complete native
  package, then invoke the unchanged shared Designer acceptance driver on that
  extracted app/launcher under independently observed Xvfb/Openbox readiness.
- Retain actual asset/worker/server-security, editing/round-trip, Monaco, missing
  ambient privileges, HTML window controls, native minimize and close assertions.
  Do not replace native acceptance with a compiled launcher or server-only probe.
- Extend the renderer workflow with the pinned production Designer build and this
  required gate. Only success preserves a versioned candidate tar/checksum and
  original BUILD metadata for Actions upload. Failure keeps diagnostic logs;
  missing candidate files fail upload. No automatic Linux release publication.
- Parameterize shared archive helper version while preserving the existing runtime
  fixture default. Document native commands, candidate scope and unfinished native
  Designer/distribution acceptance. Ladybird/runtime sources are unchanged.

Validation: Python syntax/help, full source formatting, actionlint/diff, payload
(3), ELF guard/parser and unchanged-upstream checks pass without GUI. Generated
site: 183 pages and 12,660 links/resources/fragments pass. Native Designer archive
acceptance remains pending on hosted Linux. Goal active.

## 2026-11-07: Add an independently testable native Linux display provider

- Previous goal turn is progress: 9f79375 commits/pushes actual extracted Designer
  acceptance and candidate upload gates. Recheck original da5e3e8 renderer job
  128335356488/run 42349719779: authoritatively live compiling its native graph.
  Leave it active, with its archive gate still pending; no observation-age restart.
- Continue required Linux OS services with a Qt/XCB RandR snapshot adapter,
  independent of JS/renderer ownership. Use actual active output XIDs, not invented
  array/hash identities. Match native output names strictly, refuse ambiguous or
  missing mappings/resource races/reflections, translate rotation direction and
  read primary-relative DIP geometry/work area, scale, refresh and cursor.
- Require the native xcb GUI application thread. Return explicit unavailable
  records outside it; omit unverified panel/color/depth properties. Do not wire
  or advertise public Linux screen until the component and app integration pass.
- Add a native read-only probe and standalone service CTest gates: actual no-GUI
  refusal, independent xrandr output identity/geometry, EWMH work area and Xlib
  pointer comparisons at Qt scale 1 and 2 in isolated Xvfb/Openbox. The provider
  never creates a window or warps the pointer. CI's independent manager control
  remains required before native reads; do not substitute a startup sleep.
- Add ADR 0095 with language/boundary/identity/error choices and exact remaining
  public API, events, physical/mixed-DPI/rotation/hotplug/Wayland acceptance work.
  Keep Ladybird unchanged and existing public Linux service refusals intact.

Validation: Python syntax/help, full source formatting (142 Python, 569 JS/config,
193 native, 26 CMake), actionlint/diff and unchanged-upstream checks pass. Generated
site: 184 pages and 12,728 links/resources/fragments pass. No local GUI. Native X11
compilation/acceptance is pending on hosted Linux; no macOS stand-in result is
claimed. Goal active.

## 2026-11-07: Enforce a portable Linux CPU policy and retain SIGILL diagnostics

- Native run 42349719779/job 128335356488 at da5e3e8 is terminal failure: actual
  engine/realm/helper/window gates pass, but the relocated main app exits -4
  (SIGILL) before stdout/stderr/control observation. Required package gates remain
  skipped. Do not claim archive acceptance or silently retry/weaken the app gate.
- Inspect the real upstream compile policy and our builder: Linux renderer had
  ENABLE_CI_BASELINE_CPU=OFF, selecting -march=native and Rust target-cpu=native.
  Correct this unsuitable distribution contract by enabling the existing pinned
  x86-64-v2 C++/Rust policy in renderer/package-Mite builds, without upstream edits.
  This is a concrete portability fix, not proof of the unexplained SIGILL cause.
- Extend --baseline-cpu to Linux x64 and verify generated C++/Cargo commands;
  preserve the existing Apple M1 behavior. Add refusal tests for native/missing/
  conflicting CPU flags, wrong Rust CPU, lost fatal warnings and disabled policy.
  Make Linux packaging require both SDK policies and record its baseline metadata.
- Record actual CI CPU features and bounded core files for app acceptance. On
  failure report registers/current-PC disassembly/stacks with GDB and no rerun.
  Preserve the original failure; no sandbox, parent-PID or isolation assertion is
  disabled. New native evidence remains required before claiming this fixes SIGILL.
- Separately pushed 3936884 adds the native display component; its standalone
  services run 42350712038/job 128338038590 is live configuring/compiling. Keep that
  independent gate and the latest renderer run active; no observation-age restart.

Validation: CPU refusal tests (1 multi-case suite), payload tests (3), Python syntax,
full source formatting/actionlint/diff and unchanged-upstream checks pass. Generated
site: 184 pages and 12,730 links/resources/fragments pass. No local GUI. Actual
native baseline/app/archive acceptance remains pending. Goal active.

## 2026-11-07: Accept the pinned Linux Rust frame-pointer flag in CPU auditing

- Previous goal turn is progress: native display provider and portable CPU policy
  are committed/pushed with native acceptance explicitly pending.
- Source 0a7bd26 renderer run 42351084931/job 128339161550 stops during configuration:
  the generated Cargo command correctly contains -Dwarnings and x86-64-v2, plus
  upstream's Linux-only -Cforce-frame-pointers=yes. Our new checker incorrectly
  rejects that diagnostic flag; no native app/package gate has run on this source.
- Inspect pinned compile_options.cmake/rust_crate.cmake and accept only the exact
  optional frame-pointer=yes suffix. Keep strict CPU/warning validation. Add both
  positive variants and negative alternate CPU/frame-pointer/warning cases.
- Do not remove upstream frame pointers, relax CPU matching or call this a SIGILL
  fix. Keep the original native display service job 128338038590 active; it remains
  authoritatively compiling. Public screen integration is separate ongoing work.

Validation: CPU policy multi-case test, Python formatting and diff checks pass
without GUI. Hosted generated-command and native app/package acceptance remain
pending. Goal active.

## 2026-11-07: Wire public Linux screen queries without weakening native acceptance

- Bind the real RandR/Qt provider through the shared screen host and route native
  unavailable records to ERR_FIREFLY_DISPLAY_UNAVAILABLE JS exceptions. Reuse the
  existing owner-scoped Qt display watcher/VM notification path. Move notification
  implementation into a separate Qt-only source; retain macOS behavior.
- Separate Linux startup-environment refusal from Linux desktop bindings, so its
  no-GUI probe still compiles on macOS without fake X11 providers or a JS VM.
  Native Linux-only screen bindings link the actual provider and common watchers.
- Replace the Linux app fixture's old screen-unsupported assertion with real public
  readiness, enumeration, primary/cursor/matching/nearest and independent-copy
  checks. Its live peer independently validates native output identity, DIP bounds,
  work area and cursor using shared Xrandr/EWMH/Xlib observations. Preserve every
  actual renderer/parent/kernel/preload/IPC check, explicit unsupported conversions
  and unavailable nativeTheme behavior; archive execution uses the same app gate.
- Update screen contract, matrix and ADR with implemented-versus-pending scope,
  native identity/error differences, omitted fields and unverified physical/events
  coverage. User requests a broader documentation audit; that follows separately.

Validation: actual macOS Firefly/environment targets rebuild/link, native no-GUI
startup refusal and actual Node/Mite screen policy pass; Linux fixture stages and
syntax-checks without GUI. Upstream pin unchanged. Native Linux provider/public
app/archives remain pending; baseline checker fix is separately committed 108a32f.
Goal active.

## 2026-11-07: Refresh user-facing example, API and platform documentation from actual evidence

- User asks whether every Electron example works and whether API docs are current.
  Inspect the actual catalog/runner/API implementation and today's upstream examples
  overview: 21 topics plus quick-start/lifecycle = 23 original Elytra apps, not all
  Electron Fiddles. Completed macOS run 42345882824/job 128323403936 at 85557be
  confirms six shared native smoke paths, one explicit manual dark-mode gate and
  16 blocked topics. Preserve that limited scope, not full feature acceptance.
- Fetch actual completed native Mite logs at da5e3e8: Windows run 42349719866/job
  128335237044 passes all 65 suites (91.95s); Linux run 42349719739/job 128335066258
  passes all 65 (60.19s). Root CTest inventory includes Fetch/body/TLS/HTTPS/HMAC
  and the documented I/O/event suites. These default Release runs do not prove
  the newer portable-CPU build, Linux archive/Designer or new screen integration.
- Add a prominently linked current-support page and docs navigation: exact platform
  evidence, six/one/16 example breakdown, implemented API families, missing services,
  published Alpha 2 versus newer optimized Actions artifacts and pending master gates.
- Correct stale unavailable claims for HTTP/HTTPS/TLS clients, Fetch, spawn/execFile
  and clipboard in migration/baseline guides; update outdated 64-suite/platform-
  pending API summaries and matrices. Preserve specific unsupported options,
  hardware/provider/permission gaps and historical ADR/progress evidence.
- Refresh README, getting-started, API/runtime/desktop overviews, example catalog,
  roadmap and detailed client/crypto/body/process references. Do not advance the
  unaccepted Linux screen/package or absent Windows desktop to tested status.
  Newly pushed fa5aef5 wires public X11 screen with its own pending native gates.

Validation: no GUI runs. Full source formatting, actionlint/diff, unchanged-upstream
and reproducible runtime/Firefly adapter checks pass. Catalog has exactly six
partial smoke entries, one manual partial and 16 unsupported entries. Generated
site: 185 pages and 13,039 links/resources/fragments pass. Native latest profile/
archive/screen acceptance remains pending; original goal stays active.

## 2026-11-07: Start the required native Windows renderer acceptance path

- Previous goal turn is progress: docs/API/example audit is committed/pushed at
  92bf486, and Pages build/deploy run 42352011055 succeeds. Full product scope is
  still incomplete; do not turn that documentation acceptance into runtime support.
- Revalidate actual hosted Linux service and baseline renderer handles: original
  service job 128338038590 and baseline job 128340196083 remain in progress building
  real graphs. Do not restart them; take independent required Windows bring-up work.
- Inspect the actual current Ladybird pin: native clang-cl is experimental, with
  Windows Qt/DirectX sources, manifests and helper/process paths. Its WSL browser
  recommendation is not our allowed fallback. Native Mite CLI success does not
  establish a renderer/window/sandbox implementation.
- Add an independent native Windows x64 renderer workflow using the pinned native
  toolchain/bootstrap/upstream graph. Build actual LibWebView/helpers and owned
  headless/realm/wire probes. Apply the upstream console/runtime-DLL deployment
  helper to owned Windows probe targets, leaving vendor bytes unchanged.
- Require real DOM/absent ambient Node/PID separation/640x400 PNG and existing actual
  realm/GC/callback/separate-process wire tests. Keep Windows process/sandbox checks
  explicitly UNVERIFIED and production desktop/preload/window target guards intact.
  Preserve actual diagnostic/build/dependency artifacts and fail missing/failed gates.
- Document ADR 0096, native commands and remaining window/process/token/sandbox,
  main-Mite, services, Designer and package work. This is native bring-up, not a
  substitute for the full required Windows application or a release acceptance.

Validation: actionlint, full formatting, diff/upstream checks pass without local GUI.
Generated site: 186 pages and 13,110 links/resources/fragments pass. Actual Windows
graph/renderer acceptance is newly added and execution pending; Linux app/archives/
screen and Windows desktop remain unfinished. Goal active.

## 2026-11-07: Observe native Windows renderer processes independently

- Previous goal turn is progress: 9c5a9a9 adds actual native renderer/isolation/wire
  build gates and the Windows console/DLL adapter. Recheck run 42352483937/job
  128343398086: live configuring the real graph. Linux service/baseline jobs remain
  live compiling too; do not restart/cancel on observation age.
- Inspect pinned WebContent source selection: Windows uses the empty
  RendererSandboxUnimplemented::apply_sandbox. Record that concrete missing
  security implementation instead of assuming a job/token flag means containment.
- Add an independent query-only Win32 peer: Toolhelp native parent/child snapshot,
  full process image, liveness/creation time, primary-token integrity/AppContainer/
  restrictions and job membership. Preserve native error behavior and known-exit
  races; request no debug/memory/mutation permissions and never fabricate records.
- Replace the renderer runner's entirely skipped Windows process stage with actual
  manual-page process observation. Require SDK WebContent/RequestServer/Compositor
  images, native parent relation/creation ordering and agreement with the reported
  renderer PID. Retain raw token/job observations and logs while keeping Windows
  sandbox containment explicitly unimplemented/unverified.
- Add a fast independent native Windows observer workflow plus the same component
  gate in renderer CI. Its real owned Python child checks process/parent/image/
  creation/exit/invalid-PID behavior; it is not a substitute for LibWeb acceptance.
  Non-Windows refusal runs before loading native APIs. Document exact boundaries.

Validation: Python syntax, unsupported-host guard, full formatting/actionlint/
diff and unchanged-upstream checks pass without local GUI. Generated site: 186
pages and 13,112 links/resources/fragments pass. Actual native observer component
and integrated Windows renderer process acceptance are pending. Complete Windows
sandbox/desktop/Designer/packages and remaining Linux gates stay unfinished.
Goal active.

## 2026-11-07: Correct the native TokenHasRestrictions observation ABI

- Fast independent observer run 42353077365/job 128345176621 at a2775a2 fails on
  the real Windows primary-token query before any child/renderer acceptance:
  our generic DWORD decoder incorrectly handles TokenHasRestrictions (class 21).
- Inspect the recorded failure and Microsoft win32metadata discussion 2308:
  maintainer guidance specifies BOOLEAN at a DWORD-aligned address, correcting
  older DWORD documentation. Add a class-specific aligned four-byte allocation,
  require the successful one-byte return length and decode only byte zero.
- Keep all other scalar classes strictly DWORD-sized, retain API failures and
  unexpected-length refusal, and improve mismatch diagnostics. Do not reinterpret
  untouched tail bytes or globally relax scalar checking. This fixes our observer,
  not the unimplemented Windows renderer sandbox or an application API.

Validation: Python syntax/unsupported-host guard/formatting/diff pass locally
without native Windows calls or GUI. Fresh native observer acceptance is required.
Original Windows/Windows-renderer and Linux native builds remain under their own
gates. Goal active.

## 2026-11-07: Prove the native Windows observer and isolate CI formatter state

- Corrected native component run 42353208189/job 128345576075 at 2498e71 succeeds
  on Windows x64: real current/owned child image, parent PID, creation/liveness,
  primary-token integrity/AppContainer/restriction/job queries, invalid PID and
  already-exited child refusal. The class-21 decoder requires its actual one-byte
  payload; this is native evidence, not the local unsupported-host guard.
- Keep scope precise: the native Python child proves observer mechanics, not the
  separate LibWeb renderer process stage, Windows containment or desktop support.
  Record scoped component acceptance in ADR 0096; original long native jobs remain
  under their own gates.
- Style run 42353208155/job 128345575814 fails before checking sources: pinned
  Rust installation reports a partially installed toolchain and cargo-clippy file
  conflict, despite the existing fixed per-job RUSTUP_HOME path. Allocate a unique
  empty mktemp directory under RUNNER_TEMP instead of reusing that fixed location.
  Retain pinned Rust 1.98.0, rustfmt/clippy and every actual source/adapter check.
  Do not remove components or convert installation failure into success.

Validation: full formatting/actionlint/diff checks pass locally without GUI;
generated site: 186 pages and 13,114 links/resources/fragments pass. Native
observer component passes; refreshed style bootstrap and Windows/Linux renderer,
screen/archives/Designer integration still require their actual hosted results.
Goal active.

## 2026-11-07: Add the native Windows Qt/DirectX window prototype gate

- Previous goal turn is progress: native Windows process observations pass their
  real component gate after the class-21 ABI correction; fresh isolated formatter
  run 42353402489/job 128346169011 also succeeds. Full product scope stays active.
- Revalidate original native Windows renderer, Linux services and baseline renderer
  jobs: they remain authoritatively active configuring/compiling. Do not restart
  them merely because the large graphs take time; continue required presentation.
- Select an owned Windows.cmake adapter for the curated Qt view, requiring pinned
  HAS_DIRECTX and upstream's unchanged DirectX view plus d3d11. Permit only the
  internal native window probe; keep production Windows app/preload guards intact.
  Apply upstream console/DLL setup and retain its native Qt plugin SDK deployment.
- Extend the native renderer workflow after all existing headless/realm/wire gates
  with actual window input/resize/navigation/close and painted PNG assertions.
  Require QPA windows, so no offscreen replacement can satisfy native acceptance.
- Add Windows-native bounded pipe readiness (no selectors/reader-thread races),
  independent visible-style HWND/owner/client-size observation, and verified owned
  WebContent termination using actual image/parent/creation identity on a held
  process handle. Require the host's existing renderer-loss failure assertion.
- Extend the fast native observer component with real owned Python-pipe readiness
  and silent-timeout cases, explicitly not a LibWeb/window substitute. Preserve the
  original POSIX process-control branch and all existing assertions. Add ADR 0097
  with exact Qt-event/physical-display/security/production integration limits.

Validation: syntax, unsupported-host guard, actionlint/diff and full formatting
checks pass. The existing macOS window target configures/builds/links without GUI
execution. Generated site: 187 pages and 13,185 links/resources/fragments pass.
Actual Windows window/protocol acceptance is pending. Complete sandbox/main-Mite/Designer/packages and remaining Linux gates
are unfinished. Goal active.

## 2026-11-07: Record native Linux display and Windows pipe component acceptance

- Original Linux native-service run 42350712038/job 128338038590 at 3936884 completes
  successfully: all nine native OS-service tests pass (1.62s), including actual
  no-GUI display refusal and independent RandR/EWMH/Xlib scale-1/2 snapshots.
  This validates the real Linux provider, not the later public VM binding/app gate.
  Update screen contract, ADR 0095 and Linux guide with that exact evidence.
- Windows observer component run 42354246345/job 128348719912 at 3c8999e succeeds:
  native process/parent/image/creation/token/job observations, invalid/exited PID
  refusal, actual pipe readiness, bounded silent timeout and one-MiB output budget.
  Its native Python peers prove protocol/control mechanics only; the actual Qt
  window/HWND/WebContent-loss and production sandbox are still unaccepted.
- Record scoped protocol proof in ADR 0097. Do not turn a successful component
  into a general Windows desktop, physical-display or Linux distribution claim.
  Original long Windows renderer/Linux baseline jobs remain under their own gates.

Validation: native component results and local diff/upstream checks pass without
GUI. Generated site: 187 pages and 13,187 links/resources/fragments pass. Full Windows/Linux renderer/app/archive/Designer
integration remains pending; goal active.

## 2026-11-07: Refresh user-facing support and example status against completed CI

- Recheck the current official Electron overview and source catalog: 21 topics
  plus quick-start/lifecycle, 23 original apps; six shared native smoke slices,
  one manual/partial dark-mode example and 16 blocked topics. Do not describe
  rejection tests or checked-in examples as working feature acceptance.
- Inspect completed macOS Release run 42348529659/job 128334840985 at 8dc5830:
  success, including the same six shared example assertions, explicit remaining
  example gates, native preload/IPC and packaged Designer edit/Monaco/HTML controls.
  Record this newer evidence separately from published Alpha 2 and the optimized
  85557be artifact. Original Linux baseline and Windows renderer jobs remain live;
  neither is promoted to accepted desktop/package support.
- Correct stale execFile/hosted-platform claims in the Mite overview and generic
  Windows-unverified claims in crypto/OS matrix rows while preserving untested
  option/provider/permission/error limitations. Mark earlier isolated-world bridge
  notes as historical and replace the obsolete latest-build desktop-lock summary
  with completed hosted evidence. Keep documentation-depth gaps explicit.
- Preserve unfinished Windows sandbox implementation changes for the next code
  slice; this commit only updates support documentation and evidence.

Validation: source catalog counts agree with native example logs; site build and
all 187 pages / 13,191 local links/resources/fragments pass, as does diff whitespace.
No local GUI tests. Full Node/Electron parity and Windows/Linux desktop/package
completion remain unfinished. Goal active.

## 2026-11-07: Add a native Windows AppContainer launch/security component

- Previous turn made progress: refreshed API/example support evidence, committed
  and pushed bfdfdad. Resume the unfinished native Windows launch boundary without
  enabling the guarded production desktop runtime or modifying Ladybird.
- Add a standalone C++20 Win32 launcher: unique random AppContainer profile, zero
  capabilities, explicit inherited handle/std-stream list, clean environment,
  absolute executable/cwd and CRT argument quoting. Create suspended with atomic
  JOB_LIST assignment, verified one-process/kill-on-close job policy; one explicit
  resume, fail-closed ownership and synchronized active-child profile lifetime.
- Add a static-CRT native broker/worker fixture and independent Python observer.
  Require actual image/parent/token/zero-capability/low-integrity/job facts before
  resume; allowed file read, denied private read/write/handle/network/child launch,
  omitted ambient secret, malformed-contract rejection and removed-profile refusal.
  Kill the broker before and after actual child execution and require the exact
  held native child object to terminate. Restrict fixture ACL changes to its fresh
  tree, with no broad package grants, firewall or loopback exemptions.
- Add a fast Windows-only CI workflow independent of LibWeb/vcpkg/GUI builds.
  Keep ordinary AppContainer limitations and missing actual renderer IPC/resource/
  helper integration explicit in ADR 0098 and the Windows guide. Component native
  acceptance is pending; Windows desktop/Designer/packages are still unfinished.

Validation: Python syntax/help, full pinned source formatting, actionlint and diff
checks pass locally; generated site 188 pages / 13,261 links/resources/fragments.
No native Windows result or local GUI pass is claimed. Goal active.

## 2026-11-07: Supply the native AppContainer startup known folder

- First actual Windows component run 42356382899/job 128355278810 at 00d1cb2
  compiles the static broker/worker successfully, then fails before readiness.
  Retrieve the preserved raw fixture artifact: CreateProcessW AppContainer fails
  with 203 (ERROR_ENVVAR_NOT_FOUND). No token/denied-operation acceptance yet.
- The Windows AppContainer launch guide describes LocalAppData profile setup.
  Add the current user's native FOLDERID_LocalAppData through SHGetKnownFolderPath
  to the otherwise fixed environment; free its allocation on success/failure.
  Do not inherit the whole environment or relax token/handle/job/network policy.
  A fresh native run must establish whether this resolves the startup failure.
- Print bounded broker stderr into CI as well as preserving raw artifacts so future
  pre-readiness failures retain their actual native operation/error directly.
- Independent process observer run 42356382902/job 128355278917 and full source-style
  run 42356382934/job 128355278811 pass at 00d1cb2; neither proves AppContainer or
  full Windows renderer acceptance.

Validation: syntax/formatting/diff pass locally without GUI. Actual AppContainer
launch and all security negatives remain required. Goal active.

## 2026-11-07: Handle strict invalid-handle rejection in the negative fixture

- Native run 42356570628/job 128355846058 at 25d5230 compiles and passes startup,
  independent child/parent/image and AppContainer/zero-capability/low-integrity/job
  checks before resume. The native known-folder addition resolves error 203.
- After resume, the worker exits with 0xC0000008 (STATUS_INVALID_HANDLE), without
  completing its operation report. The negative fixture deliberately references an
  excluded private handle; Windows strict-handle policy may raise this exception
  instead of returning ERROR_INVALID_HANDLE. Isolate that probe in a small SEH
  boundary, accepting only that exact exception at the deliberate negative access.
  Unexpected exceptions elsewhere remain fatal. Do not disable strict handling.
- Retain actual readable-secret failure, explicit invalid/access-denied read errors
  and all other file/network/process checks. A fresh native run must prove the
  complete negative report and pre/post-execution broker-death cases; token setup
  alone is not component or renderer containment acceptance.

Validation: native source formatting and diff checks pass locally; no GUI. Full
native security fixture acceptance remains pending. Goal active.

## 2026-11-07: Diagnose the remaining native Windows network refusal gate

- Actual component run 42356707395/job 128356259732 at e30955a reaches the worker
  report: allowedRead/privateReadDenied/writeDenied/handleIsolated/cleanEnvironment/
  childSpawnDenied are true; networkDenied is false. The narrow strict-handle
  negative-test boundary resolves the earlier fatal exit without disabling policy.
- Preserve strict WSAEACCES acceptance but report the precise Winsock operation and
  native error (startup/socket/nonblocking/connect/poll/SO_ERROR), rather than an
  opaque false. Query failures and timeouts are still failures, not denied-access
  evidence. Preserve the independently live listener and no-connection assertion.
- The malformed-contract and suspended-child token assertions also run before the
  reported failure. Broker-death tests remain unexecuted until the checks mode
  succeeds; no complete component/renderer containment claim is made.

Validation: source formatting, Python syntax and diff checks pass. No local GUI.
A fresh native diagnostic result is required. Goal active.

## 2026-11-07: Add requested HTTP/2 and HTTP/3 future work

- Preserve the user's requested future scope in the delivery roadmap, explicitly
  below current platform/Designer/distribution priorities. Cover native HTTP/2
  sessions/streams and Node-style APIs, HTTP/3 over QUIC/TLS, client/server slices,
  shared Fetch transport negotiation and documented fallback boundaries.
- Require maintained pinned dependencies, standalone Mite separation, independent
  protocol peers, native cross-platform lifecycle/security/interoperability gates
  and detailed contracts as each slice becomes real. Defer HTTP/3 API selection
  until its then-current Node/standards baseline is verified.
- Link the plan from HTTP, HTTPS and Fetch references. Keep today's HTTP/1 scope
  explicit; no HTTP/2, HTTP/3 or node:http3 implementation claim is introduced.

Validation: generated site and local links/anchors pass; diff whitespace clean.
Documentation only, no local GUI or runtime changes. Windows work remains current
priority and the full goal remains active.

## 2026-11-07: Fix native Windows callback-adapter include selection

- Revalidate the original long native jobs: Windows run 42352483937/job
  128343398086 is now terminal failure, not a live wait. Its actual clang-cl
  command fails at CustomEventAdapter.cpp: serialization/EventValues.h missing.
  The command has generated-adapter includes but lacks our renderer-bridge root.
- The source-selection condition treated FIREFLY_SOURCE_DIR as a regex. Native
  Windows backslashes (and checkout metacharacters) therefore suppress the literal
  owned-path match. Replace it with string(FIND) prefix comparison, preserving the
  per-source include scope and upstream source list. No Ladybird changes.
- Original Linux baseline run 42351420391/job 128340196083 is terminal as well:
  actual renderer/app gates progress to archive staging, which refuses conflicting
  libdbus-1.so.3 contents. Preserve this correct refusal for subsequent dependency
  provenance diagnosis; do not silently choose a different library.

Validation: actual CMake adapter include block configures with native Windows
backslashes and regex-metacharacter checkout paths, requires the owned include
root and verifies upstream sources do not gain it; source formatting/diff pass.
Fresh native Windows renderer compilation remains required. Goal active.

## 2026-11-07: Prioritize actual Code-OSS compatibility and efficient file transport

- User explicitly prioritizes making VS Code run on Firefly/Mite. Update roadmap
  ordering so general Node/Electron API work follows that application's critical
  path: bootstrap, required preload/event/binary/port IPC, workspace files/watchers,
  auxiliary/extension hosts and native dependencies. Preserve the entire platform/
  Designer/security/distribution objective; HTTP/2 and HTTP/3 remain later work.
- Audit Microsoft Code-OSS at immutable 6531648f5ace8b77ae799262bdc97bf048461938
  (manifest dev 1.146.0, Electron 43.8.6), distinct from our Electron API oracle.
  Inspect bootstrap/mainImpl, compile-cache imports, preload, MessagePort acquisition
  and binary protocol, file-provider client/server and runtime dependencies.
- Record confirmed blockers and evidence-gated milestones in compatibility/vscode.md.
  Existing standalone local Mite explicitly rejects ESM imports of node:module,
  node:v8, node:perf_hooks and original-fs. This is a small bootstrap-import probe,
  not actual Code-OSS execution or current-master full acceptance. Do not fake V8
  caches or use a Node/V8 host to bypass the LibJS requirement.
- Add file-transfer guide: current IPC is plain-data only, 128 KiB wire budget;
  binary buffers/ports/Blob/File unsupported. Prioritize actual binary snapshots,
  compatible port/stream transport, authorized lazy file access and then measured
  ownership/shared-storage optimization. Electron's documented DOM File IPC
  restrictions and path metadata are distinct from file-byte transfer. Inspected
  Code-OSS port sender has no transfer list; do not infer zero-copy performance.
- Define content/revocation/pressure/crash safety and comparable Electron/Firefly
  latency/CPU/memory/copy-count benchmarks, including large streamed files. Link
  the audited target/plan from docs home, migration, roadmap and IPC reference.

Validation: source references pinned/read, four actual standalone non-GUI import
probes, site build and 190 pages / 13,415 links/resources/fragments pass; diff clean.
No Code-OSS launch, complete dependency inventory, percent/time estimate or new
file-transfer support claimed. No local GUI. Goal active.

## 2026-11-07: Implement bounded native binary snapshots for the Code-OSS path

- Previous turn made progress: prioritized/audited Code-OSS, committed and pushed
  dffa4c6. Follow that critical path with real binary transport implementation,
  while retaining the entire native platform/security/Designer objective.
- Inspect current wire/native IPC routes: existing data goes through JSON/BiDi and
  main JS text fields, so arbitrary binary cannot safely be enabled by merely
  permitting another JS type. Add a strictly opt-in binary byte budget (default
  zero) to the existing native graph; public IPC/contextBridge remain unchanged.
- Capture exact source-realm fixed/attached/nonshared ArrayBuffer bytes into owned
  snapshots, preserving graph aliases/cycles while ignoring hostile prototype/
  expando hooks. Materialize only into explicit mutable destination-owned buffers;
  reject frozen publication rather than pretending backing bytes become immutable.
- Encode native FFB2 frames with bounded version-2 JSON graph metadata and exact
  raw attachments, never base64/JSON byte arrays. Validate lengths/schema/tags,
  aggregate bytes/reachability/trailing data and explicit policy before admission.
  Retain version-1 plain graphs, existing graph limits and one-MiB full wire bound.
- Add actual source/destination realm/GC/mutation/default/frozen/malformed/boundary
  tests and encoder-exit/fresh-decoder process tests. An independent Python peer
  checks all 65,536 raw bytes and supplies its own canonical graph/frame. Preserve
  Windows native binary stdin/stdout mode for new arbitrary-byte fixtures.
- Add ADR 0099 and update Code-OSS/file guides with exact internal acceptance scope.
  Typed-array/Buffer/view semantics, public binary IPC, port handoff, streaming and
  lazy file capabilities still require implementation and actual acceptance.

Validation: native macOS wire target compiles/links and all existing/new wire and
separate-process tests pass, including independent raw-byte/frame checks. Rebuild
and run the native non-GUI isolation probe: existing bridge/callback/GC/revocation
regressions pass. Full pinned formatting, Python syntax, diff/upstream checks pass;
site 191 pages / 13,488 local links/resources/fragments pass. No local GUI. Native
Linux/Windows execution of this new slice remains pending; no Code-OSS/public
binary-transfer/zero-copy support claim. Goal active.

## 2026-11-07: Preserve typed-array/DataView geometry and backing graphs

- Previous goal turn made concrete progress: native ArrayBuffer snapshots/raw wire
  frames at 8e563af pass real cross-process tests. Continue the prioritized Code-OSS
  binary path without enabling the still-text-based public IPC transport.
- Capture native typed-array/DataView slots without getters, constructors, species,
  iterators or custom prototype hooks. Preserve byte offsets, lengths, repeated-view
  identity and backing references. A fixed table maps all twelve pinned typed kinds
  to stable private tags, independently of upstream enum numbering.
- Snapshot each complete backing buffer once and charge its full byte length,
  including for tiny selected views. Rebuild backing storage before all views so
  forward references and overlapping destination views work. Normalize Uint8Array
  subclasses to native Uint8Array; retain default/frozen publication refusal.
- Extend FFB2 metadata with typed/DataView references and geometry. Validate backing
  type/reachability/depth, kind, alignment and range before native factories. Reject
  detached/shared/resizable backing, unknown/dangling/self refs, misalignment,
  fractional/negative lengths and zero-length views beyond the backing end.
- Add actual native realm/GC/mutation/hostile-subclass tests for all twelve kinds and
  DataView, including a real native shared-buffer negative control. Extend producer-
  exit/fresh-consumer tests and the independent Python peer with Uint8/Uint16/
  DataView/empty-tail graphs and forward backing references. The payload remains
  one exact 65,536-byte attachment, not one copy per view.
- Update ADR 0099 and Code-OSS/file transport docs: cloning exposes the complete
  backing storage; a subarray is not an access restriction. Actual Mite Buffer/
  main-process integration, public binary transport/ports and platform acceptance
  remain separate work. No public support or zero-copy/performance claim.

Validation: native macOS wire target compiles/links; original/plain/buffer/new-view
wire and separate-process tests pass, including independent raw bytes and forward
refs. Rebuilt native non-GUI isolation probe and existing callback/GC/revocation
regressions pass. Full pinned formatting/Python syntax/diff checks pass; generated
site 191 pages / 13,489 local links/resources/fragments passes. No local GUI.
Native Linux/Windows acceptance of this new view slice remains pending. Full goal
active; Code-OSS does not run yet.

## 2026-11-07: Implement a private native byte channel and real child-process gate

- Previous turn made progress: native typed/DataView snapshots at 3c86eed pass real
  graph/process tests. Continue the Code-OSS critical path with actual byte delivery,
  preserving the full original platform/security/Designer objective.
- Add an owned LibIPC/LibCore/AK-only channel over the pinned native transport, with
  FFBC fixed header/raw payload, 128-KiB packet maximum, sixteen send/receive slots,
  payload-byte accounting and delivery ACK credit. Return capacity errors explicitly;
  reject replay/unknown ACK/header/length/attachment violations and revoke on EOF.
- Keep native received bytes/mapping alive during borrowed callbacks; no additional
  receive payload copy or text encoding. Dispatch on the Core owner thread, preserve
  FIFO through nested pumping and count active batches/empty frames against limits.
  Retain callback ownership when callbacks close/release their channel. Request
  fail-instead-of-block native pipe behavior on Windows; no unsandboxed fallback.
- Add actual kernel-transport tests for packet maxima/source mutation, sequence/
  credit reuse, zero-byte receive flood, reentry, unexpected descriptor and peer
  loss. Independent Python validates/constructs exact packet bytes and malformed
  cases. Windows fixture stdio uses binary mode for arbitrary-byte protocol tests.
- Spawn a real native peer: macOS admits the one owned PID once via Mach bootstrap;
  Unix/Windows branches inherit only the child's channel/broker endpoints and
  restore fixture environment. Protect partial launch cleanup, terminate/reap
  failed owned children and bound child bootstrap by a test-only watchdog. Require
  child-reported actual OS PID and all 65,536 bytes, plus an empty message/ACK.
- Add check-firefly-byte-channel to existing wire/isolation CI gates. Document
  exact component scope in ADR 0100 and Code-OSS/file guides. Raw channel handoff
  to actual WebContent, document/realm/epoch authorization/control-plane ordering,
  VM-safe payload routing and public bridge/port support remain required. Upstream
  transport allocations are outside these application-layer queue bounds; this
  is neither a full containment nor zero-copy/performance proof.

Validation: local macOS native component target compiles/links and its complete
Python gate passes, including real parent/child PID/data/ACK and native negative
cases. Link/native dependency inspection has no JS/Web/WebView/Mite/QtWidgets
inputs. Full pinned formatting, syntax/diff and unmodified-upstream checks pass;
site 192 pages / 13,559 local links/resources/fragments pass. No local GUI. New
Linux/Windows paths remain natively unaccepted; public IPC stays plain-data and
Code-OSS still does not run. Goal active.

## 2026-11-07: Transfer the native byte endpoint to real headless WebContent

- Follow the user's Code-OSS priority with an actual renderer transport slice,
  retaining the full platform/Designer/security goal. Add a native trusted endpoint
  handoff over the existing frontend control connection instead of another named
  bootstrap service or a page-visible API.
- Keep vendor sources unchanged. Owned CMake module wrappers preserve upstream
  targets/HSTS behavior and select a fingerprinted IPC-generator adapter for only
  WebContentServer. Independently generate the original protocol, require all 251
  original IDs to remain identical and append exactly one NotRouted message. Fail
  on changed fingerprints, ambiguous seams or a missing generator selection.
- Add one LibIPC hook shared by the generated control stub and helper. Default and
  all-zero-cookie admission close the transferred native handle. Stage its header
  under the existing binary include root; standalone Mite stays independent.
- Register a receiver only in the diagnostic helper, admitting once per session.
  Native PID/cookie handshake and raw byte echo run after the real renderer sandbox
  is installed. Reject duplicate endpoints without replacing the original channel;
  unregister/close on shutdown. Product helper has no receiver.
- Add a real headless LibWeb gate: verify 65,536 exact arbitrary bytes, actual
  renderer PID/cookie and duplicate refusal; require the original endpoint and DOM
  control connection to remain usable. Separate ordinary-helper/zero-cookie
  negatives require peer EOF followed by a successful live-document response,
  rejecting parser disconnect/crash false positives. No privileged page globals.
- Include the gate in macOS release helper checks and Linux renderer workflow.
  Update ADR 0101, upstream-update, Code-OSS and file transport docs. Public binary
  IPC remains disabled: document/realm/epoch binding, ordered revocation, VM-safe
  decoding and compatible event/port/preload integration are the next priority.

Validation: local macOS ordinary/diagnostic/product helper and probe builds pass;
the complete handoff gate passes.
Existing headless isolated-world/helper callback/lifetime controls, native byte
channel/owned-child tests and binary graph/separate-process wire gates pass. Full
pinned formatting, Python syntax/diff and unmodified-upstream checks pass; generated
site 193 pages / 13,633 local links/resources/fragments passes. No
local visible GUI. Linux execution and native Windows renderer integration remain
pending; neither Code-OSS launch nor public binary API support is claimed. Full
goal active.

## 2026-11-07: Add ESM-owned require contexts for the Code-OSS bootstrap

- Previous goal turn made verified progress: bc0afb2 committed/pushed trusted
  native byte handoff and real headless WebContent controls. Continue toward the
  full original specification with Code-OSS prioritized; public binary routing,
  ports, revocation and native platform/package acceptance remain open.
- Inspect the pinned Code-OSS bootstrap-node source: its createRequire(import.meta.url)
  call is a real additional startup dependency. Add node:module createRequire over
  Mite's existing native CommonJS resolver, shared CJS/JSON caches and require
  package conditions. No second evaluator, Node/V8 dependency or altered upstream.
- Extract common module metadata creation. Synthetic parents may name nonexistent
  files or directory contexts but are never executed/cached. Preserve require.main,
  cache deletion/reload, missing-module requireStack and closure-owned context
  lifetime, including retained resolve after actual GC. Validate file URL/native
  bases; reject NUL before native path operations. Keep unsupported require options.
- Add isBuiltin and a frozen sorted builtinModules inventory from the implemented
  registry. Do not advertise reserved unsupported Node names as available; reserved
  names still reject local npm fallback. Keep the default export a documented
  partial utility object, not an imitation Module constructor or compile cache.
- Add actual Node 24.15 comparisons for ESM and CJS entries, special-character file
  URLs, virtual/directory parents, package imports/exports, shared identity, error
  codes and collection. Every listed builtin is imported and required. Add this
  as the 66th standalone CTest gate for native CI; no local visible GUI.
- Refresh the detailed module API, Code-OSS inventory, concepts/matrix/status and
  ADR 0102. Recheck the older Linux/Windows renderer runs: both are terminal failures,
  not still building. Current fixes and new API branches require native evidence.

Validation: standalone macOS Release CLI/embedded host compile and all 66 CTest
suites pass (20.15 seconds), including Node/actual-GC module comparisons. Full
pinned formatting and generated runtime bundle guards pass. New Linux/Windows
module utility execution remains pending. Firefly also compiles/links against the
changed shared runtime; this is a build check, not a new desktop acceptance run.
Python syntax/diff and unmodified-upstream checks pass; generated site 195 pages /
13,772 local links/resources/fragments passes. Code-OSS still does not run: cache/V8,
perf_hooks/original-fs, desktop/process services and public port/binary routing
remain required. Full goal active.

## 2026-11-07: Native performance clock and an actual pinned Code-OSS module gate

- Previous turn made progress: 8f617c0 committed/pushed ESM-owned require contexts
  with all 66 local Mite suites passing. Continue the full goal along the Code-OSS
  startup path; do not count component successes as a desktop workbench launch.
- Inspect mainImpl's performance.timeOrigin use and the actual common/performance.ts
  module at the audited Code-OSS pin. Add global/node:perf_hooks performance with
  native steady-clock now and one fixed native epoch timeOrigin. Reuse the process
  uptime origin; later reads never depend on JS Date or the current OS wall clock.
- Preserve shared global/ESM/CJS identity, cached exports after global replacement,
  read-only origin and modern Node receiver errors, including transparent/revoked
  proxy behavior. No fabricated nodeTiming, timeline entries or profiling results;
  constructors, marks/measures, observers, utilization and histograms remain absent.
- Pin Microsoft's exact performance.ts and MIT license as test-only vendor inputs,
  with revision/paths/SHA-256 manifest. The existing Node reference erases only TS
  types during testing; both Node and Mite execute that same module. Require actual
  native-origin startup marks, explicit timestamps, reads, named/all clearing and
  retained origin using Code-OSS's own fallback. This is not a rewritten fixture,
  a Node runtime dependency or execution of mainImpl/the complete application.
- Add the 67th standalone CTest gate with Node/native timer and actual embedded-host
  GC comparisons. Include pinned fixture bytes in Windows checkout-conversion hash
  checks. Update the detailed performance contract, API index, matrix and Code-OSS
  status; preserve honest separate Linux/Windows acceptance and no local GUI.

Validation: local macOS standalone build and all 67 CTest suites pass, including
the actual pinned Code-OSS module and existing loader/Buffer/runtime regressions.
Firefly also builds/links with the changed shared runtime; no desktop test was
launched. Full pinned formatting, runtime-bundle, Python syntax/diff and
unmodified-upstream checks pass; generated site 196 pages / 13,846 local links /
resources/fragments passes. New
Linux/Windows clock/fixture execution remains pending. Full goal remains active:
Code-OSS desktop, its cache/V8 adaptation, original-fs, missing process/desktop APIs
and public binary/event/port routing are not complete.

## 2026-11-07: Separate raw main-process filesystem facade for original-fs

- Previous turn made verified progress: 66cbe5c committed/pushed native performance
  clocks and actual pinned Code-OSS module acceptance. Continue the full goal with
  Code-OSS mainImpl's original-fs import, preserving Mite independence and isolation.
- Compare Electron 45.2.3 directly in its console-only Node mode: fs and original-fs,
  including promises, are distinct objects. Add trusted register_host_filesystem
  using a fresh FS factory/JS closures over one shared native filesystem host.
  Register original-fs only in Firefly's main loader before application entry.
- Preserve independent raw sync/callback/Promise methods and internal append/exists
  calls when ordinary fs is patched. Share actual native descriptor services, not
  mutable public facade objects. Host registry owns exports through GC; invalid,
  reserved and duplicate registrations reject without replacing the existing module.
- Test the same ESM fixture in the native Firefly starter, Electron console reference
  and collecting embedded host: exact Python-verified bytes/SHA-256, raw .asar bytes,
  URL paths, callback/Promise/sync IO, both descriptor directions, FileHandle use,
  facade patch isolation and module identity. A poisoned local package cannot replace
  the trusted registration. Standalone Mite still resolves ordinary npm packages
  and rejects an absent original-fs; it gains no Firefly dependency or builtin name.
- Add the 68th standalone CTest gate and focused macOS check-firefly-original-fs,
  also required by existing macOS release acceptance. The native starter test uses
  shipped Cocoa without constructing BrowserWindow, and explicitly app.quit;
  minimal Qt has no offscreen plugin. No renderer/window test suite was launched.
- Update detailed original-fs, host registration, migration, API index, matrices and
  Code-OSS docs. Physical raw access is not ASAR archive-entry/package loading;
  Windows desktop and new Linux/Windows raw facade acceptance remain pending.

Validation: local macOS CLI/host and product builds pass; the shared Firefly/Electron
raw FS gate and all 68 standalone CTest suites pass (20.54 seconds).
Full pinned formatting, Python syntax/diff,
unmodified-upstream and generated site checks pass (197 pages / 13,921 local
links/resources/fragments). Code-OSS still
does not run: cache/V8 adaptation, process/desktop services and public binary/event/
port routing remain required. Full goal active.

## 2026-11-07: Correct Windows module oracle and share native UTF-8 entry arguments

- Inspect authoritative Windows evidence instead of inferring new platform success:
  bc0afb2 run 42372490143 completed successfully; the later 8f617c0 run 42373435149,
  job 128406101281, compiled but failed module-utilities and HTTP default-errors.
- The module failure is in Node: its C: temp filename was stripped of the drive
  while cwd was on D:. Root-relative paths use the current drive. Correct the
  fixture to test either actual current-drive resolution or MODULE_NOT_FOUND,
  based on the physical candidate, rather than require another drive's file.
- Inspect embedded host startup as well: unlike the CLI's wide entry, narrow main
  relies on the CRT code page; pinned libuv uv_setup_args returns argv unchanged
  on Windows. Extract the CLI's existing UTF-16/UTF-8 conversion into an owned
  WindowsArguments helper and reuse it in the test host's wmain. Keep UTF-8 storage
  alive through run, pointers stable and copy disabled; retain invalid-UTF-16 errors.
  Do not impose CLI stdio/inheritance policy on embedders or change runtime run().
- Existing utility and original-fs gates launch the host from Unicode temp paths;
  they will validate this on native Windows. Record the new acceptance as pending.
  The separate HTTP failure has Node return 0xC0000409 after its first response;
  Mite produced both expected responses. Do not weaken or skip that differential
  test; further Node/native diagnosis remains required.

Validation: local macOS CLI/host build, utility comparison and all 68 standalone
CTest suites pass (20.34 seconds). These checks establish POSIX regression only;
the Windows-only entry/drive branches require hosted native execution. Pinned
formatting, diff and unmodified-upstream checks pass; generated site 197 pages /
13,922 local links/resources/fragments passes. Full goal active; Code-OSS desktop
and full platform acceptance remain incomplete.

## 2026-11-07: Resolve the Linux DBus package collision through a declared ABI-checked provider

- Previous goal turn made progress with original-fs and Windows entry/oracle fixes
  in 7067636 / 678c85e. Read current native evidence: Linux development runs at
  8f617c0 and 66cbe5c succeed (66/67 suites respectively); Windows 66cbe5c now fails
  only the already-corrected drive fixture, not HTTP. Keep the intermittent earlier
  Node HTTP failure recorded rather than declaring it fixed.
- Native Linux renderer run 42374318919, job 128410439668, reaches archive staging
  after actual renderer/helper/byte-handoff/app checks, then rejects different
  libdbus-1.so.3 files. Keep that genuine guard; the package needs one provider.
- Add an explicit policy choosing the frontend's actual clean-environment DBus
  provider. Generic closure staging still rejects undeclared content conflicts.
  Before mutation require the selected source to be observed in the original graph,
  have matching SONAME and fulfill selected consumers' symbol/version/type imports.
  Unversioned imports need default exports; incompatible/unknown/host choices fail.
- Retain all candidate/selected source hashes and per-consumer imports in the
  package manifest, stage one provider and preserve normal source copying, notices,
  relative RUNPATHs and relocated dependency checks. No vendor/build tree patch.
- Extend the real native ELF fixture with independent versioned providers, actual
  relocation after deleting build paths, source preservation and incompatible ABI
  negatives. Whole-app peer checks require matching provider provenance. Add ADR
  0103 and packaging docs; matching loader symbols alone is not application proof.

Validation: confirm Linux 678c85e run 42376107084 / job 128413897697 passes all 68
runtime suites (83.24 seconds), including module/performance/raw-host APIs, and
refresh their scoped API/matrix status. Local non-native ELF/parser guards and
formatting/syntax checks pass;
native component/provider and full Linux app/archive/Designer acceptance pending.
All 68 local Mite regression suites pass (19.75 seconds); generated site 198 pages /
13,993 local links/resources/fragments passes.
No local GUI. Full goal active; Code-OSS desktop still does not run.

## 2026-11-07: Native Windows 68-suite acceptance and ELF fixture source-scope correction

- Windows 678c85e run 42376107074 / job 128413923404 succeeds: all 68 suites in
  124.04 seconds, including corrected drive behavior, Unicode collecting host,
  module utilities, original-fs host and native performance/Code-OSS module gates.
  This is Mite/host acceptance, not Windows Firefly desktop or package support.
- The first provider component run 42377345034 / job 128417550031 stages the
  compatible graph, then fails its source-preservation assertion. The fixture
  accidentally included the staged executable copies, which must receive new
  RUNPATHs. Check only the actual original executables/providers; retain hash
  evidence for all originals, real relocation and incompatible ABI refusal.
- Renewed native component and complete Linux archive/Designer gates remain
  required. Keep the earlier isolated Node HTTP crash recorded; a passing current
  suite is not a diagnosed fix for that intermittent reference failure.

Validation: local ELF/parser guards pass. Native component rerun pending; no local
GUI. Full goal active, Code-OSS desktop remains incomplete.

## 2026-11-07: Accept native provider component and refresh Windows API evidence

- Native Linux component cff79fb run 42377484491 / job 128417952776 passes all real
  ELF gates: compatible declared/versioned provider, one physical DSO, unchanged
  originals, relocation after hiding source builds and incompatible-version/
  unobserved/host/implicit-collision refusal. Existing payload, native launcher,
  live DSO provenance and distribution-notice component gates also pass.
- This is not full archive acceptance. Renderer cff79fb run 42377484455 remains
  live at native configure/compile as checked through job 128418042162. Earlier
  24ba055 renderer stopped in the already-corrected component assertion before the
  full app stages, so it provides no packaged-provider application evidence.
- Refresh API/matrix/status and Windows development docs with the authoritative
  678c85e Windows 68-suite result (124.04 seconds). New module/performance/raw-host
  and wide-entry tests are accepted on native Linux/Windows for their scoped
  contracts. Windows Firefly desktop, Linux archive/Designer, Code-OSS desktop and
  broader API/platform coverage remain required; no full-goal completion claim.

Validation: source-backed documentation update; previously completed native jobs
above are the evidence. Site 198 pages / 13,995 local links/resources/fragments
and diff checks pass. Native full archive run remains in progress. No local GUI.

## 2026-11-07: Handle genuine dependency-free ELF shared objects during package verification

- Full Linux cff79fb run 42377484455 / job 128418042162 advances past DBus provider
  selection and copying, then its closure verifier rejects ldd's "statically linked"
  listing. Do not assume an object is a valid shared library from this text alone.
- Inspect DT_NEEDED first. An empty list is accepted only for an actual ET_DYN
  object with a valid SONAME; keep static executables and ambiguous text refused.
  Add the object path to parser failures so future native errors identify inputs.
- Compile a real no-runtime/-nostdlib leaf DSO in the native relocation fixture;
  require its empty dependency result and later normal loader use after relocation.
  Compile a static executable separately and require explicit metadata refusal.
  Original source preservation, provider ABI checks and archive provenance remain.

Validation: local parser/unsupported-host guards and syntax/formatting checks pass.
Renewed native component and full Linux archive/Designer acceptance remain pending.
Process event/signal implementation is independent ongoing work, not yet accepted
or included in this commit. No local GUI. Full goal active.

## 2026-11-07: Prioritize Code-OSS SIGPIPE bootstrap with real process events

- Add an authored ESM process EventEmitter layer and a C++/libuv native signal
  adapter, without Node/V8, LibWeb or upstream modifications. Match custom event
  order, receiver, once, removal, errorMonitor and process/default identity.
- Native watch subset: SIGINT/SIGTERM, SDK SIGPIPE/SIGHUP and POSIX SIGUSR2.
  Roots survive GC, native delivery uses owner-loop dispatch/checkpoints, watches
  are unreferenced and shutdown closes them before VM destruction. Bound active
  plus deferred-close handles to 32, with explicit refusal/recovery tests.
- Require explicit embedded-host opt-in, exclusive Mite signal ownership and
  foreign POSIX handler refusal. Verify previous ignored disposition survives
  final-listener removal and shutdown. Lifecycle/rejection/IPC events remain
  explicitly unsupported; no process.kill, broad signal or Code-OSS launch claim.
- Add non-GUI Node differential, actual two-delivery POSIX child-PID controls,
  collecting-host delivery, protected native handler, ignored-disposition restore,
  throwing newListener cleanup and handle-credit gates. CLI/Firefly opt in; normal
  embedders do not. Document ADR 0104, API and current matrix/status.
- Native Linux package-component run 42379262086 at 197cd2a passes, including
  the real dependency-free DSO and static executable refusal. Full renderer run
  42379262065 remains live at configure/compile as checked through job 128423224720;
  full archive/Designer and Windows Firefly desktop remain unaccepted.

Validation: local macOS Release all 69 CTests pass (21.06 seconds), including new
process events (0.72 seconds). New native Linux/Windows API acceptance is pending.
Non-GUI Firefly integration build passes; existing dependency deployment-target
linker warnings remain, so no older macOS acceptance is claimed. Site verification
passes 199 pages / 14,066 local links/resources/fragments; source formatting,
generated integrity and diff checks pass. No local GUI tests. Full goal active; next prioritize an actual
pinned Code-OSS bootstrap gate, then truthful cache/V8 adaptation and public
binary/event/port transport prerequisites.

## 2026-11-07: Execute the actual pinned Code-OSS shared source bootstrap

- Pin unmodified bootstrap-node.ts from the existing audited Code-OSS commit
  6531648f5ace8b77ae799262bdc97bf048461938, with exact SHA-256, LF checkout guard
  and original MIT license. Node's existing test-only stripTypeScriptTypes erases
  types; both runtimes execute identical output with no function replacements.
- The actual module evaluates under Mite/LibJS: createRequire/imports, SIGPIPE
  registration, VSCODE_CWD and platform cwd setup succeed. Invoke upstream
  configurePortable with actual present/missing directories and portable tmp;
  compare resulting paths/environment against Node 24.15.0.
- Source mode is explicit (VSCODE_DEV=1). Exported developer loader hooks/global
  search customization and packaged ASAR resolution are not exercised or accepted.
  This is bootstrap-node, not the complete main.ts entry, nodeCompileCache,
  mainImpl, renderer workbench, extension host or terminal. Cache/V8 adaptation
  and public binary/event/port transport remain priority blockers.

Validation: local macOS Release all 70 CTests pass (20.68 seconds), including the
actual source-bootstrap gate (0.15 seconds), process delivery/GC and pinned
performance module. New native Linux/Windows gates remain pending. No runtime
implementation changed in this step; no local GUI. Site (199 pages / 14,067 local
links/resources/fragments), formatting, Windows LF/hash and diff checks pass.
Full goal remains active.

## 2026-11-07: Accept the complete experimental Linux archive and Designer candidate

- Native Linux renderer run 42379262065 / job 128423224720 at 197cd2a completes
  successfully. Actual renderer/helper/isolation and trusted WebContent byte
  handoff pass; unchanged upstream and owned codegen checks pass.
- Portable x86-64-v2 policy audits 2,948 project C++/19 Cargo commands in the
  renderer SDK and 278 C++/7 Cargo commands for independent Mite. Live X11 app,
  screen/clipboard and renderer kernel observations pass under Xvfb/Openbox.
- The actual stripped archive is extracted and launched under relocation with
  complete checksum/ELF closure and no SDK ELF mappings. DBus provider selection
  and genuine dependency-free DSO handling now pass the full application path.
- The pinned production Designer bundle removes 139.8 MiB of app/module build
  payload. Its actual extracted archive passes boot, edit/round-trip, Monaco,
  native HTML controls, server/capability checks and absent Node privileges.
- Accepted archive/checksum/BUILD.json are retained as Actions artifact
  linux-designer-package-candidate, ID 13577811546. ZIP upload is 122,681,800 bytes;
  that is not the installed framework size or a newly published release.
- Refresh current status/API/platform/package docs and correct the previous
  source-bootstrap site's recorded link count to the actual 14,067.

Validation: inspect the completed successful native job's metadata/logs and
artifact inventory; no retries or removed checks. Scope is Ubuntu 24.04 x64 X11
Xvfb/Openbox, not Wayland/physical displays/other distributions or Windows desktop.
Earlier SIGILL cause remains undiagnosed. New process/bootstrap sources need
their own native gates. Site (199 pages / 14,069 local links/resources/fragments)
and diff checks pass. No
local GUI. Full goal active with Code-OSS bootstrap/cache and IPC prioritized.

## 2026-11-07: Recognize truthful Mite identity in actual Code-OSS platform modules

- Inspect the actual pinned full Code-OSS source, now retained in ignored
  build/code-oss-source. common/platform and common/process recognize native
  execution through versions.node; unchanged Mite is misclassified as web/unknown,
  dropping architecture, environment and real cwd. Do not fake Node/Electron.
- Add a reusable Python source-overlay preparation command. Validate exact
  platform/process/license hashes and unique seams before creating fresh output;
  add versions.mite recognition and its TypeScript interface field only. Keep
  originals and every other source byte unchanged. Preserve MIT license and
  record revision/input/output hashes in ELYTRA-ADAPTATIONS.json.
- A real upstream source archive exposed its explicit LICENSE.txt eol=crlf rule.
  Record that exact alternate checksum; accept only the two pinned license forms
  and preserve supplied bytes. Verify both LF/CRLF and drift refusal. Require LF
  TypeScript inputs; refuse existing outputs, overlapping trees and changed files
  before writing output. This is a partial source overlay, not a runnable app.
- Pin unmodified platform.ts/process.ts/nls.ts, preserve license/hash/LF guards.
  Compile actual TypeScript with existing pinned build-only esbuild 0.25.12,
  including const enums and type-only imports. Compare unmodified Node, adapted
  Node and adapted Mite; require native OS/arch/env/cwd, NLS configuration, native
  timer and absent fake Node/Electron identity. Unadapted Mite must reproduce the
  original diagnostic/web fallback. No test module replacements or fake services.
- Install the pinned build compiler before native CLI CTests in Linux/Windows
  CI and release driver; extend trigger paths and README build prerequisites.
  Document ADR 0105, adaptation guide and exact remaining desktop/cache blockers.
- Native 94acc0c process-event acceptance completes: Linux run 42380475178 /
  job 128426838492 passes all 69 suites in 76.84 seconds, with actual POSIX child
  delivery. Windows run 42380475051 / job 128426933149 passes all 69 in 102.93
  seconds, including registration/unref/policy; no Windows POSIX delivery claim.

Validation: local macOS Release all 71 suites pass (20.61 seconds), actual
platform gate 0.40 seconds; targeted gate also passes added license checkout
controls. Prepare the overlay successfully against the actual full pinned source
archive. New native bootstrap/platform jobs are pending/running, not accepted yet.
Formatting, Windows LF/hash, site (201 pages / 14,205 local links/resources/fragments)
and diff checks pass. No local GUI,
no Ladybird changes. Full goal active; cache/V8, main.ts/mainImpl and public
binary/event/port transport remain prioritized prerequisites for real Code-OSS.

## 2026-11-07: Advance actual Code-OSS main.ts through an explicit LibJS cache policy

- Extend the audited source overlay with opt-in --libjs-cache. Redirect the two
  static engine-specific cache imports to an owned application module; preserve
  upstream algorithms and other bytes. Guard enable/ready/wait before filesystem
  effects. Ordinary development/missing/disabled paths run, while V8 generation,
  measurements, flush/statistics and marker waits fail explicitly. No fake cache,
  statistics or ready marker; public Mite Node-cache/V8 APIs remain absent.
- Pin the real cache/static common dependency corpus and main.ts with unchanged
  source/hash/license/LF guards. Run actual upstream logic in original Node and
  adapted Node/Mite. Verify statuses, logs, real paths, helper kind mapping,
  explicit errors, absent false cache metadata/files and preserved originals.
- Add pinned esbuild diagnostic source compilation with real imports, Code-OSS
  decorator/class-field/ES2024 settings, overlay input/output checks and complete
  compilation provenance. Validate/transform before creating fresh output. This
  is not type checking, package installation, workbench asset building or a
  distributable-app claim. Original Code-OSS inputs and Ladybird remain unchanged.
- Compile the actual full pinned source into 11,534 JS/JSON diagnostic files.
  Run unmodified main.ts under Mite through a diagnostic wrapper: real cache
  initialization completes in both development and default missing-cache modes,
  real mainImpl fails at original-fs, and result stays
  nonzero (1). Raw FS already exists in Firefly; the standalone CLI lacks that
  host module by design. Next acceptance requires real Firefly host/import/API
  contracts, not a fabricated mainImpl or a fake successful desktop launch.
- Add native console.warn stderr routing for the real upstream disabled branch.
  Full regression exposes previously hidden event-target-shim duplicate warnings.
  Filter only W06/W07 in our adapter to preserve Node's silent duplicate behavior;
  unchanged lifetime/abort tests pass again. Regenerate guarded bundles. Extend
  formatting to maintained TypeScript for the new owned policy module.
- Native Linux 10f6d70 run 42381776025 / job 128430730479 passes all 71 in 77.23s,
  including source-bootstrap/platform. Windows 2bf956d run 42380750929 / job
  128429517381 passes all 70 in 102.74s. Windows 10f6d70 job 128432211417 passes
  the actual new platform component but full regression fails: Node 24.15 exits
  0xC0000409 in HTTP/HTTPS, while Mite HTTP completes. Do not diagnose/fix/skip
  the oracle from these observations or claim full 71-suite Windows acceptance.

Validation: local macOS Release all 72 CTests pass (21.16 seconds) after the real
warning regression fix; targeted cache tests also pass current owned helper types
and no-fake-builtin assertions. Non-GUI Firefly integration build passes, with
existing dependency deployment-target warnings retained. Source formatting,
generated/LF integrity, site (202 pages / 14,280 local links/resources/fragments)
and diff checks pass. No local
GUI. New native cache slice pending; full goal active, Code-OSS host startup and
public binary/event/port transport remain priorities.

## 2026-11-07: Keep native cache acceptance triggered by application policy changes

- Extend native Linux/Windows workflow push/PR filters to the owned
  firefly/compat/code-oss source directory. A policy-only edit must run the real
  CLI/cache gates, even though this application adapter intentionally lives
  outside Mite. The previous cache implementation commit includes test/CMake
  changes and already triggers these workflows; this closes the future isolated
  policy-edit gap. Runtime behavior and tests are unchanged.

Validation: pinned workflow formatting and diff checks pass. No local GUI.
Full goal active; cache slice needs its own native acceptance and actual Code-OSS
desktop/transport/native Windows work remains required.

## 2026-11-07: Resolve Code-OSS Electron imports through an explicit native host policy

- Add --electron-compat before the Firefly entry: register the same actual API
  object as electron/firefly for ESM/CommonJS, with only implemented named exports.
  Default ordinary npm lookup remains; no Node/Electron version metadata is faked.
  Strip the host option while preserving all later application arguments. Pure
  native tests cover absent/repeated/unknown flags, help, UTF-8 and borrowed argv.
- Propagate the immutable bool through trusted preload metadata. Default schema
  remains filename/dirname; only a typed electronCompat extra key is accepted.
  Enabled require aliases return the same frozen contextBridge/ipcRenderer facade,
  retaining world/execution/revocation checks. No main APIs, filesystem or page
  globals/subpath loader privileges are added. Ladybird and IPC method IDs remain
  unchanged.
- Add --electron-compat to native macOS/Linux packagers, compile launchers with
  the option before the entry and record BUILD.json electronCompat. Refuse old
  SDKs through their non-GUI option/help path before output mutation. Real compiled
  launcher fixtures test both modes after relocation, spaces/UTF-8 and exec PID
  ownership. Whole flagged archives still need separate platform acceptance.
- Batch actual native macOS default/opt-in tests with hidden windows: host/npm
  precedence, ESM/CommonJS identity, unchanged app args, real owned WebContent,
  frozen restricted preload alias, deferred bridge calls and no page privileges
  all pass. This was one targeted local native batch, not repeated desktop suites.
- Run actual full-source Code-OSS main.ts in the real Firefly host. Raw FS and
  electron imports resolve, cache initialization completes, then real dependency
  lookup fails on minimist. Diagnostic wrapper explicitly exits the real host and
  retains error result 1. Do not advertise a desktop start or claim the next API
  failure before preparing the complete pinned runtime dependency graph.
- Require the native alias gate in macOS check-firefly/release and Linux renderer
  CI; require fast option/launcher components in the standalone/native package
  gates. Linux/Windows CLI trigger paths include the new pure startup sources.
- Native Linux 790235c run 42383406706 / job 128436964155 accepts all 72 suites in
  80.58s, including the real cache/refusal gate (1.09s). Full Linux 03bd439 run
  42383368668 / job 128435500318 passes archive/Designer; candidate artifact
  13579151776 is retained. New alias code is not in those accepted snapshots.

Validation: local macOS all 74 runtime/component CTests pass (21.42 seconds),
targeted option/launcher checks pass after the empty-argv guard; actual native
default/opt-in preload/host gate, real SDK non-GUI preflight and Firefly integration
build pass. Existing dependency deployment-target warnings remain. SDK preflight
rejects an unverified runtime that only echoes the requested arguments. Native
Linux 790235c Windows counterpart run 42383406745 / job 128438525424 compiles but
fails at npm fixture installation before CTests; no cache acceptance follows and
the silent exit is not diagnosed as a network or runtime failure. Formatting,
upstream/generated integrity, site (204 pages / 14,425 local links/resources/fragments)
and diff checks pass. New native Linux alias and
Windows/current package acceptance remain open. Full goal active; Code-OSS runtime
dependencies/host startup, public binary/event/port transport and native Windows
remain required.

## 2026-11-07: Install the real pinned Code-OSS root graph and reach the next loader blocker

- Prioritize actual Code-OSS bootstrap over unrelated example/API breadth. Pin
  unchanged root package/lock/workspace/configuration hook/native-build inputs,
  patch, mainImpl and notices from 6531648f5ace8b77ae799262bdc97bf048461938.
  Use upstream pnpm 12.16.0 in build-only ESM tooling; preserve Ladybird and source.
- Prepare fresh verified root production installs with --frozen-lockfile --prod
  --ignore-scripts --filter .; dependency lifecycle scripts are disabled, upstream
  configuration hook still executes. Distinguish prepared/failed/completed states.
  Root devDependencies, other workspace/importer and extension roots are excluded.
- Actual macOS ARM64 frozen install completes: 71 direct dependencies, 258 lock
  snapshots, 231 installed package directories and 27 native/WASM evidence
  candidates. Inspect package data without executing it; markers are heuristic,
  not package compatibility or N-API support. Keep original lock/config unchanged.
- Feed completed same-source/platform modules into the actual source compiler,
  validate contained links and write final metadata after successful copy. Refuse
  partial inventories and source drift before output mutation. Compile 11,534 real
  JS/JSON source outputs with actual dependencies, not replacement main modules.
- Run actual full-source main.ts once in the real Firefly host, without creating
  a workbench window. It advances beyond minimist and exits 1 at the missing named
  node:module.registerHooks export. Implement that real general loader contract
  next; no throwing/no-op export merely to make linking appear successful.
- Extract actual compiled upstream parseCLIArgs with exact minimist 1.2.8
  (lockfile integrity matches). Fourteen real parser cases agree under Node/Mite.
  This scoped subroutine test does not substitute for a desktop startup.
- Add native macOS/Linux/Windows frozen-root-install matrix and retained inventory;
  results pending. Install the build-only tools in standalone/release test jobs.
- Record completed 5735b91 acceptance: Linux 42386340937/job 128444427776 all 74
  suites, 78.51s; Windows 42386340970/job 128444466253 all 73, 130.32s. Linux full
  renderer 42386340939/job 128444427999 passes actual default/opt-in alias/preload,
  stripped default archive and Designer edit/Monaco/HTML controls; candidate
  artifact 13580715220 (122685399-byte ZIP). Native package 42386340998/job
  128444427726 passes compiled opt-in/default launcher checks. Whole opt-in app
  archive and Windows desktop remain unaccepted. Later Windows success does not
  diagnose/fix prior reference crashes. Published Alpha 2 remains old source.

Validation: local macOS Release all 75 CTests pass (22.98s), with the new actual
root-graph/CLI parser gate; targeted gate also passes both vendored/full-source
input layouts and refusal controls. One windowless actual Firefly startup
probe, no repeated local desktop suites. Formatting, generated site (205 pages / 14,496 local links/resources/fragments)
and diff checks pass. Full goal active; real loader hooks, required desktop
startup APIs, public binary/event/port transport, native service adapters and
native Windows remain required. See ADR 0108 and the updated adaptation guide.

## 2026-11-07: Implement synchronous resolve hooks and advance the actual Code-OSS import graph

- Add real node:module.registerHooks resolve chains to the ESM-owned utility
  module. LIFO nextResolve, changed specifiers/context, explicit short-circuit,
  synchronous result validation and idempotent deregistration are implemented.
  Reject load functions atomically; no fake hooks, source transforms or V8/Node.
- Bridge later static/dynamic ESM edges, CommonJS require/resolve and createRequire
  through the same native resolvers and canonical caches. Pass conditions through
  package imports/exports recursively without mutable global resolver state.
  Preserve truthful absence of module-sync while require(ESM) remains unsupported.
- Root the dispatcher in the owning VM/loader; snapshot each active chain. Saved
  continuations retain JS state and a persistent native resolver, never borrowed
  native stack frames. Real embedded-host GC tests cover active registration and
  continuation calls after return/deregistration. Bound registrations/conditions.
- Preserve native file URL/separator/NUL/protocol checks and private shim guards;
  validate format hints and effective supported JSON attributes. Node comparison
  confirms nextResolve context attributes alone do not replace returned attributes;
  correct that initial mismatch before acceptance. Explicit unsupported load,
  format/protocol/private-target and capacity controls pass.
- Compare real Node 24.15/Mite module execution: chains/context, both loaders,
  cache identity, builtin mappings, custom export conditions, changed bases,
  nested resolution and later static graphs, JSON attributes, exact thrown
  exceptions, bad results and saved continuations. No desktop simulation.
- Build actual Firefly and run one windowless full-source Code-OSS diagnostic with
  the installed pinned root dependencies. registerHooks now links; the actual
  next failure is missing node:util.promisify, still exit 1. This does not prove
  execution of Code-OSS's Electron-only ASAR path or a workbench launch.
- Record adc00d0 native dependency installation acceptance: run 42389926763 jobs
  128455167722 (Linux), 128455167869 (macOS), 128455167888 (Windows) all complete
  the exact 71-root/258-snapshot graph with scripts disabled and inventories.
  Linux standalone run 42389926816/job 128455167679 passes all 75 in 66.05s.
  These snapshots do not contain the new resolve hooks; their native gates remain
  pending. No Windows desktop acceptance follows from dependency installation.

Validation: local macOS Release all 76 CTests pass (22.39s), new hook gate 0.28s;
Node/Mite/actual host-GC comparisons and explicit refusal controls pass. Firefly
integration build passes with existing SDK deployment-target warnings. Formatting,
generated integrity, site (206 pages / 14,566 local links/resources/fragments)
and diff checks pass. One windowless
startup diagnostic, no repeated local desktop suites and no Ladybird changes.
Full goal active. Next actual loader/API blocker is util.promisify, followed by the
remaining startup contracts, public binary/event/port transport, workbench/native
service and Windows requirements. ADR 0109 records ownership/language decisions.

## 2026-11-07: Include the LibJS inline value operations required by resolve hooks

Native Linux 98866c3 run 42390960317/job 128458229557 fails at linking before
CTest: undefined JS::Value::to_boolean() const from ResolveHooks.cpp. Its declaration
is in Value.h, but the inline definition belongs to ValueInlines.h. Include that
header directly in our adapter, as existing encoding adapters already do. Do not
patch Ladybird or depend on another object happening to emit this inline symbol.

Validation: local native object/runtime rebuild and targeted actual resolver tests
pass after the include. Existing macOS full regression passed; this small include
fix still needs a fresh native Linux job before hook acceptance. No local GUI or
upstream changes. Full goal active; util.promisify work remains a separate slice.

## 2026-11-07: Implement util.promisify and actual builtin callback result contracts

- Add maintained ESM promisify to the existing util facade and named ESM exports.
  Forward this/arguments, append an error-first callback, preserve exact error
  values and synchronous exceptions, settle once and adopt success thenables.
  Preserve prototype/name/length/accessor/symbol descriptors without reading
  unrelated getters; match custom accessor order and falsy custom values.
- Implement the shared public promisify.custom symbol, exact custom-function
  identity/self marker and conversion-time validation. Ordinary conversion creates
  a new wrapper; no original-function cache. DEP0174 warning emission remains a
  documented difference, not complete util API support.
- Share a private named-argument symbol through GC-rooted per-loader native host
  state. fs.read/write and dns.lookup retain named results without a public custom
  override or changing repeated wrapper identity. Existing filesystem/DNS APIs,
  resource ownership and raw-FS facade separation remain unchanged.
- Add genuine execFile custom Promise adaptation with immediate .child, stdout/
  stderr success results and rejection output. Reuse existing Promise timer
  functions through timeout/immediate custom getters, preserving abort/ref behavior.
- Compare Node/Mite behavior and force actual host GC with pending callbacks and
  native timer-rooted receivers. Execute real file/descriptor operations, numeric
  DNS, timer cancellation and native child success/nonzero output paths. No mocks
  or GUI suites substitute for native service behavior.
- Build actual Firefly; one windowless full-source Code-OSS probe passes promisify
  and reaches missing Electron protocol (exit 1). Generic utility acceptance does
  not prove Code-OSS's later filesystem workflows or workbench startup. Next work
  is real required protocol behavior, not an empty exported object.
- Record adc00d0 acceptance: Windows 42389926814/job 128455206247 all 74 suites in
  128.09s; Linux renderer/default archive/Designer 42389926764/job 128455167530
  passes and retains artifact 13581920595 (122684408-byte ZIP). Those snapshots
  predate hooks/promisify. Linux 98866c3 fails linking before CTest; separate
  6c613ec inline-header correction is pushed and needs fresh native acceptance.

Validation: local macOS Release all 77 CTests pass (23.35s) after final private
metadata/identity correction; new util gate covers actual Node/Mite/host GC and
native services. Firefly integration build and later explicit-inline object rebuild
pass, with existing SDK deployment-target warnings. Formatting, generated integrity,
site (208 pages / 14,711 local links/resources/fragments) and diff checks pass. One windowless diagnostic, no repeated
local desktop suites, no Ladybird changes. ADR 0110 and detailed util reference
record semantics and limits. Full goal active; protocol/startup, public binary/event/
port IPC, workbench/native service integration and native Windows remain required.

## 2026-11-07: Build actual custom-protocol origin and secure-context foundations

- Audit the real pinned Code-OSS protocol usage: mainImpl registers privileged
  custom schemes; protocolMainService later registers authorized vscode-file file
  responses, blocks direct file access and handles managed remote resources.
  Do not replace the current import failure with a dummy exported registry.
- Add an internal native origin policy in C++, integrated with actual LibURL and
  LibWebCommon through two fingerprinted owned build compositions. Preserve all
  upstream bytes outside the include/early-return seams and all notices. Neither
  the Ladybird source nor standalone Mite's linked dependency graph changes.
- Native-only one-time atomic table publication validates canonical names, limits,
  duplicates and builtin/internal takeover before changing state. Keep bounded
  immutable policy alive for worker/static teardown; no reset, JS entry point,
  environment or helper argument configures it. Product/diagnostic defaults remain
  empty; only the native probe installs its own process table.
- For standard entries, derive real custom-scheme/normalized-host/explicit-port
  tuple origins using upstream domain/IDNA/IP parsing. Preserve host/scheme/port
  separation and relative URL origins. Secure trust requires a matching registered
  secure standard tuple; opaque/nonstandard/invalid/unregistered URLs gain none.
  Existing HTTPS/HTTP/loopback/file behavior remains on upstream paths.
- Actual native composed-library tests pass: default denial, tuple identity,
  path/query/fragment/userinfo behavior, explicit 0/443 ports, IDNA, IPv6, same-host
  relative resolution, invalid/duplicate/oversized registration atomicity,
  immutable publication and concurrent read loops. No mock policy-only acceptance.
- Require the non-GUI native gate in macOS check-firefly/release, Linux renderer and
  experimental Windows renderer CI. Run the Windows origin component before the
  broader renderer build, retaining its separate evidence if later work fails.
  Add the exact composition guard to standalone CTest and CI trigger paths.
- This does not yet supply Chromium custom-standard URL parser/mutation behavior,
  trusted main/helper configuration, actual resource/file transport, fetch/CORS/CSP,
  ServiceWorkers or code caches. Public protocol remains absent; Code-OSS still
  stops at its import. Next complete parser/configuration and bounded native
  request/file-response paths before public registration/renderer acceptance.
- Native Linux inline-header fix is now accepted: 6c613ec run 42391732431/job
  128460535492 passes all 76 suites in 96.57s, actual resolve hooks 0.45s. This
  snapshot predates promisify and the new protocol component; Windows/current
  source acceptance remains separate. Prior failure evidence stays recorded.

Validation: local actual LibURL/LibWebCommon default/configured probes pass without
GUI; full Firefly integration rebuild passes (existing SDK deployment-target
warnings retained). Standalone native rebuild, new protocol composition CTest and
upstream/guard tests pass (3 tests, 1.76s); last full runtime baseline remains the
previous 77-suite macOS acceptance. Formatting/site (209 pages / 14,783 local links/resources/fragments) and diff
checks pass. No local GUI, no new Code-OSS launch claim and no Ladybird edits. ADR 0111
and native README record remaining stages. Full goal active, including real
protocol/resources, public binary/event/port IPC, workbench and native Windows.

## 2026-11-07: Deliver immutable native origin policy before owned helper bootstrap

- Inspect actual pinned Rust SchemeType/BasicParseOptions: no custom-standard
  scheme extension exists. Preserve the unmodified upstream parser; do not create
  a private copy/fork. Continue the required independent native configuration path.
- Add exact versioned JSON codec with 8 KiB/32-entry budgets, typed canonical
  scheme/standard/secure fields, common pure name/duplicate/builtin validation and
  explicit unknown-field/privilege rejection. Parsing/roundtrip do not publish.
- Add one fingerprinted owned LibWebView WebContent-launch seam, preserving source
  bytes/notices and all IPC method IDs. A declared native parent policy is copied
  through one private argument; no environment, file or page/preload setter.
- Seal an empty undeclared policy on the first helper launch as well, omitting
  its option. A real NeverDestroyed empty table avoids sentinel/destruction races;
  late registration fails instead of diverging from prelaunched helpers. Public
  scheme registration timing/spare-process integration remains unfinished.
- Consume/validate the private option in both owned helper entries before upstream
  bootstrap/VM initialization. Preserve argv0, post-delimiter/empty/Unicode args,
  null termination and owned buffers through the upstream entry. Invalid/duplicate
  input fails before startup or publication. A read-only PID/canonical-policy
  readiness record supplies native startup evidence while this path develops.
- Native tests pass actual codec, malformed/unknown data, budgets, roundtrip,
  atomic refusals, copied temporary-input/argv lifetime, default origins and late
  registration denial. Reuse the actual headless renderer probe via an owned
  wrapper; ordinary probes gain no protocol test options.
- Real diagnostic and production helpers receive the exact parent's policy before
  the first page script, correlated to their actual WebContent PID. Default mode
  sends none and page/native capabilities remain absent. Require this native
  configuration gate in macOS release/check-firefly and Linux runtime CI;
  Windows has the native codec/origin gate, not owned helper delivery.
- The initial DOM URL end-to-end assertion fails: new URL(app://...).origin stays
  null despite correct native delivery. Inspect Bindings/Rust/src/domurl/url.rs:
  DOM origin/extract-origin use Rust directly, bypassing C++ URL::origin. Preserve
  a distinct strict --require-dom/check-firefly-protocol-dom-origins gate and
  confirm its failure; the internal configuration gate reports this as PENDING,
  never application compatibility. No public protocol export or successful
  Code-OSS launch follows. Resolve the Rust/C++ origin/parser gap before exposure.
- Record completed a56d183 acceptance: Linux 42391944791/job 128463329796 all 77
  suites, 68.73s (promisify 0.41s, resolve hooks 0.30s); Windows 42391944810/job
  128462034123 all 76, 130.36s (promisify 0.86s, hooks 0.67s). Linux renderer/default
  archive/Designer 42391944775/job 128462303093 passes; artifact 13582388416 is
  122713450 bytes. Later f2e887b Linux 42393201428/job 128464976481 all 78 passes
  in 97.65s, including the composition guard, not the native renderer component.

Validation: actual native origin/config/argv/sealing probes and headless diagnostic/
production helper configuration pass; strict DOM origin compatibility explicitly
fails with its observed reason. Integration compilation passes with existing SDK
warnings. The standalone composition CTest passes; Mite runtime code is unchanged
and the last full local runtime baseline remains 77 suites. Formatting, site
(210 pages / 14,854 local links/resources/fragments) and upstream/diff checks pass. No desktop windows/GUI suite or new
Code-OSS launch; Ladybird remains unchanged. ADR 0112 and references record worker,
parser/DOM, registration-lifecycle and resource-transport gaps. Full goal active.

## 2026-11-07: Unify native and DOM Rust origins on the Code-OSS critical path

- Keep Code-OSS as the first compatibility target. Record explicit execution order:
  protocols/resources, actual main bootstrap, preload/binary-port workbench bridge,
  usable file workspace, extension/terminal/native services and per-platform packages.
  Defer unrelated website/example breadth and HTTP/2/3 behind that path.
- Resolve the observed strict DOM failure at the actual Rust Url::origin entry,
  rather than decorating its JavaScript getter. One private Rust helper consults
  the canonical immutable C++ policy using upstream's existing borrowed FfiOrigin
  callback ABI. Copy before the callback returns; parsing never recurses into origin.
  Remove the earlier C++ URL.cpp composition so native/DOM consumers share one path.
- Keep Ladybird and Cargo manifests/lockfiles/parser modules unchanged. Guard
  lib.rs/origin.rs/ffi.rs fingerprints, exact source restoration and module layout;
  generate only root/module-path composition plus the narrow origin call. Preserve
  notices. Standalone Mite neither uses the wrapper nor links the Firefly policy.
- Add native build-only no-shell Rust compiler routing. Preserve non-target crate
  arguments, verify hashes, refuse ambiguous source/self-recursion and chain an
  existing compiler cache after substitution. Native argv/exit tests cover empty,
  Unicode, spaces, quotes and backslashes; Windows execution still needs fresh CI.
- First strict attempt stayed red because Cargo reused fresh pre-adapter artifacts.
  Add an explicit composition cfg fingerprint through an owned CMake module that
  includes upstream's unchanged Cargo module. Rebuild with changed Rust flags;
  inspect real LibURL and LibWeb Cargo depfiles for generated root/origin/ffi/helper.
  Require those checks and stale-output refusal, not a successful Cargo message.
- Actual local macOS native tuple/trust/codec/argv/sealing/concurrent-read probes
  pass with the upstream C++ URL source. Actual first-script headless diagnostic
  and production helpers now pass normalized DOM origins, credentials, explicit
  ports, host separation, IDNA, IPv6, relative URLs, default/builtin behavior and
  genuine getter brand checks. Page/native privileges remain absent. Make the
  existing required renderer configuration gate strict; the DOM target is an alias.
- Keep the LibURL-only origin gate independent of the broader LibWeb bindings
  build for native Windows diagnostics. Require both archive dependency checks
  for actual helper/DOM acceptance. Add standalone source/driver CTest and CI paths.
- Record previous bd83480 hosted acceptance: Linux 42396266883/job 128474188943
  all 78 tests in 100.86s; Windows 42396266975/job 128474224037 all 77 in 134.45s.
  Linux renderer/default archive/Designer 42396266897/job 128474188739 passes,
  including both real helper policy deliveries, but still reports its older DOM
  gap. Those runs do not accept this new Rust composition. Fresh CI remains needed.

Validation: native Release renderer and Firefly product compilation pass. Native
origins and strict actual headless diagnostic/
production helper gates pass; actual emitted Rust dependency audits and native
compiler-driver refusal/argv/exit tests pass. Standalone Mite rebuild passes and
4 targeted upstream/source/driver CTests pass in 2.59s; unchanged runtime's previous
full local baseline remains 77 suites. Formatting/diff checks pass. Site validates 211 pages / 14,924 local
links/resources/fragments. No desktop windows or broad GUI suite; Ladybird remains
unmodified. ADR 0113, status, API index, compatibility and update guide are current.
Full custom-standard parser/mutation semantics, worker propagation, registration
lifecycle, public protocol/resource handlers and real Code-OSS workbench remain
unfinished. No successful Code-OSS launch or new Linux/Windows acceptance is claimed.
Full goal remains active.

## 2026-11-07: Preserve Linux CPU verification with composed Rust origins

- Inspect terminal 614ebf1 Linux renderer run 42398340872/job 128480474208:
  configure/build command verification rejects the new explicit origin cfg flag
  before compiling the renderer. This is not accepted Linux origin behavior.
- Keep exact portable x86-64-v2, fatal warnings and optional retained-frame-pointer
  checks. When a generated origin composition exists, require exactly its actual
  root SHA-256 cfg input; continue rejecting arbitrary/duplicate/stale flags,
  changed CPU options and missing warnings. Standalone builds remain unchanged.
- Add generated-command controls for matching fingerprint, missing/wrong/duplicate/
  unrelated flags and changed composition, alongside existing CPU refusals.

Validation: targeted generated Linux command-policy test and formatting pass locally.
No GUI, runtime change or Ladybird edit. Fresh hosted Linux renderer acceptance is
required; full Code-OSS/protocol and cross-platform goals remain unfinished.

## 2026-11-07: Checked native protocol file bodies and real chunked transfer

- Inspect the actual pinned Code-OSS protocolMainService: it registers a file
  handler on defaultSession, returns authorized paths plus headers and blocks
  direct file requests. Do not substitute a protocol export without resource IO.
- Add a native main-side FileBody component using existing LibCore/AK only, with
  one owned checked regular-file descriptor. Do not accept a renderer URL/path or
  invent automatic root authorization; the future trusted handler must authorize
  its returned path before calling this component. No public API is added here.
- Capture content length at open; support bodyless HEAD, empty files and caller-
  paced reads up to 64 KiB, with 32,768-byte path and 64-GiB file limits. Reject NUL,
  relative paths, directories and nonregular handles. POSIX nonblocking admission
  refuses FIFO/device files without blocking before fstat; Windows checks disk
  handle and regular-file metadata and still needs actual platform CI.
- Close on completion/HEAD/cancel/error/destruction. Cancellation is idempotent;
  further reads reject. A premature EOF fails instead of silently satisfying an
  advertised content length. Growth cannot extend the original response; retain
  descriptor identity after pathname replacement. This is live file IO, not a
  content snapshot or asynchronous filesystem service.
- Actual native test reads a real 4 MiB + 19 byte file and sends 65 bounded raw-byte
  chunks through the existing transport to a separate native child. Independently
  check every byte, cumulative position and kernel child PID; the child never gets
  the source path. Verify empty transfer, HEAD, invalid chunk sizes, cancellation,
  missing/relative/NUL paths, directories, growth, replacement and truncation.
  POSIX additionally checks FIFO/device/oversize refusals. No GUI or JS mock.
- Require the component gate in macOS release/check-firefly and native Linux/
  experimental Windows renderer workflows before broader renderer compilation
  where appropriate. Preserve separate logs and truthful platform acceptance.
- Completed 614ebf1 Linux standalone run 42398340779/job 128480473977 passes all
  79 tests in 70.00s, including native compiler-driver argv test in 0.25s. It does
  not prove renderer Rust-origin or this file-body behavior. Its renderer run
  failed at CPU command verification, addressed in separate 1d033fe; new hosted
  renderer results remain pending.

Validation: local actual native checked-file/separate-child gate passes all above
cases; whole-source formatting, site and upstream/diff checks pass. Site validates
211 pages / 14,926 local links/resources/fragments. No Mite API or Ladybird source
changes and no desktop tests. Request/metadata framing, response headers/MIME,
helper/document admission and revocation, bounded concurrent lifetime, application
handler/session dispatch and actual renderer fetch/navigation integration remain
required before Code-OSS resources load. Public protocol and real workbench remain
unfinished. Full specification/Code-OSS/native Windows goal stays active.

## 2026-11-07: Native file-response metadata before streamed body bytes

- Add ResponseHead beside FileBody, reusing upstream HTTP field validation and
  Core filename MIME inference; no new Rust FFI, parser copy or upstream change.
  The internal exact version-1 schema carries nonzero request ID, status 200,
  captured length, HEAD flag and ordered headers, never an implicit source path.
- Generate Content-Length from the opened descriptor and infer Content-Type unless
  supplied. Preserve other header order/duplicates; reject length overrides,
  duplicate type/length, malformed fields and network framing/hop-by-hop headers.
  Bound both raw and encoded metadata to 32 KiB / 64 pairs. Preserve HTTP obs-text
  through explicit isomorphic byte/Unicode mapping, not accidental UTF-8 conversion.
- The actual separate native child now admits and checks MIME/length and Code-OSS
  COOP/COEP/cache/Document-Policy fields, duplicate headers and obs-text bytes before
  any file chunks. Validate completed body length; HEAD uses the same transport
  with the full advertised length and zero body bytes. Ordinary 4 MiB + 19 byte
  transfer, empty files, lifetime/cancel/mutation tests remain on the required gate.
- Add independent Python JSON interoperability/negative controls for schema,
  unknown fields/types/status, ID/length maxima and invalid values, duplicate/missing/
  mismatched headers, pair shape, framing, count/size/escaped budget, non-byte HTTP
  characters, NUL/DOS-EOF tails and nesting. Check the native builder separately,
  including explicit type override and raw/escaped/count limits. Verify upstream
  MIME results for ESM, HTML, CSS, WASM, JSON and unknown suffixes. Use explicit
  UTF-8 and binary Windows fixture streams; no local GUI was launched.
- Add ADR 0114 and update component/status/Code-OSS docs. These headers are carried,
  not yet applied in LibWeb. Do not claim cookie/CORS/CSP/COOP/COEP/browser isolation
  or public protocol compatibility from transport metadata.
- Inspect completed 614ebf1 Windows standalone 42398340794/job 128480513387:
  all 78 tests pass in 140.07s; native compiler driver argv/exit acceptance 1.67s.
  This does not accept renderer origins or current file-response code. The CPU-
  corrected 1d033fe Linux renderer run 42399004586/job 128482529005 is confirmed
  live in compile at observation; do not restart/cancel it or claim success.

Validation: actual native Release metadata/file-body gate passes locally, including
independent codec checks, native builder refusals, bodyless HEAD and metadata plus
65 raw chunks to a separate native child. Full source formatting, upstream/diff
and generated website checks pass (212 pages / 14,993 local links/resources/fragments).
Standalone Mite runtime/API code is unchanged; previous full runtime results stay
separate. Request admission, correlation/multiplexing, bounded concurrent lifetime,
helper/document revocation, application handler/session dispatch and actual renderer
fetch/navigation/response security still need implementation. Public protocol and
Code-OSS workbench remain unsupported. Full specification goal remains active.

## 2026-11-07: Own bounded file jobs and correlate native response frames

- Add FileRequests for one admitted native document generation. Require matching
  nonzero epoch and strictly increasing nonzero IDs, consuming admitted IDs even
  when later budget/open/metadata checks fail. Reject stale/wrapping/reused IDs.
  Authorization and real document admission remain the native adapter's job.
- Bound each owner to eight pending jobs and all owner threads to 64 shared atomic
  leases. Close files before returning credit. Completion, bodyless HEAD, cancel,
  read/metadata/encoded-budget failure, expiry, permanent revocation and destruction
  release ownership. Pulls return exact offsets and completion without file reopen.
- Apply a fixed 30-second deadline without activity extension. Check before reads;
  expose expiry settlement IDs for native event-loop scheduling, allocating before
  mutation. No automatic timer or forced interruption of synchronous IO is claimed.
  Revoked owners cannot revive through retained weak callbacks.
- Add versioned 32-byte little-endian FilePacket envelopes with native epoch,
  request, kind, payload size and body position. Validate exact lengths, known
  flags/kinds, nonzero identity, kind-specific metadata/chunk/control budgets and
  offset bounds. Metadata stays JSON; file bytes remain raw. These IDs identify,
  not authenticate; real helper/document request admission is still required.
- Move the actual separate-child file transfer onto the request owner and framed
  metadata/body/completion. Require exact epoch/ID and sequential positions in the
  native consumer. HEAD, empty files, 4 MiB + 19 byte transfer and cancellation
  after one 64-KiB chunk pass. Forcefully terminate the actual owned child while
  its file job is pending; actual peer-close callback revokes/releases the owner.
  Invalidate borrowed fixture callbacks before stack exit and pump deferred close
  delivery after return, preserving native callback lifetime.
- Test native interleaved pulls, deadline boundaries, foreign/stale contexts,
  completion/cancel/error/metadata-failure/destructor cleanup, late weak callbacks
  and ID exhaustion. Twelve actual native threads retain jobs concurrently and
  admit exactly the process maximum of 64, with eight-per-owner enforcement.
  Independent Python struct interoperability checks endian/layout, IDs, versions,
  reserved flags, kinds, truncated/trailing frames and offset/payload limits.
- Inspect terminal 1d033fe Linux renderer 42399004586/job 128482529005 SUCCESS:
  actual generated CPU policy, native origin/trust, Rust archive dependency audits,
  strict actual DOM origins, production helper configuration, stripped archive and
  extracted Designer gates pass. This accepts shared Rust/native origins on Linux;
  it predates file-response code. Do not apply it to the new native file component.
- Record ADR 0115 and update status/origin/Code-OSS references. No new upstream seam,
  Ladybird change, Mite API dependency or desktop GUI test is introduced.

Validation: actual local macOS native file component gate passes with managed
framed transfer, partial cancellation, forced peer loss and all registry/codec
checks. Source formatting, upstream/diff and site checks pass (213 pages / 15,061
local links/resources/fragments). New file-request code still requires Linux/Windows
component CI. Actual LibWeb pending-response/abort integration, native helper/
document binding and timer/revocation wiring, receiver multiplexing/error settlement,
application handler/session dispatch and renderer resource/security acceptance stay
unfinished. Public protocol and the real Code-OSS workbench remain unsupported.
Full original specification goal stays active.

## 2026-11-07: Repair observed native CI command and rename boundaries

- Inspect terminal accd1c7 macOS run 42400186300/job 128488113060: generated
  command verification rejects the explicit Rust-origin cfg fingerprint before
  Release compilation. Preserve exact M1/fatal-warning checks and require only
  the actual composed root SHA-256 flag, analogous to the Linux correction.
  Add standalone/composed/stale/duplicate/extra/CPU/warning command-policy tests
  and require them before the macOS workflow's native build.
- Inspect terminal 367a27d Windows renderer 42401824486/job 128491201738: the
  native file fixture fails compiling Core::System::rename, unavailable on Windows.
  Use the existing native UTF-8 FileSystem::rename_without_replacing with explicit
  probe linkage. Keep the actual descriptor-preserving path replacement test on
  Windows; do not skip it or add a platform pretend-success path.

Validation: generated M1 policy test and formatting pass locally; the full native
file-body/request/frame/separate-child gate passes on macOS with the portable
rename call. No GUI or Ladybird change. These are fixes for observed failures,
not new macOS Release/Windows acceptance. Fresh hosted runs remain required.
Full Code-OSS/resource/renderer and original cross-platform goal remains active.

## 2026-11-07: Adapt file frames to real LibWeb pending responses and streams

- Inspect pinned FetchController/FetchBodyDeliveryGate/FetchByteChannel and reuse
  their actual body/abort machinery. Add a native response owner with GC roots,
  epoch/request/state/HEAD/length/credit validation and metadata-to-response fields.
  Reject cancelled admission and an empty native fetch task destination.
- Keep the owner inside LibWeb because PendingResponse methods are not exported
  upstream. Export only an owned native class/callback boundary; do not patch
  upstream or copy its pending-response implementation. Separate pure codec/limits
  from checked files/request owners and link only the codec into LibWeb. Native
  dylib symbol inspection finds no FileBody/FileRequests globals in LibWeb.
- Use upstream 32/64-KiB channel watermarks and explicit remaining-credit accounting,
  not a parallel JS stream or whole-file buffer. Preserve real response status,
  URL and headers; HEAD has neither body nor producer credit. Require exact normal
  completion length and release native pending/controller/params/proxy roots.
- Install body transport before metadata so actual controller abort reaches it
  before headers. Use a weak transport proxy to avoid a native owner/gate/channel
  retention cycle, and retain the callback owner across reentrant cancellation.
  Terminal paths cancel once and release VM ownership. Error the body channel on
  transport failure: ConsumerCancelled alone does not settle an upstream reader.
- Add actual non-GUI principal-realm/document/FetchParams/controller/ReadableStream
  tests: a real checked 4 MiB + 19 byte file passes through FileRequests, framed
  metadata/body, real PendingResponse and the upstream stream reader. Check all
  bytes, status/MIME/length/cache headers, bounded credit, completion and GC between
  chunks. Native task destination is initialized as actual fetch requires.
- Test controller abort before headers and with queued body bytes, reader cancel,
  HEAD, empty success, dropping the native owner, reentrant credit cancellation,
  producer over-credit, wrong epoch/ID/offset, missing/duplicate/malformed headers
  and early completion. All failures settle actual pending responses or readers.
- Require the component in macOS Release/check-firefly and native Linux/experimental
  Windows renderer workflows. Add ADR 0116 and maintain the distinction between
  actual engine response adaptation and a real URL/page/renderer request service.

Validation: local native Release file-response/LibWeb component gates and Firefly
product compilation pass, without GUI. Existing file-body/request/frame/peer-loss
checks still pass after separating the codec library. Full source formatting,
site, upstream/diff checks pass (214 pages / 15,130 local links/resources/fragments). No Ladybird
source, standalone Mite API or page/native capability changes. Fresh Linux/Windows
component CI remains required. Scheme fetch dispatch, live helper/document admission,
request/credit transport/multiplexing, navigation/worker policy, lifecycle deadlines,
public protocol/session handlers and real renderer security acceptance remain open.
Code-OSS still has no accepted workbench launch. Full original goal stays active.

## 2026-11-07: Connect native protocol responses to actual JavaScript fetch

- Keep Code-OSS startup as the first execution path: protocol/resource loading,
  then real bootstrap failures, workbench binary bridge, editing/saving and native
  extension/terminal/platform acceptance. Website and unrelated example breadth
  remain behind this path; no successful workbench launch is claimed.
- Add a second exact, fingerprinted seam to the existing owned Fetching.cpp
  composition. Preserve upstream cancellation/policy/filtering and all original
  bytes; reject missing/duplicate seams. Ladybird stays unchanged.
- Add a native-only scoped provider slot, default empty in product/diagnostic
  helpers. Admit registered standard schemes only for same-origin top-level Window
  GET/HEAD. Refuse Range, navigation, worker/worklet and other request scope.
  Preserve upstream builtin/unknown-scheme handling. Retain the provider across
  reentrant revocation and stop the returned response when revoked during callback.
- Extend the non-GUI native gate to invoke actual global JavaScript fetch,
  arrayBuffer and AbortController in a real principal realm/document. A native
  same-process fixture sends checked real-file frames through the existing response
  adapter. Verify all 4 MiB + 19 bytes, HEAD, forbidden-header filtering, early scope
  refusal, abort releasing the pending file job, absent/revoked/duplicate providers,
  upstream data: fetch, reentrant cancellation and registration slot destruction.
- Preserve engine microtask assertions: save/restore the principal test realm's
  baseline context around final event pumping, as upstream spin_until does. Require
  the actual JavaScript acceptance marker in the Python gate. Add ADR 0117 and
  refresh current status, Code-OSS priorities and the upstream-update guide.

Validation: local macOS Release native file/frame/separate-child and actual
LibWeb/JavaScript fetch gates pass; Firefly product compiles without a GUI launch.
Formatting, upstream/source preservation and site validation are required before
commit. Existing e246fbf hosted Linux/Windows CLI and source/site jobs pass; its
Windows renderer now compiles the file fixture but fails opening the Unicode CLI
fixture path (job 128498692034), before origin/renderer gates. Diagnose this real
platform boundary next; do not drop the Unicode test or claim Windows support.
Live helper/document authorization, request/credit IPC, teardown/deadline wiring,
public protocol/session APIs and real renderer resource acceptance remain open.
The provider fixture is same-process only. Full original goal remains active.

## 2026-11-07: Preserve Unicode arguments in native protocol fixtures

- Inspect completed e246fbf Windows renderer run 42404341904/job 128498692034.
  The portable rename correction now compiles; check-firefly-protocol-files fails
  at its first real Unicode metadata path with ENOENT, before renderer acceptance.
- Upstream LibMain builds StringViews from narrow CRT argv. Recover the real wide
  command line inside our two protocol test entry points, convert strictly to
  owned UTF-8, then publish stable argv/views for the fixture lifetime. Keep the
  existing Unicode directory/file inputs. Add an independent argument roundtrip
  operation before file IO to distinguish entry encoding from file-open failures.
- Link only these Windows fixture executables to shell32; retain upstream entry,
  runtime initialization and all product files unchanged. This is fixture argument
  repair, not a claim that all product/helper Unicode entry paths are accepted.

Validation: local macOS native argument/file/frame/separate-child and real
JavaScript/LibWeb fetch gates pass without GUI; Windows requires the fresh hosted
roundtrip and actual file tests. Keep any later failure visible, with no Unicode
skip or permissive decoding. Source style/upstream/site checks are required before
commit. Code-OSS live resource transport and public protocol are still first;
full original goal remains active.

## 2026-11-07: Add main-side resource requests over real native IPC

- Continue the Code-OSS resource path, after actual JavaScript scheme fetch. Add
  FFQP bounded Get/Head/Credit/Cancel packets beside the existing response codec:
  exact little-endian identity/length/reserved fields, strict UTF-8 URLs and
  64-KiB grants. No file path, arbitrary headers or native handles on the wire.
- Add a separate main-side FileServer over the existing bounded byte channel.
  Its trusted native caller binds one admitted transport/epoch; an authorization
  callback alone selects the checked path/headers. Consume IDs even for refusal,
  refuse replay/foreign epochs/future grants/over-credit, bound eight jobs and
  retain existing global checked-file quotas. No product service or public API
  is installed, and this internal synchronous callback is not Electron handlers.
- Send metadata before body credit; round-robin one chunk/frame per deferred task,
  pause on actual send-window exhaustion and resume on transport acknowledgement.
  Consumer body credit remains separate. Cancellation, disconnect, malformed
  packets and native/reentrant revocation release files/jobs permanently.
- Apply native one-second sweeping with a fixed 30-second job/channel budget,
  including completed/refused jobs stalled behind transport credit. Grants do not
  refresh deadlines. Add an actual parent/child request consumer with byte-exact
  4 MiB + 19 byte files, parallel GET/HEAD/refusal, no-credit pause, cancellation,
  peer loss, forceful child termination, native/reentrant revocation, quotas,
  malformed/foreign/replayed/future/over-credit messages and a real deadline wait.
  Independent Python struct/UTF-8/budget controls cover both codec directions.
- Require the service through existing check-firefly-protocol-files, including
  macOS Release, Linux and experimental Windows renderer gates. Keep C++ at the
  existing IPC/file boundary, with no additional FFI, LibWeb or Mite dependency in
  the server. Update ADR 0118, current status and Code-OSS resource priorities.

Validation: local macOS Release two-process resource service, file/frame/child,
byte-channel and actual LibWeb/JavaScript fetch gates pass; Firefly compiles.
No desktop windows launched. Full source formatting, upstream preservation and site
validation pass (216 pages / 15,267 local links/resources/fragments).
Live helper/document admission, renderer
request-client/response-owner connection, public protocol/session routing and real
renderer navigation/security acceptance remain open. Code-OSS still fails at its
missing protocol export; no workbench launch or native Windows/Linux service
acceptance is claimed. Full original goal stays active.

## 2026-11-07: Connect actual JavaScript fetch to the native resource process

- Add a document-bound native Client linking the real Response owner to the
  existing URL request/credit/cancel channel and main-side FileServer. Keep the
  client outside LibWeb and the checked-file/server owner outside the engine.
  Move shared request quotas to the pure Limits header. Mite/Ladybird unchanged.
- Reuse the scheme seam's native eligibility function. Require an exact native
  local settings identity as well as same-origin active top-level document scope.
  Recheck activity/origin on receive/send/sweep. Creating a client installs no
  provider, helper capability or public API; product helpers still create none.
- Bound eight jobs and monotonic IDs, round-robin queued request/control frames,
  and separate channel ACK credit from actual engine consumer credit. Before-wire
  abort removes a job with no file admission. After-wire cancellation releases VM
  state and retains only a bounded retiring job until its terminal frame arrives.
- On malformed/foreign/future responses, transport failure, native revocation,
  owner destruction or fixed 30-second expiry, stop all real responses/streams,
  close transport and release document roots. Retain response ownership across
  callbacks; use weak client callbacks/timers and no map references after delivery.
- Add a real parent/child JavaScript gate: only the parent knows/opens the authorized
  Unicode file. Actual global fetch/arrayBuffer verifies all 4 MiB + 19 bytes over
  IPC, with parallel HEAD/refusal and forbidden-header filtering. Eight bodies
  finish while a ninth rejects. A second real same-origin document is refused.
  Real AbortController, zero-admission early abort, reader.cancel, native revoke,
  dropping the client after headers, peer loss and malformed/foreign/future frames
  settle promises/streams. A silent raw peer tests the client's own real deadline,
  independently of FileServer. Require native admission counts, reaped child status,
  JavaScript markers, GC at wait boundaries and absent Node/Firefly page globals.
- Require the new channel gate through check-firefly-protocol-fetch, preserving
  existing macOS Release/Linux/experimental Windows workflow entry points and old
  engine/stream tests. Add ADR 0119 and refresh status/Code-OSS resource documents.

Validation: local macOS Release channel/JavaScript, existing response/stream/fetch,
file/service/frame/child gates and Firefly product build pass without GUI windows.
Full source formatting, upstream preservation and site checks pass (217 pages /
15,335 local links/resources/fragments). Native Windows/Linux execution remains
a fresh hosted gate. The child is a native
LibWeb fixture, not a live sandboxed WebContent helper: trusted bootstrap/document
routing, navigation/teardown wiring, public protocol/session handlers and actual
renderer security acceptance remain open. Code-OSS still lacks its protocol export
and accepted workbench launch. Full original specification goal remains active.

## 2026-11-07: Classify registered standard schemes for native navigation routing

- Inspect actual helper bootstrap and navigation before wiring the live resource
  channel. The current document Client cannot create the first vscode-file document.
  LocalNavigable first branches on shared is_fetch_scheme; LibWebView's actual
  is_url_handled_internally uses that predicate too. Native custom origin identity
  and JavaScript component fetch alone do not make initial navigation reach fetch.
- Extend the existing origin composer with one exact pinned LibWebCommon
  Fetch/Infrastructure/URL.cpp seam. Registered standard schemes are fetchable;
  default, sealed, nonstandard and unknown schemes retain upstream behavior.
  Preserve FETCH_SCHEMES/HTTP_SCHEMES/LOCAL_SCHEMES constants, builtin predicates,
  full source hash/notices and exact byte restoration. No vendor modification,
  duplicate policy table, FFI or standalone Mite dependency.
- Expand native origin checks and add an actual LibWebView/Common routing probe:
  input parsing/classification and native internal URL handling agree before and
  after registration, including insecure standard vs secure-only nonstandard,
  query/fragment, unknown URLs, HTTP/HTTPS/file/data and mailto. It creates no view,
  window, navigation or external-app request. Require it through the existing fetch
  release/Linux/experimental Windows gate, keeping the lightweight origin gate.
- Record ADR 0120 and update status/Code-OSS/upstream update instructions. Navigation
  remains denied by the provider seam, and no public protocol flags/handlers appear.

Validation: local macOS Release native origin/routing gates pass. Existing actual
headless diagnostic and production-helper policy/DOM-origin regressions pass, with
no desktop windows. Actual JavaScript/native-channel regression and Firefly product
compilation pass, as do upstream/site checks (218 pages / 15,403 local links/
resources/fragments). Full source formatting is checked before commit. Fresh native
Windows/Linux classification evidence remains required. Previously confirmed
8d38e34 macOS hosted Release run 42406993506 is still in progress at observation;
no new alpha acceptance follows from local component results.

Next: implement native browser-initiated top-level navigation admission with exact
expected URL/navigable/replaced-client/reserved-client scope, real response policy
and body/controller lifetime across document commit; then wire helper bootstrap
and post-commit document routing. Do not loosen document Client guards: its source
becomes inactive at commit while navigation body delivery may still be pending.
Public protocol/session handlers and real Code-OSS workbench remain unimplemented.
Full original specification goal remains active.

## 2026-11-07: Populate native navigation responses through the resource channel

- Add a separate optional native navigation provider and one-shot Navigation scope.
  Existing document providers still deny navigation. Require current native
  top-level navigable/navigation ID, exact target URL, replaced client ID and
  reserved environment URLs, browser-UI null source client, GET/manual/document
  shape and no body/Range. No page/remote-client/worker/subframe permission.
- Reuse Client's bounded request/credit transport with distinct scope lifetime;
  keep its document admission unchanged. Native scope replacement/destruction,
  controller cancellation, peer failure and fixed expiry tear down actual bodies.
  Preserve the observed destination's identity for eventual post-commit lifetime;
  those commit branches still require real UI/helper acceptance.
- Inspect actual upstream population serialization: without a RequestServer lease
  it fully reads a body into the navigation descriptor before commit. Bound initial
  navigation metadata to 16 MiB before body/channel credit. Add a shared response
  maximum-length parameter with unchanged 64-GiB resource default. No zero-copy
  navigation or broader file privileges claimed; Ladybird/Mite remain unchanged.
- Add an actual parent/child upstream LocalNavigable population gate, using the
  exported native UI snapshot deserializer. Upstream generates the real fetch
  request/controller/reserved client. Only the parent opens the Unicode HTML file.
  Verify all 256 KiB + 19 body bytes, computed app origin, native identity and parsed
  response CSP. Exact 16 MiB succeeds; one byte over refuses without body delivery.
  Missing explicit handler, serialized page source, wrong URL, a second real native
  navigable with the same URL/ID and replay refuse without extra file admissions.
  Revocation/replacement after headers, peer loss and oversize settle population
  and release all jobs/files. Observe response state without consuming the engine's
  single PendingResponse callback; keep its assertions and vendor code intact.
- Require navigation through existing check-firefly-protocol-fetch and add ADR 0121.
  Refresh current Code-OSS/status/internal integration docs with the exact boundary.

Validation: local macOS Release navigation, prior JavaScript/native-channel and
response/stream gates, native file/service checks and Firefly product build pass
without GUI. Full source style/upstream/site checks pass (219 pages / 15,471 local
links/resources/fragments). Native Windows/Linux require fresh hosted execution. The source stays about:blank/opaque;
only navigation response population is accepted, not an app DOM load or commit.

Next wire native control/bootstrap before actual helper population (the scope
requires the native ongoing navigation ID already installed), then exercise the
real UI/helper document commit and bind post-commit resource channels to that exact
new document. Production helpers still install no resource provider/client/service.
Public protocol/session handlers and a real Code-OSS workbench remain open. Full
original specification goal remains active.

## 2026-11-07: Load a real custom-origin DOM through the production helper

- Extend the existing guarded WebContentPage composition with a call immediately
  before async_create_navigation_params. The native UI owns the population request
  there; its optional handler sends a trusted endpoint before the original message
  on the same connection. Retain page/loader, weakly track the native navigable and
  recheck current worker/navigation/loader after callback reentry. Require both exact
  seams, fingerprint/full restoration and drift rejection, without touching Ladybird.
- Append a second private NotRouted control message via the pinned upstream IPC
  generator. Keep all original IDs and the first private byte ID unchanged. Carry
  page/navigable/native navigation ID/URL/epoch plus an owned transport; preserve a
  single callback registry in LibIPC without adding a LibWebCommon dependency.
- Add an owned helper resource session after VM initialization. Validate agent
  thread, strictly increasing epoch, actual receiving ConnectionFromClient/PageHost,
  local top-level navigable and current one-shot Navigation scope before activation.
  Duplicate IDs cannot replace the original; bound 32 live roots, sweep destroyed
  records, retire completed clients, and expire unused handoffs after five seconds.
  Preserve last ID per live root and remove all callbacks/provider/clients on shutdown.
  The provider's document handler stays empty. Use the canonical staged IPC header
  alongside generated endpoints; source/copy declarations must not both be included.
- Add a real native UI/HeadlessWebView -> production helper gate. Parent alone knows
  the Unicode HTML path and authorizes it. Upstream performs actual population,
  descriptor handoff, document commit and parsing. Executed page script proves real
  app://bundle origin, secure context and absent Node/Firefly globals, with native
  source/document PID evidence and exactly one file admission. No origin assignment,
  direct population call or location getter substitution in this probe.
- Wrong ID, wrong page and no handoff open no file; duplicate handoff closes only
  its extra endpoint. Reentrant cancellation loads a replacement data: document
  with no stale app file admission and preserves the callback's borrowed request.
  Wait for helper-driven channel retirement, require no pending
  main jobs/files and query the live document again afterward. Defer that follow-up
  past the current page-info callback so upstream can clear its single pending slot.
- Require the new headless gate in check-firefly/macOS Release and experimental
  Linux runtime CI. Add ADR 0122 and refresh current status, Code-OSS and upstream
  ABI/update documentation with the boundary between native proof and public API.

Validation: local macOS Release real production-helper custom DOM/capability/refusal/
retirement gates pass without desktop windows. Existing actual diagnostic byte
handoff/PID/cookie, diagnostic/production origin policy/DOM, native navigation and
JavaScript/process fetch/stream/abort tests pass; Firefly compiles. Full source style,
upstream and site checks pass (220 pages / 15,539 local links/resources/fragments).
Fresh Linux runtime and shared Windows compilation remain required; no Windows
production-helper support claimed.

Next bind post-commit resources to the new exact document before scripts/styles
request them, including selected-process swaps and teardown. The normal main bridge
still has no public protocol/session authorizer, and the helper grants no document
resource capability. Implement those real contracts, then resume the pinned Code-OSS
bootstrap. No workbench launch is accepted. Full original goal remains active.

## 2026-11-07: Load committed-document resources through the production helper

- Add a third guarded WebContentPage seam before the selected host's original
  async_populate_navigation. Transfer the actual upstream destination document ID,
  retaining loader/weak navigable and rechecking current host/navigation/loader
  after callback reentry. Preserve source fingerprints, exact byte restoration
  and missing/duplicate seam refusal; leave Ladybird unmodified.
- Append a third private NotRouted document message without changing original or
  previous private IDs. Carry page/navigable/navigation/URL/epoch plus document ID
  and the native endpoint through the shared retained LibIPC callback registry.
- Admit pending resource grants only for the receiving connection's real page and
  registered standard URL. Bound them to 32, require increasing epochs, reject
  duplicates, and expire unused grants after five seconds. Resolve actual native
  local Window/settings on the first fetch; require exact document/page/navigable/
  URL/origin and fully active state before creating the existing strict client.
  Keep active document channels across idle periods; sweep inactive clients and
  revoke pending/bound grants on shutdown. Navigation retirement stays separate.
- Extend the real native UI/production-helper headless gate with external ESM and
  a nested import, applied CSS, every-byte verification of a 4-MiB-plus binary GET,
  HEAD length/empty body, explicit route denial, filtered Set-Cookie and absent
  page capabilities. Require selected-helper PID evidence. Wrong document ID and
  missing grants open no resource files; duplicates close only their extra peer.
  Real navigation to a replacement data: document revokes the old resource channel
  with no pending jobs/files and leaves the document/control path alive.
- Add ADR 0123 and refresh current status, VS Code priorities and the upstream
  update guide. The already-required headless resource target includes this gate
  in macOS Release and experimental Linux runtime CI. No desktop windows opened.

Validation: local macOS Release resource navigation (six modes) and committed
resource loading (five modes) pass. Actual diagnostic byte handoff, diagnostic/
production helper origin policy, native navigation and JavaScript/process fetch/
stream/abort gates pass; Firefly compiles. Full source style and upstream checks
pass; site validation passes (221 pages / 15,609 local links/resources/fragments). Fresh hosted Linux execution and shared Windows
compilation remain required; no Windows production-helper support claimed.

Next implement public protocol/session registration and main-side route ownership,
connect it to these native navigation/document grants, then rerun the actual pinned
Code-OSS main entry and follow its next failure. The ordinary main bridge still
installs no public authorizer or automatic grants. No Code-OSS workbench launch is
accepted. Full original specification goal remains active.

## 2026-11-07: Publish partial main-process protocol origin registration

- Export one protocol singleton through actual Firefly main ESM/CommonJS and the
  explicit Electron alias. Maintain its implementation in a commented ESM module;
  regenerate pinned native adapters. Expose registerSchemesAsPrivileged with
  standard/secure origin flags; reject requested unimplemented privileges and
  secure nonstandard origins, unknown options/types/names and duplicate schemes.
  Validate the whole batch before publication; failed input consumes no state.
- Add a private LibJS/LibURL registration binding that independently validates the
  existing bounded native schema and publishes the original immutable process
  table. Preserve native once-only/helper-launch sealing plus public pre-readiness
  checks. No file route, response handler, session or automatic document grant is
  created. No registration API reaches renderer/preload, standalone Mite or vendor.
- Add a real Mite/LibJS/native URL registration gate without Qt/LibWeb. Exercise
  valid/empty/late/sealed processes, atomic batch refusal, native malformed/budget/
  type refusal, real secure/normalized/opaque origins, owned helper snapshots and
  second-factory native refusal. Add actual Firefly main ESM/CommonJS registration/
  readiness acceptance without a BrowserWindow. Update the existing alias fixture
  to require the new shared export while retaining unsupported Menu absence.
- Require both gates in macOS Release. Configure experimental Linux's runtime
  before running its native binding gate; exercise the main host with Xvfb only
  after runtime compilation. Add ADR 0124, detailed public API and updated current
  startup/support/roadmap references.

Validation: local macOS Release native registration (four modes), actual Firefly
main-host singleton/registration/readiness and Node/Mite geometry regressions pass;
Firefly compiles. Full source style, generated-adapter freshness and upstream checks
pass; site validation passes (223 pages / 15,750 local links/resources/fragments).
Actual pinned Code-OSS main with frozen root production modules
now resolves protocol and fails on missing crashReporter (exit 1). This is import
link progress, not execution of Code-OSS's registration call or desktop startup.
That call also asks for still-unsupported ServiceWorker/CORS/fetch/cache privileges.
No BrowserWindow was created. The minimal Qt SDK has no offscreen plugin; its
unsupported-plugin refusal was observed, then the actual shipped native starter
was used. Fresh hosted Linux execution remains required; no Windows desktop claim.

Next connect real public resource/session handlers and automatic navigation/document
grants to the native paths; implement demanded privileges and main services/imports,
including crashReporter/Menu/contentTracing, based on actual Code-OSS requirements.
Keep explicit refusals instead of fake exported objects or silent privilege no-ops.
No workbench launch is accepted. Full original specification goal remains active.

## 2026-11-07: Support asynchronous native protocol file authorization

- Inspect the actual pinned Code-OSS ProtocolMainService: its workbench route uses
  session.defaultSession.protocol.registerFileProtocol and an application callback.
  Remove the native service's synchronous-only authorization requirement before
  connecting that callback to main-VM dispatch; do not add fake crash-reporter exports.
- Split file admission from authorization. Reserve the existing monotonic identity,
  eight-document/64-process lease and fixed 30-second deadline before any application
  response. Keep pending jobs unopened, permit out-of-order replies for admitted IDs,
  preserve HEAD and deadline, and release leases on every failed/terminal path.
  Keep begin as the original synchronous adapter; expose separate pending/file counts.
- Add FileServer.create_async with owned ID/URL/method, native completion and optional
  cancellation notification. Keep create on the same state machine. Insert waiting
  state before callback, withhold metadata/body until authorization, preserve bounded
  credit and refuse duplicate/stale results before opening a file. Retain callback
  ownership across reentry and invalidate weak completions before terminal/destructor
  notifications, so reentrant callbacks cannot revive dying owners. Wire unchanged.
- Add nine real separate-process service modes: delay, duplicate, reverse-order
  GET/HEAD/refusal, cancellation while waiting, revoke, owner destruction, forced
  peer death, pending quota and actual unanswered 30-second timeout. Add native file
  reservation/out-of-order/shared-quota/refusal tests. The production-helper document
  resource authorizer now defers to another native task and retains real ESM/CSS/
  binary/HEAD, identity and navigation-teardown checks. No desktop windows opened.
- Add ADR 0125 and refresh service/status/VS Code documentation. Existing release and
  Linux/Windows file-service gates automatically include the new cases; Ladybird,
  public JS exports and standalone Mite dependencies remain unchanged.

Validation: local macOS Release old/new separate-process file-service and file-owner
checks pass, including both actual 30-second deadline cases. Production-helper
navigation/document resources pass with asynchronous authorization. Native navigation
and JavaScript/process fetch/stream regressions pass. Final owner-thread completion
guard builds and delayed/order/cancel/destructor cases pass. Full source style and
upstream checks pass; site validation passes (224 pages / 15,819 local links/resources/
fragments). Fresh hosted execution remains required; no Windows production-helper claim.

Next connect public default-session file-route registration, queued main-VM callback
request/reply ownership and automatic navigation/document grants to this native path.
Additional demanded privileges and crashReporter/Menu/contentTracing/main services
remain open. Code-OSS's last actual import still fails at crashReporter (exit 1);
this native service step is not public callback support or workbench acceptance.
Full original specification goal remains active.

## 2026-11-07: Load public default-session file protocols through real windows

- Add public registerFileProtocol/unregisterProtocol/isProtocolHandled and the same
  main singleton at session.defaultSession after readiness. Add session to real
  Firefly/Electron-alias ESM/CommonJS exports. Support explicit supportFetchAPI on
  standard schemes; require that permission for file routes, including navigation,
  so standard-only registration does not accidentally grant Window fetch authority.
  Keep other demanded privileges explicitly unsupported and native origin schema
  unchanged. The public URL loader accepts only live registered custom routes.
- Add a native main-owned FileRoutes service with scheme generations, owned
  canonical window roots, bounded channel/pending-token maps and helper grants
  before upstream navigation/document population. Require exact initial URL and
  same-origin resources. Use the actual selected page/document identity and the
  existing strict helper/settings validation. Keep Ladybird unmodified.
- Queue URL/method metadata through main VM-owner dispatch, never call JavaScript
  from transport callbacks. Preserve native paths/bodies on main and stream bytes
  through the existing file service. Validate response metadata/path/header budgets
  independently; preserve header bytes, actual MIME/length and cookie filtering.
  Notify pending loads on actual file-open/header failures, grant failure and explicit
  denial. Drop cancelled queued requests before entering even a busy VM; only
  callbacks actually delivered to JS need cancellation messages. No fabricated
  request headers or initiator fields enter application code.
- Bind application callbacks to native tokens/generations. Reject duplicates/stale
  replies, settle throws/rejected promises and revoke channels/pending requests on
  handler removal, replacement navigation, owner loss and shutdown. Extract protocol
  hook lifetime into bridge/Protocols.cpp. Clear callbacks/timer/hooks before VM
  teardown. Re-registration currently needs navigation/reload for a fresh document
  channel; do not claim in-place rebinding.
- Add pure Node/Mite callback-policy tests and one bundled public hidden-window
  gate. Ordinary delayed JavaScript handlers load real HTML/nested ESM/CSS and
  every byte of a 4-MiB-plus GET/HEAD; deny cross-origin/POST without main admission,
  preserve absent page capabilities and refuse duplicate callbacks. Verify explicit
  denial, actual missing files, invalid headers, pending navigation/document removal,
  stale generation refusal and successful replacement navigation. No test-only path
  authorizer/endpoint is installed in this public gate.
- Require policy/public gates in macOS Release and experimental Linux's existing
  native Xvfb runtime stage. Add detailed protocol/session references and ADR 0126;
  refresh loading/compatibility/roadmap/native service docs with exact current scope.

Validation: local macOS Release actual public handler/window batch and main-host
singleton/readiness tests pass; four real LibJS/native registration modes and
Node/Mite callback/geometry policy pass. Existing native file/server/navigation/
JavaScript-process fetch/stream regressions pass, including real 30-second deadlines.
Actual pinned Code-OSS entry with frozen root production dependencies still exits 1
at missing crashReporter. No workbench startup is accepted. Full source style,
generated-adapter and upstream checks pass; site validation passes (226 pages /
15,962 local links/resources/fragments).
Fresh hosted Linux execution remains required; no Windows desktop/helper claim.

Next implement the remaining demanded protocol privileges and general Response/file:
interception, main startup services/imports (crashReporter/Menu/contentTracing and
required app methods), then rerun actual Code-OSS. Broader sessions/partitions,
request metadata, workers/frames and live document rebinding remain open.
Full original specification goal remains active.

## 2026-11-07: Reuse native local crash reporting through the public main API

- Inspect the actual pinned LibCore/LibWebView crash infrastructure instead of
  adding Crashpad or exporting an inert crashReporter. Add real local-only start,
  disabled-upload state and initial annotation snapshots, plus app.getPath/setPath
  for crashDumps only. Export the actual main singleton through Firefly/explicit
  Electron alias ESM/CommonJS. Require uploadToServer: false and explicitly reject
  uploads, OS-handler suppression and unknown options. Broader methods stay absent.
- Reuse upstream signal-safe descriptors, bounded native stacks, private report
  storage, locking/recovery and OS signal termination. Add exact guarded default-
  directory/build-description/helper-save seams in two pinned source compositions;
  restore all other bytes and notices, reject fingerprint/unique-seam drift and
  keep Ladybird untouched. Extract a reusable translation-unit ownership helper
  instead of coupling the crash composition to an origin-output path.
- Publish one bounded immutable native path/configuration with no VM references.
  Preserve main-only extra vs globalExtra and global precedence. Encode Unicode
  as JSON escapes because upstream raw build descriptions require ASCII; retain
  crashed-launch metadata on next-launch recovery. Add frozen global metadata
  when a pre-configured helper's surviving parent saves its report. Never reset
  partially installed signal state or silently consume failed initialization.
- Add Node/Mite policy checks and actual public crash tests. Externally signal
  only the test's owned main/helper PIDs; process.kill/process.crash remain absent.
  Recover a real main SIGABRT on a clean next launch; verify original Chinese/
  emoji/umlaut metadata, private 0700/0400 modes, ownership, no environment/path
  sentinel and raw-record cleanup. Create one hidden-window helper before start,
  verify its PID ancestry, crash it, observe render-process-gone and inspect its
  actual saved report: global annotations only, no main-only extra. No synthetic
  report file or test-only crash backend stands in for the public API.
- Require the gate in macOS Release and Linux's existing Xvfb runtime stage.
  Document local text vs minidump/upload semantics, strict metadata bounds,
  frozen fields, shared default folder, unsupported dynamic extras/report lists/
  Windows capture and crashDumps-only paths. Add ADR 0127 and refresh current
  API/status/Code-OSS/upstream-update references.

Validation: local macOS Release actual public main crash/recovery and owned-helper
capture pass, including the early-helper fallback. Node/Mite option/snapshot and
existing geometry policy pass. Actual Code-OSS now resolves crashReporter and
fails at missing Menu (exit 1); this remains import progress, not a workbench run.
Final source style, generated-adapter freshness and unmodified pin checks pass;
site validation passes (228 pages / 16,108 local links/resources/fragments). The
shared source-ownership helper and final API bundle compile successfully. Fresh Linux capture/shared Windows compilation remain required;
no Windows crash/desktop support is claimed.

Next implement real Menu/contentTracing and demanded app/main services, continue
required protocol privileges/Response/file interception, and rerun actual Code-OSS.
Uploads, report listing and dynamic/per-renderer annotations remain unfinished.
Full original specification goal remains active.

## 2026-11-07: Reconcile remaining wrapped Code-OSS status references

Update the guide, roadmap, earlier status paragraph and protocol reference to the
verified Menu import blocker after local crashReporter landed. Keep ADR 0126's
historical evidence and link its subsequent ADR 0127. Validate generated site links;
this is documentation only and requires no further desktop/crash tests.

## 2026-11-07: Implement native application menus on the Code-OSS startup path

- Add maintained ESM Menu/MenuItem graphs and a separate bounded Qt native provider.
  Export real main constructors through Firefly and explicit Electron ESM/CommonJS
  aliases. Support early null suppression, application-menu installation/removal,
  nested normal/checkbox/separator/submenu items, lookup, properties and quit/close
  roles. Keep callbacks in the main VM, not Qt or the renderer.
- Independently validate exact native schemas, UTF-8/size/tree budgets and monotonic
  int32 identifiers. Queue actual native action signals through the existing bridge;
  discard retired generations, synchronize checkbox state across bars/new windows,
  and dispose actions before VM teardown. Use a native global macOS menu bar and
  per-window Linux bars. Keep Ladybird untouched and standalone Mite GUI-independent.
- Reject unsupported options, roles, accelerators, radio items, installed structural
  mutation and invalid ownership/graphs. Popup/context menus, lifecycle events and
  broader menu services remain absent. Keep the combined menus sample blocked on
  popup support and update its preflight so partial application support cannot be
  mistaken for full sample acceptance.
- Add Node/Mite graph policy, real Qt QAction-to-LibJS acceptance and an actual
  public main-host export/install/update/remove gate without a BrowserWindow.
  Exercise native schema/budget rejection, callbacks, disabled/hidden suppression,
  checkbox state, properties, roles and stale events. Bound the native probe's run
  time; its Qt pump exits when Mite work is idle rather than waiting indefinitely.
  Require both native gates in macOS aggregate checks and Linux's existing Xvfb
  runtime stage. Refresh API/status/example/Code-OSS docs and ADR 0128.

Validation: local macOS Release builds and both menu gates pass. Native policy and
existing BrowserWindow geometry policy pass in Node and actual Mite. The prepared
real Code-OSS main entry now passes Menu import and fails at missing contentTracing
(exit 1), recorded in build/code-oss-menu-entry.log; no workbench launch is claimed.
All 23 ESM example apps stage successfully; this does not run/accept their features.
Full maintained-source style, generated JS freshness, unmodified upstream pin and
whitespace checks pass. Site validation passes (230 pages / 16,254 local links,
resources and fragments). No broad desktop suite or physical menu-click automation
was run. Fresh Linux native acceptance, physical focus/modifier/menu interaction
and Windows desktop remain unverified/unfinished.

Next implement real contentTracing and demanded app/main services, rerun actual
Code-OSS, then continue protocol requirements and binary/event/port IPC/workbench
services from the measured blockers. Broader example coverage follows Code-OSS
needs. Full original specification goal remains active.

## 2026-11-08: Record real native main traces through contentTracing

- Replace the next measured Code-OSS missing export with useful main-owned native
  tracing, not an inert compatibility object. Record actual monotonic main-loop,
  bridge delivery and window create/destroy request scopes in Firefly categories.
  Keep traces main-only; do not pretend Chromium/V8/renderer sampling exists.
- Add a QtCore-only native recorder with independent exact schema/category bounds,
  8,192-event first/ring retention, explicit dropped-event counts, actual PID/logical
  thread metadata and generation retirement for scopes crossing stop/restart.
  Disabled tracing allocates no event strings. No JS roots or application payloads
  are retained by native scopes. Reset recording on bridge teardown; keep Ladybird
  unmodified and Mite independent of all graphical libraries.
- Expose promise-based getCategories/startRecording/getTraceBufferUsage/stopRecording
  through the real Firefly/explicit Electron main aliases. Support exact category
  inclusion/exclusion and the two bounded recording modes in modern/legacy shapes.
  Reject sampling, memory configs, Chromium categories and unknown options explicitly.
  Preserve active configuration on repeated starts, reject conflicting saves and
  retain failed output snapshots for retry with a new path.
- Supply the existing real main fs singleton to the API factory. Serialize bounded
  snapshots on the native owner thread, then write asynchronously through Mite.
  Default output uses a native private directory/exclusive 0600 file; explicit paths
  have ordinary overwrite semantics. Document caller-owned temporary files, failed
  partial outputs and JSON vs current Electron Perfetto protobuf differences.
- Add actual native clock/filter/capacity/retention/generation tests, Node/Mite policy
  and a public main-host/file-output gate without any BrowserWindow. Wait for a
  completed main pump before asserting buffer use, since an in-flight first pump
  is not yet an event. Require the gate in macOS aggregate checks and Linux Xvfb.
  Refresh API/status/roadmap/Code-OSS docs and add ADR 0129.

Validation: local macOS Release native recorder and actual public main tracing
pass. Inspect real durations/timestamps/PID, exact filtering, 0700/0600 permissions,
absence of a supplied environment sentinel/application path, output failure/retry
and ESM/CommonJS identity. Node/Mite tracing policy and existing window geometry
policy pass; the actual public menu-host gate remains green. Full maintained-source
style, generated JS freshness, unmodified upstream pin and whitespace checks pass.
Site validation passes (232 pages / 16,394 local links/resources/fragments).
The prepared actual Code-OSS entry now passes contentTracing import and fails at
missing app.commandLine.getSwitchValue (exit 1), recorded in
build/code-oss-tracing-entry.log. No desktop workbench or Code-OSS tracing command
is claimed. No browser/window suite or physical UI automation was run.

Next implement the demanded real app.commandLine/main services, rerun Code-OSS,
then prioritize required protocol/IPC/workbench/auxiliary-host services from its
actual blockers. Window/IPC trace instrumentation still needs dedicated real-
operation capture acceptance; renderer/helper tracing, sampling/heap diagnostics,
Perfetto protobuf encoding and fresh native Linux execution remain unfinished.
Windows desktop and the full original specification goal remain unfinished/active.

## 2026-11-08: Expose native command-line reads and effective supported engine switches

- Add main-only app.commandLine over a bounded native snapshot of real sanitized
  launch arguments, separate from process.argv and synthetic browser argv. Parse
  dash prefixes, first equals, last duplicate and '--' termination; keep positional
  values and executable-owned import policy distinct. Add maintained ESM validation,
  Qt/LibWeb-independent native State, LibJS bindings and an owned EnginePolicy.
- Implement before-ready lang through the actual public WebView settings/language
  propagation APIs, Unicode canonicalization and availability checks. Restore the
  pre-application language list on removal. Native refusal restores the previous
  switch snapshot. Seal mutations at native readiness; reject unsupported engine
  options/appendArgument explicitly rather than storing inert successful flags.
- Integrate valueless --force-cpu-painting into the native synthesized browser
  arguments before Application::create launches the compositor. The first acceptance
  exposed that the setting belongs to the already-started compositor, not WebContent;
  correct that early consumption boundary and reject later append/remove even before
  readiness. Inspect the actual owned compositor argv, not an unrelated helper or
  a late in-memory option assignment. Preserve sandbox policy and upstream sources.
- Add native parser/transaction/initial-policy/count/value/phase tests, Node/Mite
  JS policy and real public aliases/argv-preservation acceptance. Compare shared
  read observations with actual Electron. Batch language/compositor acceptance into
  one hidden-window fixture; do not rerun broad renderer/window suites. Add both
  native/public gates to required macOS/Linux runtime checks, update detailed API,
  current status/Code-OSS references and ADR 0130.

Validation: macOS Release build and native/Node/Mite command-line checks pass.
Shared read results match actual Electron. The actual renderer reports only de-DE;
its owned compositor argv contains --force-cpu-painting while helper/compositor
sandbox-disabling flags remain absent. Public checks cover aliases, native invalid-
language rollback, readonly singleton, early CPU-policy refusal, ready sealing and
unchanged process.argv. Invalid startup CPU values are refused before native app
creation. Existing real menu-host/contentTracing gates and Node/Mite geometry policy
remain green. Full maintained-source style, generated JS freshness, unmodified
Ladybird pin and whitespace checks pass. Site validation passes (234 pages / 16,536
local links/resources/fragments). Fresh Linux execution and Windows desktop remain
required; no wider engine-switch/platform support is inferred.

The prepared actual Code-OSS main entry now passes commandLine reads and fails
explicitly at appendSwitch('enable-features', ...) with ERR_FIREFLY_UNSUPPORTED_API
(exit 1), recorded in build/code-oss-command-line-entry.log. Chromium/Blink/V8
feature configuration has no direct LibWeb/LibJS equivalent. Next audit that pinned
application configuration for an explicit justified engine adaptation, then
continue demanded main/protocol/IPC/workbench services from actual startup errors.
The workbench, broader command-line switches and original specification remain
unfinished; the full goal remains active.

## 2026-11-08: Audit Code-OSS implicit Chromium defaults in an opt-in LibJS overlay

- Add explicit --libjs-engine to the existing hash-checked application overlay,
  independent of --libjs-cache. Preserve the pinned mainImpl source/license bytes
  outside four declared insertion seams: helper import, user-setting guard and
  the opening/closing of the exact implicit Chromium default tuning block.
  Keep the original Node/Electron branch, application argv handling and sandbox
  branch intact. Reject missing/duplicate/order/input drift before creating output.
- Own the engine policy under firefly/compat/code-oss, not a generic Electron or
  commandLine shim. On actual Mite only, record audited omitted Chromium/Blink/
  Electron tuning names with reasons in deeply frozen startup metadata. Include
  explicit engine-default-only scope and unestablished renderer compatibility.
  Do not invent equivalent engine settings, WebGL quotas, portal services or stack
  collection; the per-feature audit records what remains unverified/unimplemented.
- Refuse audited explicit user engine/V8/proxy/rendering/debug/portal and sandbox-
  disable requests from CLI, argv.json or native switch presence before upstream
  config/argv mutations. Distinguish minimist's mirrored default no-sandbox alias
  from an actual raw request using canonical sandbox plus native presence. Actual
  startup exposed this alias behavior; correct it without weakening raw/false-valued
  no-sandbox rejection. Preserve logging, proposed-API and secret-storage argv work.
- Emit the scoped engine status in the actual probe. Add a required native Mite
  CTest that checks exact reversibility, input/license preservation, seam/overlap
  refusal, unchanged original/adapted Node configuration calls, actual Mite default
  handling, frozen metadata and eight user/sandbox-negative cases before mutations.
  Add ADR 0131 and document the explicit opt-in vs ordinary entry behavior.

Validation: local macOS Release CTests mite-code-oss-engine/platform/cache all pass
(3/3). Compile the full real 11,535-source Code-OSS diagnostic with actual frozen
root production dependencies and verified owned-overlay hashes. After compilation
and dependency staging complete, the real Firefly host now reaches missing
app.enableSandbox (exit 1), recorded in build/code-oss-engine-final-entry.log, with
10 audited omitted-default entries on macOS. Without --libjs-engine the ordinary
entry continues to reject enable-features; generic Firefly switch behavior is
unchanged. The original sandbox branch was not bypassed. No workbench, sandbox-API
or terminal/WebGL/font/zoom equivalence is claimed. Full maintained-source style,
unmodified Ladybird pin, generated JS freshness and whitespace checks pass. Site
validation passes (235 pages / 16,610 local links/resources/fragments). No native
runtime source or upstream file changed. No browser window/physical UI test ran.
Fresh native Linux/Windows overlay acceptance remains required.

Next implement the real app.enableSandbox policy demanded by the unchanged branch,
then continue app paths/locales and protocol privileges from actual startup errors.
Renderer/workbench, native services/auxiliary hosts, Windows desktop and the full
original specification remain unfinished; the goal remains active.

## 2026-11-08: Require the native renderer sandbox through app.enableSandbox

- Add a main-owned native sandbox policy with early/idempotent enable, native
  readiness sealing, stopped-state refusal and launch-policy mismatch errors.
  Verify the actual BrowserOptions sandbox setting and independently recheck it
  before trusted window creation/preload I/O. Keep the existing default sandbox
  mandatory even without a global call. No unsandboxed fallback is introduced.
- Expose app.enableSandbox on the real main singleton/aliases. Set the JS global
  requirement only after native success; override boolean sandbox:false without
  modifying caller preferences. Keep nonboolean sandbox requests, Node integration
  and context-isolation weakening explicitly unsupported. No renderer/preload app
  export, fabricated process.sandboxed or main-process sandbox is added.
- Reuse Ladybird's actual OS sandbox/bootstrap/profile unchanged. Keep the native
  lifecycle policy independent of Qt/LibWeb and avoid any new standalone Mite
  graphical dependency. Add exact native/JS policy tests and one public hidden-
  window acceptance fixture with authored ESM/bundled isolated preload.
- Independently inspect the actual owned renderer after PID ancestry checks. Require
  sandbox launch flags to remain enabled and inspect OS state outside the page:
  macOS null-operation sandbox_check reports owned main 0 / renderer 1; Linux's
  hosted gate requires NoNewPrivs and additional seccomp filters over the main.
  The private macOS query exists only in tests and is sourced in ADR 0132.
  Validate the real frozen preload bridge/page privilege absence and acknowledge
  external inspection before shutdown instead of racing an arbitrary sleep.
- Require native/public gates in macOS aggregate and Linux Xvfb checks. Refresh
  app/BrowserWindow/general API, compatibility, Code-OSS/status/roadmap and ADR 0132.

Validation: macOS Release build, native latch/phase/mismatch/stop tests and actual
Node/Mite factory policy pass. Public default-mode rejection, early/repeated enable,
late-call refusal, sandbox:false override, unchanged preferences, actual preload/
page isolation and independent owned-renderer Seatbelt activation pass. Existing
real menu-host/contentTracing gates and Node/Mite window geometry policy remain
green. Source style, generated JS freshness, unmodified Ladybird pin and whitespace
checks pass. Site validation passes (236 pages / 16,685 local links/resources/
fragments). Only the one targeted hidden-window fixture ran; no broad desktop suite
or physical UI automation was used. Fresh Linux kernel acceptance and Windows
desktop remain required; this is not an exhaustive sandbox-permission/escape audit.

The actual Code-OSS entry with the explicit engine overlay now passes its unchanged
app.enableSandbox call and fails at app.setPath('userData', ...) with
ERR_FIREFLY_UNSUPPORTED_API (exit 1), recorded in build/code-oss-sandbox-entry.log.
Next implement real app paths/userData and demanded locale/main/protocol services,
then rerun the actual entry. Renderer/workbench, broader APIs, native auxiliary
hosts/platform packaging and the original full specification remain unfinished;
the full goal stays active.

## 2026-11-08: Bind real userData/sessionData before native profile startup

- Implement native app paths instead of returning overridden strings while keeping
  an old temporary browser profile. Construct Qt/Core and the main bootstrap first;
  seal selected paths, invoke upstream's protected initialization with the actual
  --profile-path, activate queued language/theme and only then deliver readiness.
  Enable upstream SQL-backed default cookie/Web Storage stores. Keep Ladybird
  unchanged and Mite independent of native desktop libraries.
- Own native argv buffers and Qt outside the application object so both outlive
  the full WebView base destructor, helper shutdown and database flushes. Early
  exits skip profile opening; initialization errors refuse ready and preserve
  existing files. No live profile/database rebinding or private-header trick is used.
- Add bounded native getPath/setPath for actual userData/sessionData/logs and
  standard read paths, getAppPath and recursive setAppLogsPath. Default sessionData
  follows userData unless explicitly selected. Keep crashDumps' independent rules.
  Use safe package-name/entry-directory identities and private new directories;
  document name/layout/macOS-log differences, symlinks, existing-directory setters,
  profile sealing and the absence of automatic migration or broader path APIs.
- Defer native language/theme settings until the chosen profile exists; retain
  launch-only CPU policy. Reset each new main's default theme source to system
  instead of inheriting a previous explicit dark override from persisted settings.
  Preserve actual native sandboxing, preload limits and protocol boundaries.
- Add native real-filesystem/Unicode/alias/rollback/budget/seal checks and a public
  profile gate. Select separate application/session roots, inspect real settings/
  SQLite files, and verify cookies/localStorage across two launches against the
  same actual HTTP origin. Check early language/theme, isolated page capabilities
  and independently inspect the exact owned renderer's OS sandbox. Test conflicting
  config-file startup failure without ready/deletion and persistent theme reset in
  no-window launches. Require gates in macOS aggregate and Linux Xvfb stages.
  Refresh detailed API/status/Code-OSS docs and ADR 0133.

Validation: local macOS Release build and final check-firefly-app-paths gate pass,
including actual cookie/localStorage survival, selected storage roots, owned-renderer
Seatbelt activation, profile failure preservation and theme restart behavior. Native
path checks cover real Unicode directories, bounds, invalid setters, independent
session selection, logs and lifecycle. Existing actual main/owned-helper crash,
command-line language/compositor and public file-protocol regressions pass in one
targeted batch. Actual no-window menu/contentTracing gates and Node/Mite geometry/
sandbox policy remain green. Source style, generated JS freshness, unmodified pin
and whitespace checks pass. Site validation passes (238 pages / 16,832 local links,
resources and fragments). Only targeted hidden acceptance was used; no broad desktop
suite or physical UI automation ran. Fresh Linux execution and Windows desktop,
profile concurrency/recovery/migration and broader storage APIs remain unfinished.

The real prepared Code-OSS entry with the explicit engine overlay and an existing
isolated --user-data-dir passes the path setup and reaches unsupported protocol
privilege corsEnabled (exit 1), recorded in build/code-oss-userdata-entry.log.
Next implement the actual required protocol privileges/handlers, continue demanded
locale/main/IPC/workbench/auxiliary-host APIs from measured errors, and rerun the
real entry. No workbench launch or full-platform support is claimed. The original
complete specification remains unfinished and the full goal stays active.

## 2026-11-08: Implement real simple custom-protocol CORS on the Code-OSS path

- Follow the user's VS Code priority. Implement corsEnabled as immutable native/
  helper policy for standard custom schemes, without editing Ladybird or accepting
  an inert privilege. Optional bounded schema-1 cors metadata defaults false and
  independently rejects malformed/nonstandard declarations.
- Extend the fingerprinted/reversible owned Fetch composition at the CORS scheme
  check and HTTP network seam. Preserve upstream main_fetch, CORS/filtering and CSP.
  Admit only active owned top-level custom-origin documents, simple GET/HEAD, CORS
  mode and default/omit credentials. Workers/frames, HTTP/file sources, include
  credentials, preflight/unsafe headers, POST and Range remain refused.
- Authorize every target URL through its actual main route generation. Derive the
  handler's frozen headers.origin from the native source binding, never renderer
  metadata. Keep same-origin request headers absent. Reject missing/mismatched/
  duplicate CORS authorization before any body credit; filter response headers
  through real LibWeb, retain bounded file streaming/HEAD/cancellation.
- A well-formed CORS refusal cancels only its request. Public production-helper
  acceptance caught whole-channel shutdown after the first refused response;
  correct it and verify subsequent denied and allowed requests on that channel.
- Track used target schemes. Removing a source/used target retires its document
  channel and pending requests; stale callbacks cannot affect replacement routes.
  Require source reload for a fresh grant. Document this conservative difference.
- Extend the actual native no-GUI fetch gate and existing batched public hidden-
  window acceptance with real 4-MiB CORS bodies, HEAD, origin/exposed/hidden/forbidden
  headers, origin mismatch/duplicates, disabled schemes, mode/credentials/preflight/
  ranges, CSP refusal, zero unauthorized body credit, target revocation and reload.
  Existing file loading, failures, navigation/document cancellation and generations
  continue to pass. The existing CI targets run these added cases.
- Refresh detailed protocol/API, matrix, status, roadmap, Code-OSS and upstream-
  update references, plus ADR 0134. The pinned upstream CSP host-source matcher
  refuses custom opaque hosts; explicit scheme sources work. Preserve that policy
  and document the limitation instead of bypassing CSP.

Validation: local macOS Release build, native registration/file callback policy,
actual LibWeb file/fetch/CORS and real public production-helper acceptance pass.
The real prepared Code-OSS entry with explicit engine overlay and isolated existing
userData passes corsEnabled and now fails at unsupported allowServiceWorkers
(exit 1), recorded in build/code-oss-cors-entry.log. No workbench or service-worker
support is claimed. Fresh hosted Linux and native Windows desktop remain open.
Source style, generated adapter freshness, upstream-pin, whitespace and built-site
checks are recorded with this commit. Only the targeted hidden protocol batch ran;
no broad GUI suite or physical UI automation. The original full goal remains active.

Next inspect the real vscode-webview service-worker demand against actual upstream
worker execution/resource routing, implement its real contract or record a measured
architectural blocker, then continue demanded protocol handlers/main APIs. Binary/
event/port IPC, workbench files and auxiliary hosts follow the actual Code-OSS path;
HTTP/2/3, unrelated example breadth and website polish stay below that priority.

## 2026-11-08: Separate first-workbench work from the audited worker execution gap

- CORS slice 544e07a is committed and pushed to master. Keep the full goal active.
- Inspect the actual pinned Code-OSS mainImpl/webview HTML/script/element and pinned
  Ladybird service-worker container/job implementation. Non-HTTP(S) registration
  is rejected; new-script jobs explicitly reject unimplemented Run Service Worker.
  Code-OSS requests worker capability for vscode-webview, code caches for both
  local schemes and privileges on nonstandard remote schemes. These are independent
  gaps, not booleans that can truthfully be accepted without implementation.
- The existing webview disableServiceWorker option skips host-resource worker
  messaging too; no usable fallback is established. No source adaptation added.
- Refine the first-workbench priority: investigate a separately explicit limited
  application mode before spending the entire first-launch path on full webviews;
  preserve ordinary Electron behavior and explicit missing-feature failures. This
  mode is planned, not implemented, and full webview/workflow support remains due.
- Prioritize independently required native locale methods next (preferred system
  languages before ready; system/app locale afterward), then demanded protocol/
  preload/binary-event-port IPC/window services from actual execution. First
  milestone requires a real local editor and file open/edit/save, with extension/
  terminal/webview workflows and native platform acceptance tracked separately.

Validation: source audit only, no new runtime/GUI/service-worker support claim.
Documentation/site link checks and git whitespace checks pass. Preserve unmodified
upstream and the measured real-entry failure at allowServiceWorkers (exit 1).

## 2026-11-08: Implement native locale APIs required by the Code-OSS bootstrap

- Previous goal turn made concrete progress: 544e07a implements/tested simple
  custom-protocol CORS and ad113bf records the worker gap/first-workbench priority;
  both are pushed. Continue independent real bootstrap APIs while preserving the
  full specification and exact unsupported worker/cache/remote-scheme boundary.
- Add main app.getPreferredSystemLanguages/getLocaleCountryCode before readiness,
  plus getSystemLocale/getLocale afterward. Native guards independently repeat
  readiness/selector validation. Every array is fresh VM-owned data; no Node/V8,
  renderer privileges or standalone Mite dependencies are added.
- Use Foundation directly on macOS for current regional locale/country and the
  original preferred-language order. On Linux, use existing Qt system data with
  explicit fallback/name differences; POSIX C has an empty language/country list
  and en-US-POSIX regional identity. Fresh Linux execution remains unverified.
- The owned application previously inherited an empty platform language hook.
  Supply native preferences through the existing public WebView hook, preserving
  upstream supported-language filtering, stored profile selection and early --lang
  policy. Expose the actual initialized engine's first language; do not fabricate
  Chromium translation packs or claim exact Electron locale-string aliases.
- Native no-GUI tests directly check malformed selectors, readiness, fresh arrays
  and independent selected-language callbacks. Actual public Firefly host tests
  compare a separate Foundation reference with isolated process-local AppleLanguages/
  AppleLocale settings: fr-CA preference, en-FI region/FI country, de-DE and
  sr-Latn-RS overrides, restoration of stored ja-JP and a fresh profile selecting
  fr-CA. ESM/default/Electron alias/CommonJS identity, ready listener timing and
  zero windows pass. The fixture writes no OS user defaults.
- Add the required gate to the macOS aggregate and Linux Xvfb job, including Linux
  fr_CA/C environment cases. Extend the existing single batched hidden command-line
  test to tie app.getLocale to actual navigator.languages. No broad GUI suite ran.
- Add ADR 0135 and detailed locale reference; update API indexes, compatibility,
  current status, roadmap and Code-OSS priority inventory. Unsupported dynamic
  notifications, UI translations, exact Chromium mapping and Windows stay explicit.

Validation: local macOS Release build and check-firefly-app-locale pass, including
all four public profile/override cases and direct native guards. The targeted
check-firefly-command-line regression passes actual LibWeb language, compositor CPU
argv and retained sandbox. Generated Firefly freshness, upstream pin, full source
style and whitespace checks pass. Built documentation/link validation passes.
The prepared real Code-OSS entry is re-run and still fails earlier at unsupported
allowServiceWorkers (exit 1), recorded in build/code-oss-locale-entry.log; its NLS
calls have not yet executed there. Full workbench/worker and fresh Linux/native
Windows support are not claimed. The full goal remains active.

Next continue the measured Code-OSS protocol/worker/cache/remote-scheme contracts
and required main/preload/binary-event-port transport. Investigate a clearly scoped
opt-in application adaptation only with explicit missing-feature failures, never
an inert protocol flag or a successful-workbench claim from an import probe.

## 2026-11-08: Add real renderer-to-main send events for the Code-OSS preload

- Previous goal turn made progress: c94e5de implements/tested native locale APIs
  and is pushed. Keep the full goal active and prioritize actual Code-OSS startup,
  preload/events/ports and native file transport instead of unrelated example work.
- Inspect actual pinned sandbox preload: it explicitly forwards send/invoke/on/
  once/removeListener. Public binary routing still needs document-bound native
  endpoints; take the missing public send prerequisite first without pretending
  that the existing internal byte channel enables public binary/port APIs.
- Add actual preload ipcRenderer.send -> main ipcMain event listeners. Share
  trap-free capture, bounds, monotonic IDs and native canonical document/realm/
  window ownership with invoke. Carry event kind through the reserved native route,
  never a caller-provided sender/permission field. Return undefined; local invalid
  data/inactive callers/quota exhaustion throw synchronously.
- Reuse native history ordering and reserve quota while main VM dispatch is busy.
  Events and invokes share 16/64/64 limits. Keep private ACK-only endpoint records
  without reply Promises; release before main listeners, revalidate before native
  ACK and reclaim on navigation/closure. Later admission/deadline refusal is an
  explicit helper diagnostic; there is no application delivery receipt/reply.
- Main events use the real EventEmitter order/once/removal/receiver semantics and
  shared senderFrame snapshot. Ignore listener results, preserve application
  exceptions, and add Electron's default no-op error-channel listener. Validate
  graphs natively; refuse malformed argument envelopes before EventEmitter rather
  than turning hostile renderer data into a main listener exception. Block raw
  send publication through contextBridge; publish fixed actions instead.
- Add no-GUI Node/Mite factory checks for order, once/reentrancy, receiver/removal,
  sender/history/revocation, malformed envelopes and listener exception behavior.
  Batch public acceptance in one hidden window with reloads: early sends, invokes,
  cycles/UTF-16/mutability, trap-free synchronous rejection, raw publication refusal,
  native history/sender, 16-event limit/credit recovery and listener-driven teardown.
- Run a separate real Electron positive event fixture. Use a ready continuation
  rather than awaiting app.whenReady at top-level ESM, and trap fixture errors to
  exit instead of allowing modal error dialogs. Its positive early/order/clone/
  sender/reload/close contract passes; Firefly quotas and per-document snapshots
  remain separate differences. No broad desktop suite or physical UI automation.
- Require the new public/native-policy gate in the macOS aggregate and Linux Xvfb
  job; include the positive Electron reference in the macOS aggregate. Add ADR 0136
  and update detailed IPC/API, compatibility, status, roadmap and file-transfer docs.

Validation: local macOS Release build and check-firefly-ipc-events pass. Main
policy also passes directly in Node and actual Mite, including the final malformed-
argument guard; positive actual Electron reference passes. Final source style,
generated JS freshness, upstream pin, built-site links and whitespace checks pass.
No main-to-renderer, binary, MessagePort, subframe or sync IPC support is claimed.
Fresh hosted Linux execution and native Windows desktop remain open. Full Code-OSS
still stops earlier at unsupported worker/cache/remote-scheme contracts; this is a
real public prerequisite, not a workbench launch. Full original goal remains active.

Next implement main-to-renderer events/listeners and public binary/port delivery
with exact sender/document revocation. Continue the real Code-OSS entry as protocol
worker/cache/remote-scheme contracts are resolved; keep Ladybird unmodified and the
explicit missing-feature boundary intact.

## 2026-11-08: Deliver main-to-preload events with native binding and retired timeout credit

- Previous goal turn made progress: e469c9e adds/tested real renderer-to-main send
  events and is pushed. Continue the Code-OSS preload/IPC priority with the other
  direction, retaining the full original specification and unmodified upstream.
- Add webContents.send and isolated ipcRenderer.on/once/removeListener/
  removeAllListeners, with addListener/off aliases. Native exports announce their
  owned document/realm before any application message; main can send to a loaded
  preload without requiring an initial application invoke/send. Bind/ACK control
  kinds remain reserved and cannot enter application dispatch or bind via an ACK.
- Capture/validate bounded plain-data arguments; record the native document/realm
  at send time and refuse stale/duplicate receiver IDs. Outgoing events have
  separate 16-document/64-host ACK credit and monotonic IDs. The receiver bounds
  deferred messages to 16 and enters a fresh isolated frame instead of recursing
  into an active one. Navigation/closure reclaim all pending state.
- Keep at most 64 source-realm listener registrations per document. Return the
  native module for chaining, preserve alias identity/receiver, snapshot listeners
  for removal/addition ordering, remove once before calling, share one mutable
  destination copy and stop after world/document revocation. event.sender is the
  private module and ports is empty; no port transfer or complete DOM Event parity.
  Block raw listener publication and clear exported module/listener roots at detach.
- Native ACKs release transport credit, not application results. Listener failures
  log explicit helper diagnostics and release credit; return values/Promises are
  not serialized/awaited. Full EventEmitter/DOM exception behavior remains partial.
- Retire a document's outgoing endpoint after a missing 30-second ACK. Merely
  freeing/replenishing credit to a stalled peer would grow the native transport
  queue. Further sends throw ERR_FIREFLY_IPC_REVOKED until a fresh document; late
  ACKs cannot revive it. Existing queued effects are not rolled back.
- Batch actual production-helper acceptance in one hidden window: first main send,
  cycles/aliases/UTF-16/mutability, listener receiver/event/alias/order/once/removal/
  additions, validation, listener-root and outgoing-queue limits, ACK recovery,
  native document replacement and listener-driven closure. OS-check the actual
  owned renderer PPID before SIGSTOP; a real timeout retires it, SIGCONT/reload
  admits a new working document. Always reap the owned process group.
- Run a separate real Electron common positive fixture. Keep strict Firefly type
  errors/limits separate: actual Electron differs in send clone-error handling and
  inherited listener coercion. Keep the reference alive through final window
  destruction assertions. Positive listener/clone/order/reload/close parity passes.
- Require check-firefly-ipc-outbound in macOS aggregate and Linux Xvfb, with the
  Electron positive reference in the macOS aggregate. Provide an explicit quick
  functional-only developer rerun; required CI retains the real stalled-peer test.
  Update detailed IPC/webContents/API, matrices/status/roadmap/file guide and ADR 0137.

Validation: local macOS Release build, full outgoing public gate (including actual
30-second stopped-peer retirement/reload), real Electron positive reference and
incoming-event regression pass. Final typed-listener selector/alias build and
quick functional rerun pass; Node/Mite geometry-policy regression remains green.
Source style, generated JS freshness, upstream pin, built-site link and whitespace
checks pass. Only targeted hidden IPC batches ran, without broad desktop suites or
physical UI automation. Fresh Linux and native Windows desktop remain unverified.
The actual prepared Code-OSS entry is re-run and still stops at allowServiceWorkers
(exit 1), logged in build/code-oss-outbound-entry.log. No workbench or general
page-callback/MessagePort/binary support is claimed. Full goal remains active.

Next complete demanded contextBridge callback forwarding and public binary/port
transport with the same native sender/document/revocation rules. Continue the real
Code-OSS entry as protocol worker/cache/remote-scheme contracts are resolved; do
not replace those features with inert flags or a narrowed completion claim.

## 2026-11-08 — Bounded contextBridge document callbacks (VS Code prerequisite)

- Share a weak per-document callback registry across exported APIs. Ordinary page
  function arguments now become frozen, revocable isolated proxies, with graph
  aliases and stable live identity across subscription/removal methods. Roll back
  new proxies on failed capture; commit before executing application code.
- Copy/freeze data both ways and bridge actual callback Promise results with direct
  engine observation. Remove privileged frames and install principal HTML callback
  settings; page Promise jobs remain independent of isolated-world cancellation.
- Bound live callback endpoints and aggregate pending callback Promises to 64 each,
  arguments to 64 and nested callback/API entries to 32. Permit controlled API
  reentry through fresh source frames, keeping general recursive evaluation refused.
- Keep pending endpoints revocable after JS proxy GC. Do not execute JS in GC-driven
  native destructors. Teardown releases callback roots and rejects pending bridges.
- Add actual-engine tests for identity/aliases, frozen realm-owned data, clean stacks,
  hostile getters/errors/Promise hooks, rollback, weak GC, repeated endpoint budgets,
  exact pending quota/recovery, nested calls, outer-turn microtask ordering and
  detach with surviving page jobs.
- Extend the existing hidden public IPC batch with document subscriptions, on/off
  identity, API reentry and Promise results after reload. Keep the positive Electron
  comparison scoped: Electron differs in cross-method proxy identity and callback
  input mutability. Update detailed bridge/IPC/API status and ADR 0138.

Validation: local macOS Release firefly build and complete native isolation gate
pass. Actual hidden production-helper callback/IPC functional batch and actual
Electron common-contract reference pass. Existing stalled-peer transport logic is
unchanged; its full timeout stress gate was accepted in the previous slice and is
not rerun for this bridge-only change. No broad UI suite or physical desktop
interaction. Final source style, generated-adapter freshness, unmodified upstream
pin, built-site links and whitespace checks pass. Fresh Linux and native Windows
callback execution remain unverified.

Remaining: raw IPC event/module forwarding, higher-order returned functions, binary
values and MessagePorts remain refused; the unmodified Code-OSS preload/workbench
is not accepted. Its real entry still has the previously observed allowServiceWorkers
blocker; this callback slice does not change protocol registration. Next prioritize
safe full preload graph compatibility and binary/port transport while resolving
actual worker/cache/resource requirements. HTTP/2/3 and website polish stay lower
priority. Ladybird remains unmodified; full goal stays active.

## 2026-11-08 — Native IPC module redaction for document event callbacks

- Continue from the pushed callback slice (2be9b30), which made authoritative
  implementation/test progress. Preserve the full active goal and VS Code priority.
- Register the exact native ipcRenderer exports object weakly with its isolated
  owner. Replace it with an empty ordinary data node only during contextBridge
  source-to-document capture; keep graph aliases and destination-owned freezing.
  Never inspect module methods, expandos, getters or private listener bookkeeping.
- Centralize that policy for direct/nested publication, synchronous/Promise
  results and callback arguments. Default raw IPC capture stays strict and
  function-free. Individual native methods, copies containing them, proxy wrappers
  and foreign-realm values still reject. Clear native metadata on detach.
- Add actual LibJS/LibWeb checks for all those routes, exact object shape/prototype,
  no getter execution, unchanged default capture and weak metadata collection.
  Separate the weak-GC test's allocation frame from conservative native stack roots.
- Extend the existing single hidden IPC batch with a direct document callback
  registered through native once, native event.sender redaction, empty ports and
  sync/Promise/private-module publication on reload. This admits the supported
  event graph used by Code-OSS wrappers, without publishing the full preload.
- Compare the actual Electron 45.2.3 reference: it removes IPC methods but copies own
  EventEmitter bookkeeping and uses mutable function/callback results. Firefly uses
  the documented empty module object and frozen data policy; reference tests assert
  only the common absence of IPC methods, with strict Firefly assertions separate.
  Update detailed APIs, compatibility/status/roadmap and ADR 0139.

Validation: local macOS Release build and complete native isolation gate pass.
Actual production-helper functional IPC batch and actual Electron common reference
pass. Final source style, generated adapters, unmodified upstream pin, built-site
links and whitespace checks pass. Only focused hidden IPC batches ran; no broad GUI
suite or physical desktop interaction. Stalled-peer transport logic is unchanged;
its prior full timeout/reload acceptance is not rerun. Fresh Linux/native Windows
callback execution remain unverified; Ladybird remains unmodified.

Remaining: the pinned Code-OSS preload still needs source-accessor publication,
process/options, webFrame/webUtils and binary/port handoff contracts. Its main entry
has the previously observed allowServiceWorkers blocker; no new actual workbench
launch or startup-protocol change is claimed. Next prioritize the demanded binary/
MessagePort transport and full preload graph alongside real worker/cache/resource
requirements. Do not replace those contracts with inert flags. Full goal stays active.

## 2026-11-08 — Native contextBridge binary copies for the Code-OSS boundary

- Resume from pushed 6480d35, which made authoritative implementation/test progress.
  Keep the full goal and actual VS Code/efficient file transport priorities intact.
- Enable a 64 KiB aggregate complete-backing-store policy for contextBridge API
  publication, function arguments/results, Promise results and callback directions.
  Reuse the validated native buffer/view snapshot; retain cycles, aliases, offsets,
  destination view types and one destination allocation per distinct backing store.
- Add explicit FrozenContainers materialization: plain graph containers freeze;
  copied binary stores/views stay writable and independent. Strict Frozen and
  default byte-free snapshots retain their refusals. Binary expandos/prototypes
  are omitted; no getter/constructor/iterator hooks execute or source buffer detaches.
- Extend Promise prototype/then checks for native binary instances/prototypes,
  without running polluted destination getters or accepting unknown host/proxy
  prototypes. Existing callback identity/pending/document/revocation rules remain.
- Add actual-engine binary contextBridge tests: publication/sync/Promise/callback
  directions, 65536-byte boundary and overflow refusal, writable independent bytes,
  aliases/offsets, trap-free capture, then pollution and surviving copies at detach.
- Extend one hidden production-helper IPC fixture with a plain-data trigger for
  renderer-local binary work and plain-data reports. It does not send bytes to main.
  The real Electron reference passes the common behavior; its separate backing-store
  copies differ from Firefly's preserved backing aliases, tested separately.
- Keep actual process IPC binary refusal. Its UTF-8 string seam is unsuitable for
  raw FFB2 frames; reject a Base64 workaround in favor of integrating the existing
  native byte channel with real current-document/preload/main authorization next.
  Update APIs, file-transport plan, compatibility/status/roadmap and ADR 0140.

Validation: local macOS Release build/native isolation gate and actual hidden
production-helper and Electron common-contract batches pass. Final style, generated
adapter freshness, unmodified upstream pin, built-site links and whitespace checks
pass. No broad GUI suite or physical desktop interaction. Native Linux/Windows
execution remain unverified; Ladybird stays unmodified. No new actual Code-OSS
entry/workbench launch is claimed; the known allowServiceWorkers blocker remains.

Next integrate raw native document-authorized binary process transport, then actual
port handoff/streaming, while continuing demanded preload process/options/accessors
and worker/cache/resource contracts. Do not substitute a text-encoded file transport,
inert feature flags or a smaller success definition. Full goal stays active.

## 2026-11-08 — Real document/preload raw-channel handoff and readiness

- Continue from pushed d6343f1, which made authoritative contextBridge binary-copy
  progress. Keep actual native binary IPC/Code-OSS and the full product goal active.
- Append a separate trusted NotRouted application-IPC handle handoff to our generated
  endpoint, preserving all 251 upstream and three previous owned message IDs.
  Keep file-resource authority separate. Bootstrap forwarding stays in the shared
  native IPC library; consumers use the canonical staged declaration only.
- Create one real native channel pair after the authenticated canonical preload
  Bind. Bind page/navigable/document/realm/epoch to the actual selected connection
  and active configured world. Unknown/foreign/duplicate handoffs refuse admission;
  there is no JavaScript handle/epoch/binding API. Retire failure until a fresh doc.
- Exchange a raw 52-byte FIH1 hello containing epoch, actual helper PID and document/
  navigable identities. Main compares the exact canonical binding and its real owned
  helper PID before readiness. Unexpected packets/repeated hello close the channel;
  no raw echo or application packet receiver is installed at this stage.
- Close the channel on reset/navigation/crash/window/world teardown and unregister
  the receiver at helper shutdown. Guard deferred close callbacks by epoch so an
  old channel cannot clear the replacement. Add a 30-second missing-hello retirement
  path; actual stalled-hello runtime acceptance remains pending.
- Add a native non-GUI handshake probe with an independent little-endian byte vector,
  NUL/high-bit fields and size/version/zero/truncation refusals. Keep the existing
  raw channel/frame/credit/adversarial/fresh-child gate green.
- Add a focused production-helper gate: two hidden windows load the same file,
  the first reloads, and three distinct document bindings complete raw handshakes.
  Check every observed helper PID against its live OS parent. Run existing public
  IPC/contextBridge assertions and closure in that batch; always reap the group.
  Wire the new gate into the macOS aggregate and Linux Xvfb workflow.
- Update IPC/file-transfer/status/VS Code/roadmap/upstream-update docs and ADR 0141.
  Public invoke/send/webContents.send still reject binary; no Base64/string-byte
  transport, application packet dispatch or MessagePort claim is introduced.

Validation: local macOS Release build, native handshake and complete existing byte
channel/fresh-child suite pass. The actual production-helper reload/multiwindow
handshake gate passes with real OS-parent checks. Final source style, generated
adapter freshness, unmodified upstream pin, built-site links and whitespace checks
pass. No broad UI suite or physical desktop interaction. Fresh hosted Linux and
native Windows execution remain unverified; Ladybird remains unmodified.

Next implement raw application packet kinds/FFB2 payloads, bounded early preload
queueing, busy main-VM/history dispatch, canonical sender metadata and binary
replies/ACK/timeouts/revocation on this actual bound channel. Add negative forged/
duplicate handoff and real stalled-hello acceptance before admitting public binary
calls. Then complete MessagePorts/streaming and demanded preload process/options/
accessors/worker-cache resource contracts. The actual Code-OSS main entry's known
allowServiceWorkers blocker is unchanged; no new workbench launch is claimed.
Full goal stays active.

## 2026-11-08 — Public raw binary IPC with native request/history correlation

- Resume the paused uncommitted application-transport implementation; the previous
  accepted handoff is pushed as 1eb2581. Keep the full goal and VS Code priority.
- Send all ordinary/binary application payloads over the actual bound native channel,
  using FIM1 kind/epoch/request frames and raw FFB2 graph attachments. No byte-to-UTF-8,
  Base64 or JSON number-array transport. Public invoke/reply and both send directions
  now use the bounded 64 KiB distinct-complete-backing copy policy; Buffer normalizes
  to Uint8Array, with cycles/view geometry/backing aliases and independent storage.
- Retain a native empty-wire control marker on the original history/control connection.
  Join marker, raw payload and completed canonical history step before main dispatch.
  This handles either arrival order and keeps app-supplied URLs out of sender identity.
  Capture native metadata in a destination-owned object and private byte view; native
  wire extraction reads slots without getters and rejects foreign/invalid storage.
- Reserve 16/document and 64/host admission credit on the first half; keep it through
  history and VM-busy retries. Bound refused counterpart IDs and expire missing halves
  at 30 seconds. Bound native early preload/unsent queues to 48 frames with underlying
  byte transport credit 16. Drop expired unsent requests/events at flush. No source
  JS pointers remain in native packet buffers. Existing source pending limits remain.
- Preserve application ACK quotas, deadlines and document/epoch revocation. Deferrals
  retain at most one settlement per pending helper request; transport retirement rejects
  pending calls and cannot publish a delayed success. Close/clear old queues and native
  budgets; stale close callbacks cannot clear replacement endpoints. Disable old text
  application payload/reply dispatch as an alternate transport path.
- Add a modern ESM binary fixture with an invoke during preload before handle arrival,
  asynchronous binary replies, main Buffer, 64 KiB views/cycles/aliases/offsets, sends
  both ways, caller mutation and reload. The real Electron common reference passes;
  backing alias guarantees across contextBridge remain separate Firefly assertions.
- Extend native frame probes with an independent application-header vector, flags,
  kind/identity/truncation and exact/overflow sizes. Native isolation now checks raw
  private wire byte preservation, foreign realm refusal and trap-free slot extraction.
- Add explicit parent-only diagnostic handoff observers: unknown realm, wrong document
  and duplicate correct handle must each close without hello/app data. Actual helper
  reload/multiwindow test passes all three cases for each document and checks live OS
  parent PIDs. Default apps allocate no diagnostic observers or page binding capability.
- Fix a stale regression expectation from ADR 0139: whole ipcRenderer publication
  must become inert empty data, while individual raw methods still reject. Check that
  in the principal page and source separately. Change the previous Uint8Array refusal
  case to real over-budget storage now that bounded binary input is supported.
- Wire the binary gate into native macOS aggregate and Linux Xvfb. Update detailed API,
  file transport, compatibility/status/roadmap, historical ADR links and ADR 0142.

Validation: local macOS Release build, native raw frame/channel/real-child gates and
complete actual-engine isolation gate pass. Public binary Firefly and actual Electron
common-contract fixtures pass. Full existing public request/reply regression passes,
including source traps/quota/recovery, navigation, stale replies, closure, real helper
crash/replacement and actual 30-second request deadline. Send event/history gate passes.
Full outgoing gate passes its actual OS-verified SIGSTOP/SIGCONT timeout retirement and
fresh-document recovery. Runtime admission rejects all nine unknown-realm/wrong-doc/
duplicate handles across three document epochs/two hidden windows. Final source style,
generated JS, unmodified upstream pin, built-site link and whitespace checks pass.
Only focused hidden batches ran; no broad desktop suite or physical UI automation.

Remaining: actual stalled-hello fault injection and broader compromised-process testing,
MessagePort handoff/streaming, larger-file transport and performance evidence. Fresh
hosted Linux and native Windows execution remain unverified. Ladybird is unmodified;
Mite remains independent. No new Code-OSS workbench launch is claimed: its actual entry
still has the known allowServiceWorkers blocker and needs complete preload accessors/
process/options/zoom/file and worker/cache/resource contracts. Next advance those demanded
contracts and ports, keeping the full product goal active rather than declaring the
first copied-buffer transport complete as the overall objective.

## 2026-11-08 — Trusted source getter snapshots for Code-OSS API publication

- Continue from pushed d0fe3c2, which made authoritative native binary IPC progress.
  Inspect the actual pinned preload: its process metadata graph contains own getters
  that previously caused exposeInMainWorld to reject. Prioritize that demanded
  publication contract without claiming that missing process/worker APIs exist.
- Add an explicit native accessor reader to snapshot capture, default disabled.
  Supply it only for trusted API publication. Require exact source-realm ordinary
  ECMAScript getters, preserve the source receiver, copy supported values/functions/
  binary/cycles and expose frozen data properties rather than live page getters.
  Setter-only fields copy undefined; setters never run. Native/foreign/Proxy/bound
  getters refuse without hooks. Redact marked IPC modules before enumerating them.
- Limit getter execution to 64 per publication alongside existing graph/function/
  byte budgets. Root all visited source values so getter-triggered GC cannot invalidate
  transient alias keys. Retain source/document/realm/registry during reentrant native
  work. Refuse recursive publication, recheck liveness and a newly occupied principal
  namespace, and revoke temporary function exports on failure. Getter effects in the
  source are real and not rolled back. Other capture directions stay accessor-free.
- Add actual-engine cases for one-time snapshots, source receiver, metadata values,
  frozen descriptors, functions/aliases/cycles/binary, GC, throwing/unsupported values,
  exact getter quota, foreign/native/Proxy/bound getters, strict page/result boundaries,
  atomic function-budget rollback, namespace collision, recursive publish and revoke.
  Extend module-redaction coverage to a source getter returning the private module.
- Keep older no-trap tests meaningful by using genuinely refused Proxy getters;
  ordinary source getters now have their own side-effect/error expectations.
- Add one hidden production-helper getter batch with reload and an actual Electron
  common reference. Check Code-OSS-shaped metadata, immutable snapshots after source
  updates, descriptor/receiver semantics, returned functions, rollback and isolation.
  Wire the helper gate into the macOS aggregate/Linux Xvfb path. Update detailed API,
  compatibility/status/roadmap/upstream update guide and ADR 0143.

Validation: local macOS Release build, actual native isolation and production-helper
getter gate pass. Real Electron snapshot/descriptor/receiver/freeze/rollback reference
passes, with Firefly-only strict direction checks separate. Final native rebuild,
source style, generated JS freshness, unmodified upstream pin, built-site links and
whitespace checks pass.
Only focused hidden getter batches ran; no broad desktop suite or physical UI
interaction. Fresh Linux/native Windows execution remain unverified; Ladybird stays
unmodified. Full goal stays active.

Remaining: no actual renderer process object is installed by this feature. Continue
native sandbox process metadata and additionalArguments, real zoom/file APIs and
MessagePorts, while resolving the known actual-entry allowServiceWorkers/worker/
cache/resource requirements. No new actual Code-OSS preload/workbench launch is
claimed; a real workbench open/edit/save milestone remains unaccepted.

## 2026-11-08 — Native sandbox process snapshots and window arguments

- Resume from pushed 2f28123. The preceding status-only turn made no implementation
  progress; revalidate the failed in-flight process build and fix its actual native
  compile errors rather than restarting from a claimed success. Keep the full goal
  and real Code-OSS launch priority active.
- Add bounded webPreferences.additionalArguments through the real native constructor
  and per-window preload metadata. Read exact native own array entries, enforce
  strict Unicode/NUL/argument count/bytes, require a preload for nonempty arguments,
  and snapshot the main OS environment at construction. Preserve caller mutation
  isolation. Keep app arguments out of the upstream helper OS command line.
- Add a pure AK versioned process configuration decoder shared by main/helper, with
  independent helper schema/type/count/byte validation. Bound process content to
  64 KiB/window and 1 MiB across 16 windows, separate from existing source limits.
  Supply pinned engine hashes from the manifest and track configure-time changes.
- Capture the actual helper executable before upstream sandbox bootstrap. Install
  only in each product top-level isolated preload: actual PID/platform/architecture,
  helper execPath, renderer flags, truthful framework/engine versions and fresh
  mutable document-local argv/env containers. Reload restores the original window
  snapshot. No Node/V8/Electron spoofing or Mite/LibWeb dependency inversion.
- Explicitly reject demanded but unimplemented process.on/getProcessMemoryInfo;
  block their raw native functions from bridge publication. Keep the principal,
  child-frame, initial blank and diagnostic worlds free of this process object.
- Add native schema roundtrip, exact/overflow argument/environment quotas, type,
  version, NUL, Unicode and __proto__ cases. Add a hidden two-window/reload public
  gate using the real Code-OSS argv/config IPC/userEnv merge shape. Check live OS
  parent/helper PID independently, actual executable/identity, local mutation,
  getter publication, unsupported methods and page/frame isolation.
- The first public gate found AK's default UTF-16 conversion allowed lonely
  surrogates; use explicit AllowLonelySurrogates::No and verify rejection. The
  measured Electron macOS reference does not equate execPath/argv[0], reorders
  switches/positionals and drops empty arguments. Keep its shared nonempty values/
  multiplicity assertions separate from Firefly's exact logical argv contract.
- Update the existing preload/sandbox regressions to require the restricted source
  process while retaining absence in the principal. Wire the new gate into the
  macOS aggregate and hosted Linux Xvfb workflow. Add detailed sandbox process API,
  current status/matrix/VS Code/roadmap/upgrade notes and ADR 0144. Correct stale
  VS Code binary transport statements superseded by the raw IPC slice.

Validation: local macOS Release build and complete actual-engine isolation gate
pass, including native metadata limits. Actual production-helper process gate and
Electron 45.2.3 common reference pass. Existing full public preload/reference gate
passes first-script/nested frozen APIs/Promises, three windows, child-frame refusal,
reload and file-to-HTTP process replacement; source validation/exact 64 KiB boundary/
restricted loader/accessor and syntax/throw failure controls pass. Global sandbox
regression passes actual owned-helper kernel enforcement and isolation, including
late-call refusal. Only focused hidden batches ran; no physical UI automation or
broad desktop suite. Source style, generated adapters, upstream and built-site checks
are recorded with this commit's final validation.

Re-run the actual prepared Code-OSS entry from build/code-oss-engine-final with an
existing temporary user-data directory: exit 1 remains the real allowServiceWorkers
unsupported privilege (build/code-oss-process-entry.log). An older diagnostic output
had stale engine-option handling, and running without an existing user-data directory
stopped at setPath validation; neither was treated as workbench acceptance.
Remaining: demanded process events/metrics, zoom/file APIs and MessagePorts, then
actual worker/cache/resource contracts and a real workbench open/edit/save gate.
Fresh Linux/native Windows execution of this slice remains unverified. Ladybird
stays unmodified. The full goal stays active; this prerequisite is not VS Code launch.

## 2026-11-08 — Native webFrame browser zoom for the Code-OSS preload

- Resume from pushed 40c4d67, which completed native restricted process metadata and
  additionalArguments. The preceding goal turn made tested/pushed implementation
  progress; keep the full goal and real Code-OSS startup priority active.
- Inspect the pinned main privilege registration and preload again. A first local
  workbench adaptation cannot merely delete allowServiceWorkers/codeCache/remote
  privileges: it needs explicit unavailable-feature paths. That mode remains planned.
  Continue the independently demanded webFrame.setZoomLevel contract instead of
  claiming an inert protocol flag or worker implementation.
- Add a document-bound C++ webFrame preload capability with factor/level getters and
  setters. Read native state and convert with powers/logarithms of 1.2. Route changes
  through the existing upstream page-client/parent-view browser zoom handshake,
  preserving viewport/input/compositor agreement without downstream Ladybird changes.
- Require the live executing source realm and fully active original document. Capture
  weak references; retain owner/document during calls and revalidate after the native
  handshake's nested event loop. No parallel JS zoom preference or author CSS transform.
  The upstream handshake's unresponsive-parent timeout remains unverified.
- Reject nonnumeric primitives/objects without coercion and nonfinite/out-of-range
  values explicitly. Use the native 0.3..5 factor range with documented partial
  compatibility, and view/window scope rather than Chromium same-origin propagation.
  Freeze the capability module, block raw native method publication and redact the
  whole marked module; expose application-authored narrow wrappers instead.
- Add non-GUI actual-engine validation for NaN/infinity/object/undefined/type inputs,
  coercion-hook refusal, getters, freezing and native export denial. Add one focused
  hidden production-helper fixture for early preload level 2, asynchronous config-IPC
  source job setting, synchronous factor/level return values, actual viewport and DPR
  changes, exact native factor boundaries, invalid values, isolation, redaction,
  reload and a second same-origin window with sticky resize/zoom-change detection.
- Initial hidden tests that waited on animation frames/timers did not report before
  their deadline; synchronous metrics after the real parent acknowledgement now
  prove the API's intended semantics without requiring visible render frames. The
  actual Electron reference uses only supported numeric/layout/reload assertions;
  native factor limits, validation and module/window policies remain Firefly-specific.
- Wire the focused gate into the macOS aggregate/Linux Xvfb workflow. Update detailed
  webFrame/preload API, compatibility/status/VS Code/roadmap/upstream notes and ADR 0145.

Validation: native macOS Release build and complete real-engine isolation suite pass,
including the new webFrame direct native cases. Production-helper early/config-IPC/
viewport/DPR/factor/level/limits/isolation/reload/multiwindow gate passes. Actual
Electron 45.2.3 numeric/layout/reload reference passes. Existing full public preload
and Electron-reference regression passes source validation/exact 64 KiB boundary,
restricted loader, first script/nested API/Promises, three windows, child-frame
absence, reload and file-to-HTTP helper replacement. Final style/generated-adapter/
unmodified-upstream/site-link/whitespace checks accompany this commit. Tests used
focused hidden batches; no physical UI automation or broad desktop suite.

Remaining: webUtils/file mapping, process events/metrics, MessagePorts, actual
worker/cache/remote-resource contracts and the first real workbench open/edit/save
milestone. The last real-entry probe at 40c4d67 still stops at allowServiceWorkers;
this independent preload API does not change that registration or claim a new
workbench launch. Fresh Linux/native Windows execution remains unverified. Ladybird
is unmodified, Mite remains independent, and the full product goal stays active.

## 2026-11-08 — Real bounded native HTML file selection

- Resume from pushed 3b8c87b, which accepted native webFrame zoom and was implementation
  progress. Keep the original full goal and Code-OSS priority active.
- Inspect the pinned Code-OSS webUtils.getPathForFile wrapper and real upstream file
  plumbing. Find an earlier missing vertical prerequisite: Firefly does not attach
  on_request_file_picker (the upstream browser Tab frontend does). SelectedFile carries
  basename/data or descriptor and decode eagerly reads it; Rust-backed File exposes
  no source-path field. Default contextBridge correctly refuses File/DOM objects.
  Do not implement a fake filename/path method or broaden arbitrary DOM capture.
- Add one owned nonmodal Qt file-input chooser per native Firefly window. Map advisory
  accept extensions/MIME/audio-image-video groups with bounded filter work and an All
  files choice. Use ExistingFile/ExistingFiles for single/multiple selection. Production
  keeps Qt's platform-native dialog selection; only C++ probes supply hidden widgets.
- Validate absolute/NUL-free/bounded native paths and open nonblocking, then fstat the
  actual descriptor. Refuse directories/FIFOs/devices and read at most 4 MiB+1 to catch
  growing inputs. Snapshot at most 16 files / 4 MiB each / 16 MiB aggregate, with empty
  files allowed and all-or-nothing refusal. Send basename and bounded byte-backed
  SelectedFile through the unchanged engine IPC/FileList path, never absolute names.
  This avoids upstream descriptor decode's unbounded later read, not whole-file copies.
- Cancel before explicit main load, engine load-start, helper crash or window close.
  Invalidate active dialog identity before rejection, ignore stale finished signals,
  cancel duplicate requests without data and drop completion before destruction so
  a dead view cannot be called. Keep QObject ownership out of the Mite VM.
- Add hidden real Qt widget dialog tests for accept/cancel/multiple mode/stale finish/
  duplicate/destruction plus actual temporary binary/Unicode/empty/FIFO/directory/
  missing/relative/NUL files and exact/overflow count/file/aggregate budgets.
- Add a hidden actual Qt-view/owned upstream SDK helper gate: trusted native mouse
  events activate the input, a real hidden chooser accepts a known file, unchanged
  SelectedFile IPC creates browser File data, and the page verifies Unicode basename,
  no parent path disclosure, File.text() and all File.arrayBuffer() bytes of a real
  4 MiB exact-limit file with embedded NUL. Cancellation works; navigation dismisses
  a third pending chooser and the new document has an empty FileList. The initial
  probe URL-count guard was corrected to use raw_urls because SDK temporary profiles
  can prepend a welcome URL. No inferred pass was recorded from that failed setup.
- Wire both gates into the macOS aggregate and Linux Xvfb workflow. Document native
  input limits/validation and missing provenance in the API/matrix/status/VS Code/
  roadmap/upstream guide and ADR 0146. Keep getPathForFile absent rather than return
  invented paths or claim negative-only API acceptance.

Validation: macOS Release runtime and native probe builds pass. Hidden Qt chooser/
filesystem policy gate and real native-view/SDK-helper 4 MiB text/binary/cancel/
navigation gate pass. The latter directly exercises the underlying native view,
not production-helper file selection, a complete JavaScript/Designer app or interactive
platform sheets; those acceptance scopes remain open. Existing full public preload
and actual Electron-reference regression passes three windows, source validation,
exact 64 KiB source limit, first-script/nested API/Promises, subframe absence, reload
and file-to-HTTP process replacement. Final style/generated adapter/upstream/site/
whitespace checks accompany this commit. No physical desktop input automation ran;
widget and view tests have DontShowOnScreen, and no broad GUI suite was launched.

Remaining: retain selected-file paths through a separate trusted native provenance
association bound to exact File/document identities; add explicit File-wrapper passage
only where required, keeping ordinary data/IPC DOM refusal. Then implement the real
webUtils.getPathForFile API, drop/clone provenance, production-helper/app acceptance,
large/streamed file transport and measured overhead. Process events/metrics, MessagePorts,
worker/cache/remote-resource contracts and real Code-OSS open/edit/save remain required.
This prerequisite does not change the last actual-entry allowServiceWorkers blocker.
Fresh hosted Linux/native Windows execution is unverified. Ladybird is unmodified,
Mite remains independent and the full product goal stays active.

## 2026-11-08 — Weak native input File provenance component

- Resume from pushed ecf161d, which added the real bounded native picker and was
  tested implementation progress. Preserve the original full goal and Code-OSS priority.
- Add the internal native File/document origin association required before a real
  webUtils.getPathForFile implementation. Keep it distinct from ordinary bridge data,
  JavaScript-visible names, file-body copies and filesystem access. Public API and
  trusted main-to-helper path delivery remain missing; this is a native component.
- Supply a versioned synchronous native Selection scope: bounded canonical nonzero
  u64:u64 document IDs, path/span count, absolute NUL-free UTF-8 paths, per-path/total
  budgets and matching native basenames. Refuse nested scopes, duplicate IDs, malformed
  identifiers and schema drift. Bind only the exact span element and native File name
  to the actual fully active document identity; copied value-equivalent spans cannot
  consume another selection's metadata. Never upgrade an unavailable native File later.
- Store exact Rust File-cell and Document identities in weak references. Source
  inactivation releases paths while keeping weak negative File tombstones, so surviving
  native Files cannot become apparently unobserved after reactivation. Collected Files
  release slots. Bound records to 1,024 and path content to 1 MiB/helper. Quota refusal
  marks the document unavailable; a bounded weak failure ledger and sticky ambiguity
  for missing identities prevent refusal/GC from turning lost records into synthetic
  Files. Failure-ledger overflow refuses all lookup. Cleanup is native-only.
- Add a fingerprinted owned composition of HTMLInputElement.cpp with a FileList root
  and one post-creation observer. Retain current File/document while recording. Preserve
  every other upstream byte and the Rust File ABI; missing/duplicate/version drift
  checks fail closed. LibWeb owns the single exported native registry; SDK tests link
  their actual direct LibIPC dependency for SelectedFile lifetime operations.
- Extend the actual-engine isolation gate with real HTMLInputElement::did_select_files
  FileList/File creation using controlled native test metadata. Verify Unicode path
  association, same-name/data noninheritance, wrong span and created name, missing/wrong
  document metadata, no upgrade, schema/path/identity/type/version/nesting refusal,
  inactivity/reactivation, weak GC, exact 1,024-entry and 1 MiB path limits and overflow.
  These native metadata inputs are not evidence of an OS chooser path handoff.
- Update native input/VS Code/current status/roadmap/upstream notes and ADR 0147 with
  this precise component scope and the remaining controller/bridge/public API work.

Validation: local macOS Release runtime/native-view probe builds and complete actual
LibWeb/LibJS isolation suite pass, including the new component's exact boundary and
negative cases. The one focused hidden Qt-view/owned upstream SDK helper regression
still passes real 4 MiB File.text/arrayBuffer bytes, basename privacy, cancellation
and navigation. No public path API, production chooser-to-helper metadata handoff,
physical desktop input automation or broad GUI suite is claimed. Final style,
generated-adapter, unmodified upstream, site/link and whitespace gates accompany
this commit. Fresh hosted Linux/native Windows execution remains unverified.

Remaining: capture original document/page selection identity when the native dialog
opens and deliver accepted paths together with the exact selected byte batch over
trusted native control IPC. Validate connection/page/document, cancellation, stale
navigation, subframes and replay. Then implement narrowly validated File-wrapper
argument passage and the actual webUtils.getPathForFile API; ordinary DOM/data/IPC
capture must keep refusing File values. Drops/clones, large-file/streaming transport,
performance evidence, process events/metrics, MessagePorts and actual worker/cache/
remote-resource contracts remain open. The real Code-OSS entry's allowServiceWorkers
blocker and workbench open/edit/save acceptance are unchanged. Ladybird is unmodified,
Mite remains independent and the full product goal stays active.

## 2026-11-08 — One-shot native chooser request ownership

- Resume from pushed b027246, which accepted the weak native File origin component
  and was concrete tested implementation progress. Preserve the full original goal
  and VS Code priority. Answer the user's status question explicitly: no workbench
  exists and the actual entry still has the worker privilege blocker.
- Inspect the next trusted path handoff. A basename/content match or list of active
  documents would not bind a delayed result to the original pending input. Add the
  required native one-shot ownership layer before wiring a parent/helper transport.
  This is still a private component; no public getPathForFile or native wire claim.
- Add weak Page/input/Document request records, monotonic serial/document tickets and
  original multiple mode. Sweep collected, inactive, adopted/rehosted targets; bound
  pending pages to 64. Replacing the native sender clears requests without serial
  reuse. Sender callbacks enqueue/capture only, without reentrant Page mutation.
- Match Page/serial/document before consuming a result. Reject stale/foreign responses
  without cancelling a newer request. Root a matching original target and consume
  before callbacks; validate exact document metadata, byte-backed data, original
  single/multiple mode and count/per-file/aggregate budgets. Invalid matching results
  cancel only the original input. A native guard admits only its original input and
  exact selected span before the real Page/FileList path runs in the origin scope.
- Compose the pinned Page.cpp with exact request/close/delivery guard insertions,
  preserving all other bytes and rejecting hash/missing/duplicate seam drift. Handle
  missing weak targets without dereference. Without a sender the normal SDK/browser
  input path remains unchanged and does not fabricate provenance. Share the three
  byte/count limits between the existing Qt chooser and native helper policy.
- Extend non-GUI real-engine tests with actual Page pending-target handling, positive
  native path lookup from controlled metadata, null input, replay/old-new request,
  foreign page/document, malformed fields, wrong path, single/count/per-file/aggregate
  overflow, cancellation, observed inactivity, sender replacement, input adoption and
  collected weak targets. The first foreign-realm fixture left an extra upstream test
  factory context current and failed settings-frame verification; explicitly restore
  that context and only accept the later complete passing suite.
- Update the API/status/VS Code/upstream documentation and ADR 0148 with the exact
  component acceptance and still-missing trusted transport/public API.

Validation: local macOS Release build and complete actual-engine isolation suite pass,
including all new native Page/ticket/payload cases. Focused hidden Qt-view/owned SDK
helper selection regression passes real 4 MiB File.text/arrayBuffer bytes, basename
privacy, cancel and navigation. No actual chooser paths cross a native message in
this slice: metadata is provided by controlled C++ tests, and no public webUtils API
is installed. Native 64-page concurrency and fresh Linux/Windows remain unverified.
The actual prepared Code-OSS entry is re-run against the fresh runtime with an
existing temporary user-data directory: exit 1 remains protocol privilege
allowServiceWorkers (build/code-oss-ticket-entry.log), so no startup advancement is
claimed from this component. Style/generated-adapter/unmodified-upstream/site-link/
whitespace checks pass. No physical desktop input automation or broad GUI suite ran.

Remaining: install the helper sender and owned native request/result control messages
with preserved IDs; bind actual parent connection/page/canonical document and dialog
lifecycle to the ticket; send actual accepted paths plus the exact bounded byte batch.
Then validate File wrapper passage and implement public webUtils.getPathForFile.
Keep generic DOM/data/IPC refusal, and audit real Code-OSS startup rather than infer
launch from private file tests. The last actual-entry allowServiceWorkers blocker,
workbench open/edit/save, process events/metrics, MessagePorts, worker/cache/resources,
large-file/streaming performance and platform-complete original goal remain open.
Ladybird is unmodified, Mite remains independent and the full goal stays active.


## 2026-11-08 — Explicit local Code-OSS desktop experiment

- Revalidate the prior status-only turn as no implementation progress. Resume from
  pushed b562101 and inspect the uncommitted local-desktop draft, pinned sources,
  current progress/spec and actual-entry logs. Keep the original full framework
  goal and real Code-OSS workbench priority active.
- Complete --local-desktop in the checked source overlay. Require the existing
  cache/engine build adaptations and a separate truthful-LibJS runtime flag
  ELYTRA_CODE_OSS_LOCAL_DESKTOP=1. Capture mode once, publish frozen limitations and
  retain original inactive/Node/Electron behavior. Invalid runtime values reject.
- Pin seven additional original application source files and preserve their license
  headers. Keep only supported vscode-file privileges in this explicit mode; disable
  the actual engine cache path. Reject browser/Electron webview factories, all main
  webview methods, remote CLI/resolver/provider/request entry points. Omit disabled
  native webview and managed remote protocol initialization rather than installing
  fake working services. Remote resolver refusals remain Promise rejections.
- Add exact-source/policy tests that reconstruct every original byte outside the
  declared additions, reject drift/duplicate/missing seams before output, and run
  the actual leaf policy/pinned scheme literal under Node and Mite. Test runtime
  absent/0/1/invalid, frozen capture/status, original identity, scheme drift, cache
  callback suppression, remote CLI and explicit feature/Promise failures. Register
  the native non-GUI CTest mite-code-oss-local-desktop.
- Add --firefly-ready to the real-source probe to observe the actual asynchronous
  app ready branch for three seconds. It always exits 1 with workbenchAccepted:false;
  an import or readiness is deliberately not a desktop pass. Update guide/status/
  roadmap/compatibility and ADR 0149 with the precise opt-in scope.

Validation: local Node/Mite policy and exact-source tests pass. A real compiled
11,536-source tree with the actual prepared root production packages completes
its entry import only in enabled mode; the real ready callback then reports missing
generated src/nls.messages.json and unsupported node:zlib. The same tree with mode
0 still rejects allowServiceWorkers. The first compile invocation referenced an
absent dependency path and its prematurely attempted probes only proved missing
output; correct the path and accept only the later completed compilation/probes.
The final formatted-tree compilation passes and all three actual-entry assertions
repeat on that output (build/code-oss-local-desktop-final-{0,1,1-ready}.log).
The standalone Mite Release build and all six mite-code-oss-* CTests pass, including
the new policy gate. The independent engine regression, full maintained-source style,
unmodified-upstream, whitespace and 258-page/18,245-local-link site checks pass.
No physical desktop input automation, actual
workbench, service workflow or new native Linux/Windows acceptance is claimed.

Remaining: generate genuine NLS assets and implement demanded node:zlib behavior;
continue the actual ready/main module graph, then connect and accept the real local
editor opening/editing/saving a file. Native File request transport/public webUtils,
MessagePorts, worker/cache/resources, extension/shared/PTY hosts and native platform
packaging remain part of the original goal. This experiment does not replace full
webviews/remote requirements. Ladybird is unmodified and Mite remains independent.


## 2026-11-08 — Generate actual indexed Code-OSS English NLS

- Resume from pushed 56b4d1b as concrete progress. Recheck the clean tracked worktree,
  spec/progress and actual ready failure. Prioritize the missing build catalog before
  implementing the demanded zlib streaming contracts; do not install import stubs.
- Inspect the pinned upstream NLS build/analyzer/runtime. Add --nls to the real
  source compiler: assign globally consistent localize/localize2 indices before
  transpiling sorted modules, preserve English fallback arguments and generate the
  matching messages/keys/metadata/ASCII JavaScript catalogs. Preserve translator
  comments. Without the option, no NLS assets are generated.
- Fetch and compare three additional pinned upstream build sources against the
  materialized baseline, retain their MIT notices and manifest fingerprints. Execute
  the exact checked analyzer/serializer as build tools only. Pin the same upstream
  @typescript/typescript6 package at 6.0.2 in the separate tool lock; it reports
  compiler 6.0.3, so verify/report both identities. This adds no Node/V8 runtime
  dependency or application devDependency installation.
- Decode only literal message/key/comment expressions without eval. Reject dynamic
  forms, changed spans, overlap/order violations and generated-path collisions
  before creating output. Record builder/analyzer/serializer hashes, package/version,
  catalog counts and generated artifact hashes in ELYTRA-COMPILE.json.
- Register mite-code-oss-nls. Test the real upstream binding analyzer with namespace
  and alias imports, shadowing, localize/localize2 ordering, empty/multiline/Unicode
  strings, translator metadata, deterministic artifacts and malformed/dynamic input.
  Execute the actual pinned runtime nls.ts against generated indices/catalogs on
  Node and Mite; compare formatted text and original/value objects. Exercise actual
  compiler opt-in/default branches, artifact hashes and fail-before-output cases.
- Update current status/compatibility/roadmap, reproduction guide and ADR 0150.

Validation: local standalone Mite Release build and all seven mite-code-oss-* CTests
pass. The full final 11,536-source compilation with real root production packages
produces 34,273 messages across 2,501 modules. Verify current builder/generated hashes
and run the actual Firefly ready/bootstrap on that final tree: app ready is observed,
no NLS read error remains, and the next failure is unsupported node:zlib. The bounded
probe still exits 1; no window/workbench or open/edit/save acceptance is claimed.
Full maintained-source style, unmodified-upstream, 259-page/18,314-local-link site
and whitespace checks pass. A final compiler adjustment preserves untouched JS/JSON
bytes when NLS is inactive; the real compiler integration gate passes again.
No physical desktop GUI automation ran; native Linux/Windows execution and translated
language packs remain unverified.

Remaining: implement actual demanded node:zlib compression streams and flush behavior,
rerun the real main graph and handle subsequent demanded services/assets. The source
compiler remains diagnostic: complete generated workbench assets/bundles, secure
preload/File/MessagePort handoff, editor acceptance, extension/PTY hosts and native
platform packaging remain required. Keep full webview/remote requirements after the
limited experiment. Ladybird is unmodified, Mite independent, and the full original
product goal stays active.


## 2026-11-08 — Native node:zlib streams and actual Code-OSS IPC compression

- Resume from pushed 2131b4c as implementation progress; inspect spec/progress,
  current runtime and the actual node:zlib startup failure. Audit real Code-OSS
  raw compression classes and Node/native zlib references, preserving the existing
  independent Fetch/Web Streams inflater contract.
- Add a small C++ RAII codec host over the existing pinned zlib 1.3.2#2. Own native
  stream state, bound copied input/output per call to 1 MiB, clear native pointers
  before JS allocation/return and validate numeric/detached-byte inputs. Support
  raw/zlib/gzip encode/decode, final validation and gzip members/zero padding.
- Add authored ESM option/error policy and shared Transform facade, generated with
  the existing pinned compiler. Serialize real flush operations using private empty
  writable markers; preserve dictionary context across messages. Respect readable
  push pressure and yield steps through setImmediate. Retire handles/work on destroy,
  explicit close and GC. Keep consumer exceptions outside native error conversion.
- Register the actual zlib builtin with shared CJS/ESM classes/factories, sync and
  error-first convenience functions, bounded output collection and constants. Refuse
  unsupported options/dictionaries/flush kinds/reset/params/autodetection/Brotli/Zstd/
  crc32 explicitly. Keep full Node signatures/options/error/threadpool parity partial.
- Differential tests discover two Node pressure/close differences instead of hiding
  them: Node's emitClose:false does not deliver this close callback (reject that
  option in Mite), and destroying a pressure-blocked transform retains Node's pending
  write callback while Mite explicitly retires it once. Record both policies.
- Add mite-zlib with real Node/Mite/Python cross-codec bytes, input above native-step
  size, sync/async/fragmented streams, RFC7692 context takeover, corruption/checksum/
  truncation/output limits, forced native GC, pressure, close and cancellation. Pin
  actual ipc.net.ts and add a local component probe running its unchanged compression
  classes with real compiled VSBuffer/Emitter/Disposable/common IPC services on
  Node/Mite. Update API/status/guide/matrix/roadmap and ADR 0151.

Validation: standalone Mite and Firefly local Release builds pass. Actual Code-OSS
ZlibDeflateStream/ZlibInflateStream messages and context takeover pass under Node and
Mite. The rebuilt Firefly real ready/main graph passes zlib and now reports unsupported
node:assert (build/code-oss-zlib-ready.log); no editor/window/workbench is claimed.
The first native compile incorrectly treated a byte-copy vector as a fallible result;
fix that and accept only subsequent builds. The full runtime suite passes 82/83;
the remaining line-ending gate initially required new generated/source snapshots in
Git's index. After staging, that gate and the final zlib gate both pass (83 gates
accepted across the full run plus targeted repeat). Focused final zlib/real-wrapper
checks, full maintained-source style, unmodified-upstream and generated integrity
checks pass. The site/link and whitespace checks accompany the commit. No GUI automation ran.

Remaining: actual node:assert contract and subsequent demanded main services, followed
by real workbench assets/window/preload/ports/File handoff and open/edit/save. Keep
large-file streaming/performance, extension/shared/PTY hosts, full webview/remote and
native platform packaging in the original goal. Zlib remains partial and fresh
Linux/Windows acceptance is not claimed. Ladybird is unmodified, Mite independent,
and the original full goal stays active.


## 2026-11-08 — Portable node:assert and real Code-OSS CLI validation

- Resume from pushed 0751d43 as concrete progress. Re-read spec/progress/current
  source and actual ready failure. Audit Code-OSS's truthy CLI validation and package
  assertion usage rather than bypassing the import or installing an inert function.
- Pin assert 2.1.0 and all portable dependencies with source/license/integrity records.
  Extend the existing generator's explicitly allowed authored ESM inputs and portable
  bundle list. Bind internal util to the provider's full private portable types/
  inspection implementation; no public node:util support is fabricated. The resulting
  assertion bundle has no external runtime imports and carries all 37 package notices.
- Add callable assert and shared strict/CJS/ESM aliases, real AssertionError metadata,
  equality/deep structures and exception APIs. Keep known opaque/modern comparison
  state and traversal/accessor cases explicitly unavailable, including negative
  assertions, instead of reporting an unproven success. Preserve missing-argument
  arity and existing validated identity behavior. Final metadata coverage catches
  and fixes the provider's generatedMessage flag on automatic truthy failures;
  callable/ok/strict identities remain shared and are rechecked.
- Differential evidence catches the provider's older exception-mismatch contract.
  Adapt constructor/RegExp/predicate mismatches to modern AssertionError while retaining
  the original actual/expected values. Forward validators through a Proxy so their own
  exceptions (even the same actual error) propagate unchanged. Preserve synchronous
  throws in rejects; validate current-realm Promises and refuse other thenables.
  Keep Assert/CallTracker/partialDeepStrictEqual and legacy fail overloads explicit.
  Additional differential cases find obsolete loose NaN comparisons, including
  false successful negative assertions. Compose three unique provider seams in
  memory; retain installed package files/notices and normal input/builder/output
  hash provenance. Modern scalar/nested NaN cases now match Node.
- Add mite-assert with real Node/Mite/native-host GC comparisons, metadata, loose/
  strict/deep collections, cycles/symbols, missing arguments, unwanted/missing errors,
  validator identity and asynchronous paths. Add negative policy cases. Pin the actual
  Code-OSS argvHelper.ts and run its unchanged compiled main/CLI parser on both hosts:
  valid goto passes, invalid line/column syntax throws AssertionError.
- Reproduce Firefly's manifest after the shared build lock changes; the first Firefly
  build correctly refused its stale input hash, and the later regenerated build passes.
  Update API reference, current matrix/status/guide/roadmap and ADR 0152.

Validation: final standalone Mite and Firefly local Release builds pass. All 84
enabled runtime gates pass again after the final generatedMessage and NaN fixes
in the complete local CTest run (84.30 seconds). Actual pinned Code-OSS CLI positive/
negative validation passes under Node/Mite. The final rebuilt Firefly ready/main
graph advances past assert and reports unsupported node:readline, with app ready
observed and no workbench accepted (build/code-oss-assert-final-ready.log).
Unmodified-upstream, both generated-source integrity checks, exact runtime bundle
reproduction, full maintained-source style and 263-page/18,593-local-link site checks
pass. Whitespace checks pass. No desktop UI automation, editor/window/workbench
acceptance or fresh native Linux/Windows result is claimed.

Remaining: implement the actually demanded readline contract and continue the real
main graph. Full workbench assets/window/preload/ports/File handoff and open/edit/save,
extension/shared/PTY hosts, efficient file streaming, full webviews/remote and native
platform packaging remain in the original goal. Assert is deliberately partial;
current Node diagnostic and full comparison/constructor/foreign-realm parity remain
open. Ladybird stays unmodified, Mite independent, and the full goal stays active.


## 2026-11-08 — Nonterminal readline and bounded native FileHandle streams

- Resume from pushed 8af2dbb as concrete progress; inspect spec/progress, actual
  readline failure and Code-OSS's real osReleaseInfo.ts/server prompt usage. The Linux
  path needs async line iteration over FileHandle.createReadStream, so implement a
  real file-to-lines slice instead of a successful import with empty methods.
- Add authored ESM UTF-8/CRLF parser, bounded queue iterator and EventEmitter interface.
  Reuse actual StringDecoder, AbortSignal, scheduling and Node-compatible stream APIs.
  Preserve elapsed split-CRLF timing/adjacent pairs, final nonempty text and the measured
  Node incomplete-UTF-8-tail behavior. Support nonterminal lifecycle/prompt/question;
  refuse TTY/completion/history/key/cursor/Promise APIs and unknown options explicitly.
- Propagate consumer exceptions, handle reentrant pause/resume and retire pending
  iterator reads on close/abort/error, including falsy error values. Input pressure
  defers remaining text rather than collecting the entire source. Document bounds
  and Mite's immediate iterator-return cleanup versus the observed Node 24 listener
  retention. Closing the interface pauses but does not destroy the caller's input.
- Add native FileHandle-to-Readable adaptation using existing bounded asynchronous
  reads, positional ranges, encoding and real descriptor ownership. Retain the actual
  handle through work, distinguish auto-close at EOF from explicit destruction and
  wait for outstanding native reads before close. Keep public close semantics intact.
- The first full suite catches changed idle-close Promise identity from a public
  close-order adjustment. Restore that existing Node-compatible behavior and supply a
  private callback to await any pending native close from the stream instead. The
  filesystem regression and new manual/in-flight close cases pass after correction.
- Additional pre-aborted signal tests expose synchronous Readable destruction before
  callback initialization. Install the callback first and avoid attaching it to an
  already destroyed stream. An intermediate duplicate declaration is rejected by
  generation/integrity checks; fix it and accept only the later passing build/tests.
- Add mite-readline and mite-code-oss-os-release. Real Node/Mite/native-host tests cover
  framing/timing/events/questions/abort, iterator pressure/return, consumer exceptions,
  encoded/ranged reads, ownership/destruction/manual close, 30,000 native file lines
  and forced GC. Pin and compile the actual OS-release function with truthful platform
  identity: the macOS early-return branch passes; native Linux file acceptance remains
  open. Update API/status/matrix/guide/roadmap and ADR 0153.

Validation: local standalone Mite and Firefly Release builds pass. After restoring
public close behavior, the full 86-test runtime suite passes (84.61 seconds).
The subsequently expanded pre/active-abort and encoded-file fixture passes under
Node/Mite/native GC after the constructor fix. Final targeted affected/integrity
checks all pass (five targeted gates, 6.98 seconds), including the filesystem
regression, expanded stream abort/encoding/native-GC fixture and actual OS module.
The final rebuilt Firefly ready/main graph passes readline and next fails linking
missing MessageChannelMain (build/code-oss-readline-final-ready.log), with no accepted
workbench/window/open-edit-save. Generated bundles reproduce exactly; upstream and
both adapter-integrity checks, full maintained-source style, 266-page/18,801-local-link
site checks and whitespace gates pass. No desktop input automation or
fresh native Linux/Windows acceptance is claimed.

Remaining: implement the real Electron MessageChannelMain/main-renderer MessagePort
contracts, preserving binary data, endpoint/process identity and revocation. Continue
actual startup and workbench assets/window/preload/File/open-edit-save acceptance.
Full terminal/readline/ReadStream parity, efficient file transport, extension/shared/
PTY hosts, webviews/remote and native platform packaging remain in the original goal.
Ladybird is unmodified, Mite independent and the full original goal stays active.

## 2026-11-08 — Real main MessageChannelMain ownership and Code-OSS Protocol

- Resume from pushed 7a8d112 and the actual missing MessageChannelMain failure.
  With the user's explicit multi-agent authorization, split public differential
  tests, pinned Code-OSS component acceptance and native lifetime review while
  implementing the shared core. Keep one coherent reviewed commit.
- Add maintained ESM main-realm connected MessagePortMain endpoints and export
  MessageChannelMain from the actual Firefly/Electron-alias host. Use the existing
  native BridgeData codec, start-gated asynchronous FIFO, copied binary aliases,
  repeated ownership transfer, detached old wrappers and generation-checked jobs.
  MessagePortMain is a returned type, not a fabricated exported constructor.
- Bound endpoint count, message count, encoded endpoint/global queue bytes and
  transfer-list length. Reject unsupported graphs/transfers explicitly. Preserve
  accepted peer messages before close; document Firefly's deferred local close
  difference. Stop channels on actual application exit/accepted quit.
- Keep canonical records weak with scalar queue leases so unreachable cycles of
  queued transferred wrappers collect and refund endpoint/wire budgets. Add a
  genuine headless Mite/native-codec/GC probe, without QApplication or mock cloning.
  Prove all 256 endpoint slots and near-global byte budget can be reclaimed.
- Lifetime review catches throwing/reentrant inherited emitter hooks during transfer:
  prepare wrappers first and revalidate ownership/generation/quotas before mutation.
  Capture registration/own-slot primitives for the tested mutation cases. Reject
  constructor shutdown reentry. Explicit close now retains the owner until its
  deferred notification; a weak-only negative control loses that callback under GC.
  This is not a claim against arbitrary privileged main intrinsic replacement.
- Compare actual public Firefly/Electron 45.2.3 start/FIFO, snapshot/cycle/binary,
  multi-hop transfer, detached owners, atomic rejection and close behavior. Add
  actual main-only native acceptance gates to check-firefly and Linux hosted CI.
- Pin the exact upstream ipc.mp.ts at Code-OSS revision
  6531648f5ace8b77ae799262bdc97bf048461938 with its original notice/hash. Run its
  unchanged Protocol and once helper with real compiled base services and public
  channels: binary frames, copy isolation, FIFO, echo, disconnect and moved once
  helper pass under both Firefly and Electron. No replacement Protocol is used.
- Document the API, ownership/GC/quotas and ADR 0154, and refresh current support,
  Code-OSS audit, implementation order and component reproduction commands.

Validation: local Release Firefly and the new native lifetime probe build. Fresh
public Firefly, genuine native codec/GC and actual Code-OSS Protocol probes pass;
Electron differential positives also pass. Both new CMake gates pass. The rebuilt
actual ready/main graph completes entry import and observes ready, then advances
from missing MessageChannelMain to missing utilityProcess
(build/code-oss-message-ports-final-ready.log). This is not workbench acceptance.
The full 86-test runtime regression passes (82.81 seconds). Unmodified-upstream,
both adapter-integrity checks, exact generated bundle reproduction and full maintained
source style checks pass. Site build/check accepts 268 pages and 18,948 local links.
Whitespace checks pass. No desktop input automation or fresh native Linux/Windows
acceptance is claimed.

Remaining: implement genuine utilityProcess/parentPort ownership and lifecycle,
native authenticated process/document port handoff and renderer event.ports. The
existing webContents.postMessage explicitly refuses rather than fabricating a
handoff. Main-realm port success does not establish renderer, utility/shared process,
zero-copy or large-file performance. Continue actual startup, workbench assets,
window/preload/File/open-edit-save, extension/PTY hosts, webviews/remote and native
Windows/Linux acceptance. Ladybird stays unmodified, Mite independent and the full
original goal stays active.

## 2026-11-08 — Real LibJS utility children, native payloads and process stdio

- Resume from pushed c0c3734 and the observed utilityProcess import failure. Split
  the native process/framing review, actual Electron/public tests and real Code-OSS
  consumer audit across the explicitly authorized agents; integrate one genuine
  child-to-parent vertical slice rather than an import-only constructor.
- Fork the actual executable through libuv, deriving its path natively. A private
  inherited duplex pipe binds the exact spawned child; no listening socket/public
  port IDs, Node/V8 fallback or renderer process stands in for a utility. Enter its
  Mite/LibJS runtime before Qt/desktop initialization. Preserve logical argv, actual
  PID/parent PID, environment replacement and native working directory selection.
- Add maintained ESM utilityProcess and child process.parentPort facades. Use actual
  native BridgeData snapshots with binary aliases/cycles, pre-spawn/pre-listener
  queuing, child {data,ports:[]} events and raw parent messages. Separate actual
  stdout/stderr pipes from the framing channel; keep PID/exit/EOF ownership distinct.
  Track kill state and terminate/reap live owned children on accepted app shutdown.
- Bound native child count, frame size, pending write count/bytes, child message
  queues and unconsumed output. Overflow destroys the output wrapper before a
  termination request, so ignoring SIGTERM cannot grow that retained stream queue.
  This is bounded output admission, not a true native pause/resume backpressure API.
- Six real early-helper malformed/missing-channel tests expose an absent-fd hazard:
  Mite/libuv can reuse fd 3 for its own wakeup pipe before initialization. Validate
  the inherited pipe before runtime creation; the formerly hanging no-fd negative
  control now rejects before entry. Idempotent spawn announcement also prevents a
  rapid exit overtaking the startup timer and later resurrecting a retired PID.
- Public differential tests expose missing Mite stdout/stderr: genuine messages
  already arrive, but the child throws before its echo while writing output. Add
  independent Writable adapters using existing asynchronous native fs.write on
  borrowed fd 1/2. Handle partial writes and zero progress; never close the borrowed
  descriptors when wrappers end/destroy. Add named stdout/stderr exports; exclude
  explicitly unsupported stdin from fixed named exports so process imports work.
- Actual Node/Mite stdio tests pass raw 64 KiB binary/UTF-8 output, callbacks/EOF,
  named import identity, actual three-byte short writes, descriptor survival after
  wrapper retirement and zero-progress EIO. Review native completion GC roots and
  deferred callback ownership; no new Firefly/LibWeb dependency is added to Mite.
- Actual Firefly/Electron 45.2.3 public tests pass genuine runtime identity (Mite
  without Node/Electron spoofing), queued binary graphs, args/env/cwd/stdio, final
  messages/natural exit 7, kill/noops and eight rapid failed module entries. Separate
  parent teardown observes a live owned child before app.exit and proves it absent,
  including native reaping rather than merely zombie state, before test cleanup.
- Refuse native MessagePort transfers, engine flags, unsigned library loading,
  authentication forwarding, service naming/metrics and unknown options explicitly.
  Pinning a named export does not accept Code-OSS utility startup: its actual shared
  process requires these native transfers and authentication behavior. Document the
  utility contract, ADR 0155, Mite stdio and refreshed support/priority matrix.

Validation: local standalone Mite and Firefly Release builds pass. The actual public
Firefly/Electron utility suite and live-child reaping fixture pass; all six genuine
headless native frame boundaries pass. New Node/Mite stdio fixtures pass. Actual
ready/main Code-OSS import observes ready and now stops at missing safeStorage,
not utilityProcess (build/code-oss-utility-final-ready.log). This does not establish a Code-OSS fork, service handshake or
workbench. The final full 87-test runtime suite passes (91.19 seconds), including the new
stdio gate. The final CMake utility/public-main-port/native-GC gates pass, and the
actual Code-OSS Protocol still passes on the final Firefly binary. Both generated
adapters reproduce exactly; upstream, source integrity, full maintained-source style,
270-page/19,098-local-link site and whitespace checks pass. No desktop input
or window automation is used; no fresh native Linux/Windows acceptance is claimed.

Remaining: continue real startup at safeStorage; connect the existing port ownership
core to native utility and renderer endpoints with identity, atomic transfer and
revocation. Run actual bootstrap-fork/shared-process services with demanded auth,
then workbench/window/preload/File/open-edit-save, extension/PTY hosts, webviews/remote
and full native platforms/packaging. stdin/TTY/full stdio, full utility lifecycle/crash
reporting and true large-file transport remain unfinished. Ladybird stays unmodified,
Mite independent and the full original goal stays active.

## 2026-11-08 — OS-backed safeStorage and actual Code-OSS encryption service

- Resume from pushed 96cd167 and the real missing safeStorage import. Audit exact
  pinned EncryptionMainService and Electron 45.2.3 native/export contracts. Code-OSS
  needs sync string-to-Buffer encryption, Buffer JSON persistence and decryption;
  the newer Electron async encryption API is a separate unavailable contract.
- Add maintained ESM main-only safeStorage and independently guarded native host.
  Validate types/readiness/byte budgets and return actual Buffer ciphertext. Capture
  the typed-array byte-length getter so a shadowed Buffer.length cannot turn a
  nonempty invalid ciphertext into the empty-Buffer compatibility case. Preserve
  UTF-8 replacement/NUL handling; refuse plaintext fallback, foreign ciphertext,
  authentication failures and unavailable async operations explicitly.
- Implement actual macOS OS-Keychain storage: a per-userData SHA-256 account owns a
  random 32-byte key under a versioned Firefly service. Availability reads status/
  existing key without creating; encryption lazily creates; decryption never
  silently replaces a missing/corrupt key. Handle competing first writers by reading
  the winning OS item. Validate key length/access and cleanse owned secret buffers.
- Use native OpenSSL AES-256-GCM with fresh 12-byte nonces, 16-byte tags and authenticated
  version/profile metadata. Bound UTF-8 plaintext to 1 MiB; accept/authenticate the
  exact native maximum and reject overflow. No Node/V8 crypto or application-file
  plaintext key substitute is introduced. The Firefly format intentionally differs
  from Electron/Chromium; existing encrypted Electron data needs a future migration.
- Investigate macOS UI policy in Apple/Chromium primary sources: legacy keychains
  ignore per-query UI-denial controls. The production and test backends therefore
  share a short serialized save/disable/restore scope for Security's process-global
  interaction policy. Check restore failures and retry restoration on cleanup;
  document that unrelated in-process credential consumers can temporarily see denial.
  Do not permanently alter preferences or silently require unsigned-incompatible
  Data Protection entitlements as a different implementation.
- Keep explicit test-keychain injection inside separate native test executables,
  absent from production Firefly. Real temporary-Keychain tests prove AES persistence,
  nonce variation, binary/empty/max-size data, profile separation, all authenticated
  regions, truncation, wrong keys, malformed stored keys and locked denial. Verify
  zero key creation by availability/decryption and preservation of UI/search-list
  state. No test opens/writes the user's Login keychain or launches windows.
- Actual public-facade write/read/wrong-key phases pass across native process
  restarts. Direct-native bypass tests prove independent guards; surrogate/invalid
  UTF-8 replacement, type/Buffer/size checks and unsupported methods pass. Actual
  Firefly/Electron pre-ready reference checks pass; post-ready Electron encryption
  parity remains unverified, and unsafe interposition experiments were discarded.
- Pin exact original EncryptionMainService/encryptionService sources with MIT notices
  and hashes. Execute the unchanged compiled class with 39 verified dependencies:
  real OS encryption, persisted Buffer JSON and decryption across two native
  processes pass. Its explicitly minimal component lifecycle is not a full desktop
  app acceptance. Actual main ready/import now advances to missing WebContentsView.
- Add backend/public native gates and Linux hosted wiring (Linux tests only assert
  truthful unavailable behavior until a real provider exists). Document API/ADR 0156
  and refresh current support, Code-OSS priorities and reproduction commands.

Validation: local Firefly and both native safe-storage probes build. Real native
Keychain/AES-GCM, public isolated-Keychain phases and real Firefly/Electron pre-ready
checks pass. The actual compiled Code-OSS encryption component passes write/read;
actual ready graph reports missing WebContentsView with workbench unaccepted
(build/code-oss-safe-storage-ready.log). The final 87-test runtime regression passes (90.28 seconds). The native utility,
public-main-port and codec/GC regressions pass alongside both new storage gates.
Final native tests additionally prove exact maximum-byte acceptance, both prior UI
policy values and shadowed Buffer-length handling. Both generated adapters reproduce
exactly; unmodified upstream, source integrity, full maintained-source style and
whitespace checks pass. The documentation site check passes 272 pages / 19,241 local
links. No fresh native Linux/Windows crypto or production Login-keychain acceptance
is claimed.

Remaining: actual post-ready/default-keychain packaged/signed app acceptance,
Electron ciphertext migration/recovery, async encryption and native Linux/Windows
providers. Continue real startup at WebContentsView and genuine native utility/
renderer port handoff, auth forwarding, workbench/window/preload/File/open-edit-save,
extension/PTY hosts, webviews/remote and platform packaging. Ladybird stays unmodified,
Mite independent and the full original goal stays active.

## 2026-11-08 — Real WebContentsView composition, registry and independent contents lifetime

- Resume from pushed 85a65fb and the actual missing WebContentsView import. Audit
  pinned Code-OSS: its main workbench still uses BrowserWindow; the demanded view
  belongs to optional integrated browser panels and also imports webContents.
  Implement genuine composition/ownership instead of an import-only constructor.
- Share maintained isolation/preload preference validation between windows/views.
  Expose WebContentsView, a real webContents registry, immutable contents/window IDs,
  contents focus/owner/close and BrowserWindow.contentView composition. Keep main
  window contents outside root.children, matching actual Electron 45 behavior.
- Create actual detached Ladybird widgets under permanent native contents IDs.
  Distinguish WindowMain/ChildView state; presentation changes never replace the
  canonical view/document/renderer/channel identity. Put primary/extra widgets in a
  content container below native menu chrome. Reparent the same widget and native
  tree, keeping bounds, navigation, preloads and validated IPC intact.
- Query actual Qt widget parents and child order for public owner/children getters;
  native composition acceptance cannot pass using only JS ownership metadata. Order
  and reparent native first, then commit JS bookkeeping. Refused native operations
  preserve prior JS ownership. Detached owner lookup deliberately returns null
  rather than Electron's observed retained-last-owner quirk.
- Detach children before deleting a host. A host closes only its primary contents;
  orphan views keep working until explicit webContents.close. Route native close/
  load/title/crash/IPC events by permanent contents ID, independently of windows.
  Close retires queues/counters/channels once, removes registry authority and emits
  destroyed once. App shutdown releases all native views, including detached ones.
- Move file-picker lifetime to the persistent widget, cancel on transfer and refuse
  detached chooser requests. Keep frameless drag callbacks main-only and chrome
  operations window-only. Check late shutdown after preference/emitter reentry and
  in native creation. Bound child count64 / canonical contents256 and geometry;
  preserve exact logical zero-area edge coordinates without Qt rectangle overflow.
- Seven policy groups pass actual Node/Mite: bounds/receivers/refusals, native-failure
  metadata atomicity, reparent/reorder/snapshots, destruction and shutdown reentry.
  A real shared fixture loads distinct pages, validates renderer/preload identity,
  requested viewports, native tree ownership/order and concurrent binary invokes,
  then moves/detaches/reloads/closes views and destroys an old host with a surviving
  child. Both Firefly and Electron pass; no fake browser/session service is used.
- Static Qt initially exposes only Cocoa, so an offscreen request fails before app
  startup. Compile app/adapters once as shared OBJECT files; a separate test frontend
  adds the matching static offscreen plugin. Production/packagers do not include it.
  Fix its clean-target helper/resource dependency, not just an already staged build.
- The first actual offscreen constructor crashes because upstream macOS AX treats
  synthetic Qt window IDs as NSView pointers. Generate an owned Cocoa-only adapter
  with exact original source/header pins, license notices, byte restoration and
  missing/duplicate/drift refusal. Cocoa behavior stays active; non-Cocoa skips AX.
  Ladybird remains unmodified. The next run reaches real pages/IPC but catches stale
  hidden viewports; call upstream's existing push_viewport_size after geometry/moves
  and before navigation, preserving actual widget/device-scale geometry.
- The complete fixture now passes offscreen and Electron. Run one additional hidden
  production Cocoa batch for the actual backend: it also passes, with all windows
  show:false and no input automation. Keep on-screen GPU/compositor/AX/performance
  claims separate. Default test mode never falls back to the desktop; Linux hosted
  testing selects production XCB/Xvfb explicitly and is still unverified locally.
- Document API/ADR0157/current matrix and real startup priority. Adopting existing
  contents, nested views, BaseWindow/View exports, custom sessions/subframe preloads,
  background/radius styling, popup/CDP and actual Code-OSS BrowserView remain open.

Validation: production and offscreen Release builds pass. All 88 non-GUI runtime
gates pass (88.34 seconds). Shared real Firefly-offscreen/Electron and the one
production Cocoa-hidden fixture pass real pages/PIDs/viewport/binary IPC/native
composition and independent lifetimes. The actual prepared Code-OSS ready graph
now passes both view exports and next stops at missing node:fs createWriteStream
(build/code-oss-web-contents-view-ready.log), without an accepted workbench.
Final CMake native-view/utility/public-main-port/codec-GC gates all pass. Native
ownership queries validate actual Qt parents/order; six final affected source-policy/
line-ending gates also pass (1.95 seconds). Both generated-source
integrity gates and exact Firefly adapter reproduction pass; upstream remains
unmodified, full maintained-source style and whitespace checks pass. The site
build/check accepts 274 pages / 19,387 local links. Fresh Linux/XCB/Windows,
on-screen/GPU/compositor/accessibility and performance acceptance remain open.

Remaining: implement the actually demanded bounded native file-write stream, then
continue real main startup/window/workbench acceptance. Complete integrated-browser
session/subframe/popup/CDP and view styling/adoption/nesting only with real behavior.
Native utility/renderer MessagePort handoff, auth forwarding, File/open-edit-save,
extension/PTY hosts, webviews/remote and native platforms/packaging remain required.
Mite is independent, Ladybird unmodified and the full original goal stays active.

## 2026-11-08 — Bounded native WriteStream and actual next Code-OSS startup blocker

- Resume from pushed 223dbdd and the actual missing fs.createWriteStream export.
  Implement maintained ESM WriteStream/createWriteStream and FileHandle writer
  factories over real asynchronous native open/write/close, never whole-file
  buffering or a Node runtime. CommonJS/default/named exports share the API.
- Split actual native requests at 64 KiB; continue short writes with correct
  positions/counts, bound zero-progress/EAGAIN retries and preserve Writable queue,
  UTF-8 byte pressure, callbacks, drain, finish and close. Numeric descriptors are
  not wrapped in a second native owner. Explicit destruction closes even when
  autoClose:false; ordinary finish with that option preserves the descriptor.
- FileHandle writers hold a lease spanning queued operations. Public handle.close
  gracefully ends attached writers; queued writes drain before native retirement.
  Destruction waits for the complete pending partial-write loop, releases its lease
  before awaiting native close, and prevents new leases after closing begins.
- Seven shared Node24.15/Mite real-file workflows pass exact observations and bytes:
  pressure/events/binary queues, flags/offsets/append/URL/mode, raw/handle ownership,
  queued handle close, errors/abort and explicit close. Record one deliberate
  difference: pre-aborted Mite paths never open/create/truncate; Node may create an
  empty target after its close event. The runner permits only that exact file delta.
- Validate 28 invalid/unsupported option configurations without create/truncate.
  Unknown/custom-fs/flush options fail explicitly. An internal factory test forces
  genuine seven-byte OS writes across more than 10,000 requests and checks positions,
  request bounds and exact bytes. Separate simulated zero/EAGAIN tests prove bounded
  retry failure and actual descriptor retirement; these are not native error parity.
- Existing embedded LibJS host forces GC while native writes are pending, after
  application references disappear. End and queued manual-close cases pass callback
  survival, single descriptor retirement, weak reclamation/reused-fd safety and
  independent verification of two 8 MiB+3-byte files. No desktop test is needed.
- Pin unchanged MIT Code-OSS zip.ts and add a probe using the actual compiled consumer,
  verified dependencies and genuine yazl. Implement the demanded util.inherits with
  Node differential valid/invalid/prototype/descriptor/side-effect coverage. Node ZIP
  reference plus independent Python CRC/extracted bytes pass. Mite ZIP remains
  unaccepted: dynamic CommonJS import does not expose genuine yazl.ZipFile. Preserve
  the consumer/package unchanged; fix the loader in a later coherent slice.
- Rebuild production and its exact-object offscreen frontend. Actual indexed-NLS
  Code-OSS main/ready graph now passes createWriteStream and next fails linking
  node:child_process exec (build/code-oss-write-stream-ready.log). Entry and app-ready
  observations are not workbench acceptance. Update API/status/roadmap documentation.

Validation: Release runtime and production/offscreen builds pass; writer differential,
policy, real partial-write, simulated retry, native-GC and util.inherits gates pass.
The first full 90-test regression passes 89; only the checkout line-ending gate
requires staging the newly added sources before its git checkout-index operation.
After staging, all six final affected gates pass, including checkout line endings,
filesystem, writer, writer GC, inherits and generated integrity (2.93 seconds).
Together with the full regression, all 91 registered runtime gates have acceptance.
Full maintained-source style/whitespace and upstream/generation checks pass; the site
accepts 275 pages and 19,463 local links. Production/offscreen builds pass and the
actual ready probe reports the next exec linking failure. Ladybird remains unchanged.

Remaining: genuine shell exec and next actual startup/window/workbench blockers;
proper CommonJS named-export interoperability for ZIP; native utility/renderer port
handoff, auth, File/open-edit-save, extension/PTY hosts, webviews/remote and platform
packaging. No new native Linux/Windows acceptance or usable VS Code is claimed.
The full original goal remains active.

## 2026-11-08 — Genuine shell exec and unchanged Code-OSS terminal encoding

- Resume from pushed 77304d0 and actual missing node:child_process exec import.
  Add shared default/CommonJS/named exec over the native process backend. POSIX
  passes the original command unchanged to /bin/sh -c; custom shells are actual
  executables. Windows source uses ComSpec/cmd.exe /d /s /c with native verbatim
  arguments; native Windows execution remains unverified, not inferred from macOS.
- Reuse execFile's bounded capture, actual ChildProcess/stdio lifecycle, encodings,
  timeouts/kill and callback settlement. Error cmd preserves the original command.
  Direct spawn/execFile continue rejecting their unsupported shell option. Both
  buffered functions expose custom promisify with immediate actual promise.child
  and stdout/stderr on rejection, without a Node/V8 runtime dependency.
- Compare real Node24.15 and Mite shells for Unicode/quoted arguments, pipelines,
  cwd/env, custom shell, binary and stdin capture, nonzero exits, per-stream output
  limits, timeout, callback/close order and promisification. Seventeen invalid calls
  cannot execute a sentinel command. Actual oracle corrects the earlier audit:
  empty commands throw synchronously. Explicit empty shell strings perform literal
  direct execution (pwd succeeds; unsplit printf a fails ENOENT), also compared.
- Pin unchanged MIT terminalEncoding.ts and verify three actual compiled dependencies.
  Remove VSCODE_CLI_ENCODING in the probe environment to require the genuine command
  branch. Both Node and Mite receive independently checked real locale charmap raw
  output and normalize UTF-8 to utf8 in the unchanged Code-OSS consumer
  (build/code-oss-exec-component.log). This is component, not PTY/workbench acceptance.
- Force actual embedded LibJS collection during delayed real shell stdout/stderr and
  a timeout after application ChildProcess references disappear. Complete output,
  original-command timeout errors and callbacks survive; timeout replaces its shell
  before killing the owned process, avoiding orphan descendants. POSIX fixture only.
- Rebuild production/offscreen frontend; actual local-desktop/indexed-NLS ready
  graph now passes exec and next fails linking desktopCapturer
  (build/code-oss-exec-ready.log). Entry/app-ready observations do not accept a
  workbench. Audit actual consumers: screen-source selection and macOS screen
  permission use zero-thumbnail enumeration; source identity/permission/media binding
  require genuine native implementation, never fabricated empty source success.
- Update child-process/util APIs and compatibility/status/roadmap guides. Preserve
  explicit coercion, AbortSignal, process supervision, platform and optional browser
  limits. The separate real ZIP consumer still needs proper CommonJS named exports.

Validation: Release runtime and production/offscreen builds pass. All 92 registered
runtime gates pass in the full regression (34.19 seconds), including existing spawn,
execFile/promisify, the new real-shell differential and line-ending/integrity tests.
The additional actual embedded-host shell GC gate passes separately. All eight final
staged affected gates pass (6.69 seconds), covering shell/GC, existing child/execFile/
promisify, generated integrity and checkout line endings: all 93 registered runtime
gates have acceptance. Maintained-source formatting, exact generated reproduction,
upstream/whitespace checks and the site (275 pages / 19,466 local links) pass.
Production/offscreen Release builds and the actual Code-OSS component/ready probes
pass their stated diagnostic scope; no desktop input or visible window test ran.

Remaining: real desktopCapturer and subsequent main/window/workbench acceptance;
CommonJS named exports for ZIP; utility/renderer transferred endpoints, auth,
File/open-edit-save, extension/PTY hosts, webviews/remote and native platforms/releases.
No native Windows/Linux exec or VS Code workbench acceptance is claimed. Mite remains
independent and Ladybird unmodified; the full original goal stays active.

## 2026-11-08 — Genuine CommonJS namespace analysis and Code-OSS ZIP acceptance

- Resume from pushed 545a36e. Audit actual desktopCapturer consumers and native
  providers: Code-OSS needs screen-source identity/permission and zero thumbnails;
  even that Electron call can prompt. Native no-prompt preflight confirms existing
  CoreGraphics IDs and ScreenCaptureKit availability only. Do not enumerate/capture
  or fabricate sources. Additional actual missing imports/session handlers remain
  real work; the main graph's desktopCapturer blocker is unchanged.
- Implement the independent required CommonJS named-export slice that blocked the
  genuine Code-OSS ZIP writer. Reuse pinned cjs-module-lexer2.1.1's pure JS parser
  privately on LibJS; C++ owns metadata, resolution, cycles and snapshots. Preserve
  package registry integrity, source hashes and MIT notices. No public parser
  builtin, Wasm, Node/V8 runtime dependency or Ladybird modification is introduced.
- Static analysis discovers names before application evaluation, follows actual
  CommonJS reexports and skips unresolved/builtin/JSON/ESM targets. Publish direct
  names before recursion; bound analysis depth512. Quote arbitrary generated names
  as UTF-16 data, not identifiers. Link errors occur before CJS native side effects.
- Snapshot recognized own values only when the genuine CommonJS module evaluates.
  Getter errors become undefined; property-check traps propagate. Named slots retain
  snapshots while default/module.exports markers share the actual whole export,
  including function exports, cycles and marker collisions. Private original-export
  storage preserves require.cache.exports replacement semantics and refreshes after
  actual evaluation/reload. Root originals and captured values across arbitrary
  getter code/GC; private lexer/parser/cache ownership is realm-local.
- The first Node comparison catches a provider discrepancy: Node24 native discovery
  retains prior recognized names even after unsupported getters; both upstream JS
  provider2.1.1 and2.3.0 remove them. Apply one exact guarded build-time filter seam
  without altering dependency files. Twenty-six actual Node/Mite namespace cases
  validate getters, literal/computed classifications, quotes/newlines/injection text
  and surrogate handling. No runtime enumeration broadens static detection.
- Eight public workflows pass lexical false positives, snapshots, reexport chains/
  cycles, cache replacement/reload and missing-export no-early-execution behavior.
  Review finds failed reexport analysis retained incomplete metadata after invalid
  package config. Add ScopeGuard rollback for every failure path and a real retry
  workflow: repair config, import new query identity, receive the correct named value.
- Actual embedded-host GC proves private original-only exports survive pre-import
  collection, earlier ephemeral getter results survive later getter-triggered GC,
  and the private parser/namespace cache remain alive. Update API/matrices/concepts/
  troubleshooting and ADR0158 with supported behavior and remaining source-edit,
  advanced hook/coercion/native-platform limits.
- The unchanged compiled Code-OSS ZIP writer now passes under actual Node and Mite
  with genuine yazl and34 verified compiled dependencies. Independent Python verifies
  archive CRCs and exact binary/Unicode/NUL/empty/prefix contents; cancellation/open
  errors and progress/finish/close paths pass (build/code-oss-cjs-zip.log). This is
  real component acceptance, not extension installation or a workbench.

Validation: all95 runtime gates pass the full regression (34.74 seconds), including
namespace/classification and existing runtime/builtin/resolver/native services. The
additional embedded GC gate passes. All13 final affected runtime/builtin/resolver/
package/generation/line-ending gates pass (3.54 seconds); after explicit CMake GC
registration, all3 CJS gates pass (1.01 seconds), with96 registered gates validated
across the full regression and final affected reruns. Full maintained-source style
plus final changed-source checks, generated runtime/Firefly reproduction, notices,
upstream and whitespace checks pass. Production/offscreen builds pass; the shared
lockfile change also regenerates Firefly's input manifest. The site accepts276 pages
and19,541 local links. Final actual ready probe still reaches desktopCapturer with
entry/app-ready observed and workbench unaccepted (build/code-oss-cjs-ready.log).

Remaining: real desktopCapturer/permission/image/media binding, demanded main/window
services and workbench; utility/renderer port handoff, auth, File/open-edit-save,
extension/PTY hosts, webviews/remote, native platforms/packages/releases. Preserve
original source-edit and advanced-loader limitations. Mite remains independent,
Ladybird unchanged and the full original goal active; no usable VS Code is claimed.

## 2026-11-08 — Canonical module identity under reentrant CommonJS analysis

- Review after 1c75f27 finds a real native abort: a registered resolve hook starts
  import of the same CJS URL during reexport analysis; the nested load publishes a
  module, then the outer load constructs a second record for that URL. The engine
  correctly refuses competing module records. Recheck our canonical module cache
  after analysis callbacks and return the record already published by reentry.
- Add a real Node/Mite hook regression. Node24 rejects the measured flow with
  ReferenceError; Mite intentionally retains its first namespace and its then-known
  names. Verify all imports share one native namespace and the genuine default
  CommonJS value, without a fatal engine abort. This is not full Node reentry parity.

Validation: rebuilt native Release runtime; CJS reentry/namespace/classification/GC
and resolve-hook gates all pass (5/5, 0.90 seconds). The ordinary actual Code-OSS ZIP
consumer remains accepted; Ladybird is untouched. Desktop capture implementation
continues separately; the full goal and VS Code workbench remain incomplete.

## 2026-11-08 — Native desktop source metadata and empty NativeImage

- Continue after canonical CJS reentry fix47e084b. Implement desktopCapturer and
  nativeImage main-process exports, not import-only placeholders. The real SCK
  provider enumerates screen/window metadata with native CGDirectDisplayID/CGWindowID;
  screen display_id matches existing AppKit/CoreGraphics screen identities. Platforms
  without a provider reject explicitly. No pixel, screenshot or stream API is added.
- Implement the demanded zero-thumbnail contract. Either zero dimension skips image
  work; default/nonzero thumbnails and requested icons fail explicitly. Return real
  empty NativeImage values, not fake nonempty images: zero bytes/size, measured data
  URL/aspect ratio/scale factors/template behavior and empty transforms. Nonempty
  codecs and native handle APIs remain explicitly unsupported.
- Separate provider/C++ dispatcher/JS policy. The provider admits8 actual OS requests
  process-wide, bounds1024 sources/name4096UTF8bytes/aggregate1MiB, and releases slots
  only on the actual OS callback. JS30-second deadline/cancel never refunds an OSslot.
  Callback blocks retain plain metadata/weak native state only, never VM/GC objects.
- Marshal worker completion through the owner QObject under a posting/teardown
  mutex, then through the established bridge busy-VM/job path. Monotonic IDs cannot
  be reused; cancellation/exit removes delivery authority. Stop invalidates native
  dispatch before receiver/VM teardown. JS bounds pending requests, rejects shutdown,
  validates canonical IDs/complete metadata before filtering and rechecks reentrant
  option-getter exits. Source strings grant no renderer media capability.
- Verify raw ID semantics against primary Chromium source: the third component is
  a native-window registry token, not a process-ownership flag. Raw SCK source IDs
  therefore end:0; remove the earlier uncommitted own-process assumption and reject
  nonzero fabricated tokens. No Chromium registry or upstream patch is introduced.
- Node/Mite policy units pass readiness/options, metadata/image projection, malformed
  IDs/duplicates/UTF8/count/aggregate bounds, provider errors, synchronous/reentrant
  completion, getter reads, quota/deadline/late completion and shutdown. Safe actual
  Electron probes measured only malformed-option rejection and empty-image behavior;
  no valid Electron capture request or TCC prompt was invoked.
- Exact production Host.cpp compiles with a controlled native completion peer.
  Real worker threads/Qt queues prove owner-only delivery, data/errors/once-only,
  strict IDs, quotas, cancellation, stop, receiver deletion and callbacks after VM
  teardown. No OS provider is linked into this component acceptance executable.
- Genuine provider compiles and actual no-prompt CGpreflight/native display probe
  passes. Valid ScreenCaptureKit enumeration is implemented but UNTESTED: it may
  prompt even with0x0 thumbnails. Add a clearly opt-in native metadata/display-ID
  fixture, never included in default tests and never run here. No source/capture,
  installed-app permission, image/media binding or native Linux/Windows acceptance
  is claimed. Code-OSS session/display-media handler support is still missing.
- Rebuild production and exact-object offscreen frontend. Actual indexed-NLS/local
  desktop main graph now passes desktopCapturer/nativeImage imports and next fails
  linking Notification (build/code-oss-desktop-capture-ready.log). Entry/app-ready
  observations are not a workbench. Update API/matrices/status/roadmap and ADR0159.

Validation: all98 runtime gates pass the full regression (31.94 seconds), including
new policy and prior CJS reentry. Native no-prompt provider and controlled dispatcher
component checks pass. Final five identity/affected/line-ending/generation gates
pass (1.03 seconds), including the corrected raw-window registry field. Exact runtime
and Firefly generation, maintained-source formatting, upstream and whitespace checks
pass. Production/offscreen Release plus both component targets build and run their
stated no-prompt scope. The site accepts279 pages /19,754 local links. The actual
ready probe reaches Notification; source enumeration/capture remain unaccepted. No desktop input, valid capture enumeration, screenshot pixels,
OS permission request or visible window tests were run.

Remaining: Notification and subsequent demanded native main services; actual SCK
source acceptance, nonempty NativeImage/thumbnails/icons, session/media/permission
binding, utility/renderer port handoff, auth, File/open-edit-save, extension/PTY hosts,
webviews/remote and native platform/packages/releases. The full goal stays active;
Mite independent, Ladybird unchanged, VS Code workbench still unaccepted.

## 2026-11-08 — Native Notification scheduling, response ownership and controlled provider acceptance

- Resume from pushed b442141 and actual missing Notification import. Implement
  main-only Notification/EventEmitter and native macOS UserNotifications provider,
  not an import-only facade. Mite remains independent; other platforms explicitly
  report unavailable. Constructor requires readiness; supported() is read-only
  OS/bundle/delegate eligibility, false for bare CLI, never a permission grant.
  Only show may ask authorization/schedule; actual OS delivery is still UNTESTED.
- Own mutable title/body/silent/button actions in validated JS snapshots. Bound
  UTF8 title4096/body65536/actioncount4/actiontext1024 and64 active generations with
  monotonic positive INTMAX tokens. Unknown properties/options fail explicitly.
  Show replaces its previous generation; retire before native close/user callbacks.
  Version checks preserve reentrant show/close/exit and suppress stale support,
  managed close and terminal callbacks. Native submission exceptions queue failed.
- Native host validates JSON512KiB/options/text/actions and tokens before admission,
  creates the provider lazily and queues even synchronous native callbacks to the
  owner. No VM/realm/GC object crosses an OS queue. Retire terminal tokens before
  cleanup; stop revokes receiver/pending state under the posting mutex, then calls
  provider teardown outside the lock. Reuse existing busy-VM/job delivery plumbing.
- Native provider coalesces settings/authorization, cancels waiting generations
  before unnecessary permission requests, schedules actual UNNotificationRequest
  content and routes real default/button/dismiss responses. Successful add callback
  acknowledges request acceptance, not a visible banner. Managed close timing is
  an explicit Firefly lifecycle difference; OS removal is best effort.
- Native OS callback tickets are process-wide/bounded64 and release only on actual
  completion. Close/stop removes pending/delivered IDs and late add removes its old
  ID again. Per-provider UUID/request generations and unique action categories avoid
  canceled requests acquiring new button labels. Serialize category read/merge/write
  transactions and preserve foreign snapshot categories. Never replace a foreign
  center delegate; remove only our own delegate/categories/requests on stop.
- Shared Node/Mite policy passes readiness/options/byte limits, snapshots, payloads,
  repeated generations, close/show/terminal/getter reentry, queued failures, quotas,
  shutdown and late events. This is controlled-peer policy, not Electron/OS parity.
- Exact production C++ host passes strict input checks and real worker-to-owner
  event delivery with a controlled native provider: show dedup, action/click/close/
  failed terminals, invalid payloads, quota, reused IDs, sync close/stop callbacks,
  receiver deletion and completions after VM teardown. No OS provider is linked.
- Add black-box state tests for the exact compiled Mac provider. Test-only Objective-C
  peers intercept bundle/center singleton factories before any provider call and
  never invoke their original implementations; real notification value objects are
  retained. Controlled settings/auth/add/response queues prove coalescing, canceled
  authorization, denial, acceptance, late add cleanup, native response mapping,
  unique categories, queued cross-generation category/foreign ownership, and actual
  callback-ticket lifetime. These do not establish OS authorization/UI delivery.
- Actual bare native identity probe passes false without center access. No Electron
  Notification operation is invoked: its lazy supported() can request permission.
  Add API/ADR0160/status/compatibility docs. Refresh the modern notification example
  to partial/manual-only, native accepted-vs-visible result, failed/closed handling
  and bounded cleanup. Fresh Firefly/Electron catalog staging/syntax passes; apps
  never launch. Current catalog6smoke/2manual/15blocked is source classification,
  not new GUI acceptance; historical hosted counts remain unchanged.
- Rebuild production/exact-object offscreen frontend. Actual local-desktop/indexed
  NLS graph passes Notification and next fails linking powerMonitor
  (build/code-oss-notification-ready.log). Entry/app-ready are not a workbench.
  Audit next actual idle/battery/thermal/power/lock/shutdown requirements read-only;
  no sleep, lock, power-source or OS preference changes are triggered.

Validation: all99 runtime gates pass the full regression (30.03 seconds). Production
and component targets compile; JS policy, controlled native host, exact controlled
Mac provider state and bare preflight all pass. All5 final staged affected/line-ending/generation gates
pass (1.01 seconds). Maintained-source formatting plus final example checks,
runtime/Firefly generation, upstream and whitespace checks pass. The site accepts
281 pages /19,897 local links. Final production/offscreen and all3 native component
targets build and pass their stated scope. The final-code bounded ready diagnostic
again reaches powerMonitor, with entry/app-ready observed and workbench unaccepted
(build/code-oss-notification-final-ready.log). No actual authorization,
OS notification, visible window, user-input automation or original OS-center call
was run by the controlled tests.

Remaining: powerMonitor and subsequent native main services/workbench; real signed
OS notification permission/delivery/user action and SCK source acceptance; nonempty
images, media/session binding, utility/renderer port handoff/auth, File/open-edit-save,
extension/PTY hosts, webviews/remote and native platforms/packages/releases. The full
original goal remains active, Ladybird unchanged, Mite independent and VS Code
workbench unaccepted.

## 2026-11-08 — Native passive power queries and owned observation epochs

- Implement main-process powerMonitor with genuine macOS idle/session, thermal,
  battery/AC and internal CPU-limit observations. Maintained ESM, native dispatcher
  and platform provider remain separate; Mite independent, Ladybird unmodified.
  Missing session information yields unknown; unavailable data never becomes AC,
  nominal temperature or a fabricated 100-percent CPU limit.
- Passive AppKit/Foundation/distributed/IOKit observations use no power acknowledgement
  hook, assertion, preference change or synthesized input. Shutdown observation has
  no inhibition: preventDefault fails explicitly. Native Windows/Linux providers
  remain unavailable; actual physical-transition delivery is UNTESTED.
- Seal native capabilities once per host. Supported pre-ready listeners defer native
  registration; first/last listeners own one monotonic epoch. Validate query and
  event schemas; stale callbacks/stops cannot affect replacements. Failed starts
  roll back, exit revokes delivery before cleanup, and cleanup failure cannot stop
  other app shutdown. Bound reentry exhaustion revokes the epoch and clears inert
  listeners; a fresh later subscription recovers.
- Native callbacks carry weak plain state to the Qt owner, never JS/GC objects.
  The exact dispatcher passes worker delivery, cached-capability validation,
  native query errors/Unicode, reused/stale epochs, failed startup, receiver deletion
  and VM retirement with controlled providers. OS provider observers/sources are
  released on the main thread before their state expires.
- Genuine native read-only queries compare against OS framework observations;
  passive observers start/stop twice. The production factory/host/OS component
  passes queries, two subscription epochs and real GC with a minimal ready context.
  This is not desktop lifecycle or induced sleep/lock/battery/thermal acceptance.
- Add API contract, ADR0161, compatibility entry and refresh current VS Code/status/
  roadmap/adaptation blockers. The actual final-code local-desktop/NLS diagnostic
  passes powerMonitor and next fails linking powerSaveBlocker. Entry and app-ready
  observed, no workbench accepted (build/code-oss-power-monitor-final-ready.log).

Validation: all100 runtime gates pass (32.20 seconds). After the final reentry/exit
cleanup fixes, shared Node/Mite policy passes seven groups and all10 selected
policy/generation/checkout gates pass (3.35 seconds). Production and exact-object
non-GUI frontend rebuild, native dispatcher, real provider and public native
component gates pass. Site accepts283 pages /20,036 local links; runtime/Firefly
bundle integrity, maintained formatting, upstream pin and whitespace checks pass.
No desktop input, physical power transition, permission prompt or visible window
was used by this slice.

Remaining: owned powerSaveBlocker/native main continuation; real notification and
SCK platform acceptance, nonempty images/media/session binding, native utility/
renderer endpoint handoff/auth, actual workbench/File/open-edit-save, extension/PTY,
webviews/remote and native platforms/packages/releases. The full goal remains active.

## 2026-11-08 — Owned native powerSaveBlocker assertions and real OS readback

- Continue from pushed ef80dd5 and actual missing powerSaveBlocker export. Implement
  main-only pre-ready start/stop/isStarted with Electron's two types, valid zero ID,
  signed-int32 unknown IDs and display-over-app precedence. Strict validation,
  negative-zero rejection and native error propagation precede mutation. IDs are
  per-host monotonic and active logical requests bounded64, not a JS shadow registry.
- The native macOS provider uses genuine IOPM NoIdleSleep/NoDisplaySleep assertions.
  Acquire replacements before releasing previous protection. Failed creation or
  release preserves logical state; failed rollback retains one extra known ID and
  prevents new allocations until cleanup succeeds. Bound native ownership to two
  IDs, including under100 repeated failed promotions. Shutdown revokes logical
  ownership permanently and releases only owned assertions; persistent native
  failure has bounded destructor retries, not a false cleanup guarantee.
- Native host is lazy and VM-independent: imports/unknown queries create no provider
  or assertion. Controller teardown revokes state before VM retirement. Malformed
  native admission shuts the service; JS malformed snapshots similarly revoke and
  request cleanup. Exit/reentry cannot publish stale success or prevent the rest of
  application teardown. No readiness guard: Electron permits these calls pre-ready.
  Synchronous OS error reporting differs from Electron's asynchronous backend.
- Shared Node/Mite policy passes five groups, including native-method getter exit,
  late admission cleanup, malformed results and shutdown failure. Exact Host/real
  Mite factory controlled-provider fixture passes strict IDs, UTF8 failures, real GC,
  lazy admission and controller/VM retirement with an outstanding request. Exact
  Mac provider fixture passes ownership/precedence/failure transactions with
  executable-owned IOPM symbols; original OS functions are never called there.
- Separately run the opt-in genuine native probe once: actual assertion create,
  aggregate, promote, downgrade, stop and shutdown all pass native process-record
  readback, with zero matching assertions after cleanup (0.14seconds). It neither
  triggers sleep/lock nor changes power preferences or UI. Physical idle/display,
  lid-close, forced sleep and native Linux/Windows behavior remain unaccepted.
  Unsupported-provider standalone compile/run passes explicit unavailable starts
  and unknown-ID false; it is not native Windows/Linux acceptance.
- Add API/ADR0162/compatibility docs and refresh current startup references, including
  old API-page blocker text. Final production/exact-object offscreen build passes.
  The real pinned Code-OSS local-desktop/NLS ready graph passes powerSaveBlocker,
  then fails linking shell (build/code-oss-power-save-final-ready.log). Entry and
  app-ready are observed; no workbench follows.

Validation: all101 runtime gates pass (37.28seconds). After the final exit guard,
focused shared Node/Mite policy passes and all11 selected policy/generation/checkout
checks pass (2.38seconds). Controlled native provider and exact Host gates pass;
actual OS assertion lifecycle/readback passes independently. Maintained formatting,
runtime/Firefly generation, upstream and whitespace checks pass. Website accepts285 generated pages and20,176 local links/resources/fragments. No GUI acceptance or physical power transition
was performed. Ladybird stays unmodified; Mite remains independent.

Remaining: genuine shell and subsequent native main services, actual workbench and
File/open-edit-save, utility/renderer endpoint handoff/auth, extension/PTY hosts,
nonempty images/media/session and real notification/SCK acceptance, webviews/remote,
native Windows/Linux packages and releases. The full original goal remains active.

## 2026-11-08 — Owned native shell dispatch, reversible Trash and non-GUI service gates

- Continue from pushed f6f9b03 and actual missing shell import. Implement main-only
  macOS showItemInFolder/openExternal/openPath/trashItem using NSWorkspace and
  NSFileManager, never command interpolation or permanent-delete fallback. No
  readiness guard. Unsupported beep/shortcuts/options and native Linux/Windows
  providers fail explicitly; visible app/browser/Finder behavior is UNACCEPTED.
- Validate primitive receivers/options/text, NUL, lone UTF16 surrogates and UTF8
  budgets (path32768/URL65536). Capture absolute paths before asynchronous work,
  preserving dot segments and symlinks. Foundation file URLs can expose canonically
  equivalent decomposed Unicode; exact-provider tests compare native URL and
  canonical full-path equivalence without losing spaces, percent or hash names.
- Own64 pending delivery IDs, monotonically increasing positive INTMAX. Register
  before admission; synchronous callback delivery waits until admission succeeds,
  while a synchronous admission throw rejects. Only ordinary completed OS openPath
  errors resolve a message; unavailable/admission/quota/data/exit errors reject.
  Corrupt current results retire only that request. Shutdown revokes delivery and
  rejects every promise; cancellation never claims to undo an OS launch/Trash move.
- Native host strictly validates512KiB JSON snapshots/keys and request IDs, queues
  plain bounded native results to the Qt owner, retires delivery before JS and
  revokes receiver/notify under the posting mutex before stopping the provider
  outside it. Normalize sync reveal errors as well as async results before LibJS
  conversion. No OS callback carries a VM/realm/GC root.
- Native provider uses actual launch configuration/handler preflight and a utility
  worker for Trash. A process-global64 ticket budget releases only on real native
  completion, including callbacks after stop/VM teardown; canceled delivery does
  not refund unfinished OS work. No fake timeout or delete fallback.
- Shared Node/Mite policy passes six groups including mixed success/error and
  malformed-surrogate records, option/native getter reentry, early callback/throw,
  quota/recovery and stop. Exact native Host passes real factory/worker owner-thread
  dispatch, strict validation, sync/async malformed results, actual GC, quotas and
  controller/receiver/VM disposal. Exact Mac provider passes controlled URL/config/
  error/worker/ticket tests without original launch/reveal/trash calls. Actual
  unavailable backend passes explicit failure and permanent stop without Qt/OS UI.
- Run opt-in genuine native Trash once: create a unique owned temporary Unicode
  file, move it via the production provider, capture that operation's exact native
  destination, restore it and verify bytes (0.19seconds). No Trash enumeration or
  unrelated file is touched; no application, browser or Finder is opened.
- Wire controlled/read-only desktop-capture/Notification/power/shell gates into
  the release builder and Linux renderer workflow. Run the entire corresponding
  macOS selection locally; fresh hosted/Linux outcomes remain UNVERIFIED. Permission requests,
  visible launch, genuine Trash and real power-assertion probes are not
  dependencies of that non-GUI selection.
- Add API/ADR0163/compatibility docs and refresh current blockers across guides/API
  pages/status/roadmap. Production/exact-object offscreen frontend builds. The real
  pinned Code-OSS local-desktop/NLS ready graph passes shell, then fails linking
  systemPreferences (build/code-oss-shell-final-ready.log). Entry/app-ready observed;
  no Code-OSS workbench or open/edit/save acceptance.

Validation: all102 runtime gates pass (35.20seconds). Six shared policy groups,
controlled native Host/provider, unavailable backend and independent actual owned
Trash/restoration pass. The newly wired release native service selection passes
locally with production/exact-object frontend rebuild. Maintained formatting,
Python release-script syntax, runtime/Firefly generation, upstream and whitespace
checks pass. All12 final selected policy/generation/staged-checkout gates pass
(3.09seconds), and the site accepts287 pages /20,319 local links/resources/fragments. No GUI tests or
permission prompts run. Ladybird remains unmodified and Mite independent.

Remaining: systemPreferences/media permissions and later main services, native
utility/renderer endpoint transfer/auth, real workbench/File/open-edit-save,
extension/PTY hosts, nonempty images/session/media and actual notification/SCK/
launch/reveal acceptance, webviews/remote, native platforms/packages/releases.
The full original goal remains active.

## 2026-11-08 — Native scalar systemPreferences, passive observers and truthful media status

- Continue from pushed1856629 and actual missing systemPreferences export. Implement
  main-only pre-ready string/boolean get/setUserDefault, all3 native notification
  families and read-only camera/microphone/screen status. Other types, posting,
  permission requests, appearance/colors/accessibility/TouchID/EventEmitter APIs
  explicitly fail. Native Windows/Linux providers remain unavailable.
- Use real NSUserDefaults typed getters/setters; absent native string/bool values
  follow those getters, not a simulated missing backend. Validate bounded scalar
  JSON/Unicode types. Materialize QJsonValue copies before temporary containers
  disappear: exact Host tests found and now guard the getter/setter reference
  lifetime crash. The actual isolated-suite probe verifies writes/fresh store reads.
- Observe local/distributed/workspace centers passively with0-based monotonic IDs,
  string/null names, kind-bound removal and owned callback/token lifetime. Wrong
  family/unknown removal is a documented no-op. Callbacks carry detached bounded
  userInfo/name/NSString sender; nonstring senders follow Electron's empty string.
  JSON data retains escaped NUL while identifiers/default values reject it.
- Bound64 active subscriptions and early packets, userInfo64KiB/depth8/nodes1024,
  text4096/stringdefault65536. Native queue bounds128 normal events globally and64
  per observer; overflow/corrupt native data emits one terminal failure and retires
  that subscription. Register before admission; failed start discards queued events.
  JS callback roots clear before removal/stop; weak native workers never own VM/GC
  values. Reentrant late admission receives another best-effort ID/family removal.
- Native observer tickets are process-global64 and release only on successful
  actual observer removal. Failed cleanup keeps inactive owned records and charged
  capacity for bounded teardown retry, preventing unbounded host/cancel churn.
  Stop revokes receiver/pending state under posting mutex, then native cleanup runs
  outside it. Controller/receiver/VM retirement drops all queued/late authority.
- Map AVFoundation authorization statuses directly. Screen preflight grants only
  when known true; false reports unknown because denied versus not-yet-requested
  is not distinguishable. Queries never request permission or grant renderer access.
  Actual read-only values independently match OS frameworks, with passive observers
  installed/removed; no physical/permission transition is induced.
- Shared Node/Mite six-group policy and exact Host/controlled worker/real GC/queue/
  teardown tests pass. Exact Mac provider with controlled defaults/centers/AV peers
  passes typed metadata, snapshotting, failed removal quotas/recovery and late stop;
  original OS writes/posts/permission methods never run in that controlled gate.
- Separately run opt-in genuine native suite/local test: redirect only the defaults
  factory to a real UUID-owned NSUserDefaults suite, write scalar values, synchronize,
  read from fresh native store/provider and verify exact owned-domain cleanup. Post
  one unique local notification only within the probe process, verifying real
  delivery/unsubscribe/stop. No personal defaults, distributed/workspace posts,
  permission requests or GUI operations; no cross-process durability claim.
- Wire new non-GUI Host/unavailable gates into Linux/release and controlled/read-only
  Mac gates into release; opt-in suite mutation stays outside defaults. Add API/
  ADR0164/compatibility docs and refresh current source blockers. Actual final-code
  Code-OSS ready graph passes systemPreferences and next fails linking fork from
  child_process (build/code-oss-system-preferences-final-ready.log). The actual
  static chain is main → diskFileSystemProvider → watcherClient → ipc.cp; its fork
  call is lazy and has NOT executed. Entry/app-ready is not workbench acceptance.

Validation: all103 runtime gates pass (33.02seconds). Controlled policy, actual
native Host/provider, independent read-only probe, unavailable backend and isolated
suite/local notification acceptance pass. Final production/exact-object offscreen
frontend and component builds pass. Maintained formatting/generation, release-script
syntax, upstream and whitespace checks pass. All13 final selected policy/
generation/staged-checkout gates pass (3.05seconds); site accepts289 pages and
20,462 local links/resources/fragments. Ladybird remains unmodified; Mite independent; no GUI test ran.

Remaining: genuine Mite child_process.fork/JSON IPC and runtime-only Firefly child
bootstrap for the watcher importer; native utility/renderer endpoint transfer/auth,
actual workbench/File/open-edit-save, extension/PTY, further session/media/images,
real notification/SCK/permission/physical event and visible launch acceptance,
webviews/remote, native platforms/packages/releases. The full goal remains active.

## 2026-11-08 — Genuine Mite fork/JSON IPC and synchronous process exit

- Resume from pushed cf31e05 and actual missing child_process.fork import. Extend
  Mite's existing libuv process/pipe service with a fourth duplex byte channel,
  preserving ordinary three-slot stdio/process mask behavior. No Node/V8 runtime,
  LibWeb/Firefly dependency or Ladybird source modification. Maintained ESM owns
  JSON policy in a separate IPC module; native owns handles, bytes and callbacks.
- Implement self-fork with inherited/default stdio, silent pipes, env/cwd/file URL/
  argv snapshots, JSON send/completion/pressure, connected/channel, disconnect,
  message/internalMessage and ref/unref. Wire frames cap at 1 MiB including newline;
  64 writes/4 MiB backlog and 128 KiB pressure. Buffer 64 messages/4 MiB before a
  listener and replay FIFO; child queues alone never manufacture an event-loop ref.
  Foreign executables, engine flags, advanced serialization and handle transfer
  fail explicitly. Additional IPC descriptors remain unavailable on ordinary spawn.
- Add an explicit internal runtime role with descriptor 3 and environment marker.
  Validate before file/VM/loop initialization can reuse a missing descriptor for
  the loop's own wakeup pipe (caught by negative bootstrap tests). A marker alone
  cannot install process IPC. Hide role/marker from child argv/execArgv/env; expose
  truthful file/eval execArgv. Eager child initialization installs process.send and
  admits message/disconnect events only under the private capability. parentPort
  is absent: Node IPC cannot select Code-OSS's Firefly utility protocol by mistake.
- Firefly routes that role into standalone Mite before any Qt/compositor/desktop
  setup. Exact production application objects pass parent ready → same executable
  child with deliberately invalid Qt plugin → native JSON snapshot reply → actual
  child exit/reaping, with no desktop imports or utility capabilities in the child.
  This batch is entirely windowless/offscreen, not a Cocoa/GPU acceptance claim.
- Native pending write/shutdown callback roots retire before loop teardown; libuv
  close/completion drains retained native storage without JS after VM retirement.
  Independent child processes are not killed at parent VM teardown. Failed forks
  retain a closing IPC facade with asynchronous EPIPE/CHANNEL_CLOSED; explicit
  channel ref choices override automatic write/listener holds. Native EOF and
  completed writes do not claim application receipt or perfect Node close timing.
- Actual unchanged Code-OSS ipc.cp Client/Server demanded process.once('exit').
  Implement one private native exit state shared by explicit and orderly exit,
  before host teardown. It emits once, supports listener-selected code, immediate
  nested exit and uncaught-listener failure. Preserve explicit host status and
  existing pending-work stop policy; new blocking exit work cannot extend lifetime.
  Timers/ticks are not driven. Node24 natural-exit Promise reactions differ: Mite
  currently does not drain those; this intentional difference is documented.
- Real Node/Mite self-child tests pass typed/base64 FIFO snapshots, 768 KiB pressure,
  late listeners, spawn failure, callbacks, EOF/disconnect/errors, reference policy
  and post-parent independent child survival. Controlled channel policy is labeled
  separately. Eight real Node/Mite process-exit cases pass, alongside CLI marker/
  inherited-fd rejection and ordinary-process capability denial. Both runtimes run
  the new modern ESM fork-job example and print [2,4,6]. No native Windows/Linux
  fork acceptance is claimed from these local macOS tests.
- Vendor exact ipc.cp TypeScript source/license at the existing Code-OSS pin; the
  unchanged compiled Client/Server passes under actual Node/Mite: 512 KiB VSBuffer/
  base64, concurrent calls, queued sender, remote errors, events and natural child
  exit. Verify 44 compiled dependencies including NLS. It is a real transport
  component, not the full watcher or extension host. No source consumer is replaced.
- Add API/ADR0165/example and update compatibility/current blockers. Wire standalone
  fork/exit gates into CTest and same-executable Firefly acceptance into release and
  Linux native batch (fresh hosted results pending). Final actual ready diagnostic
  passes fork and next fails linking fs.watchFile in baseWatcher suspended-path
  recovery (build/code-oss-fork-final-ready.log). Entry/app-ready are not a workbench.

Validation: full 105-suite runtime regression passes (38.63 seconds). After the
final bootstrap hardening, complete fork/reference/policy/capability checks pass.
Exact-object Firefly target and production binaries rebuild, and registered
check-firefly-fork passes. Actual pinned Code-OSS IPC component and eight exit
cases pass. All 9 final focused process/IPC/dependency/generation/checkout gates
pass (17.76 seconds). Maintained formatting, generation, upstream and whitespace
checks pass. The site accepts 290 pages and 20,540 local links/resources/fragments. No visible desktop/window/input test ran.

Remaining: native fs.watchFile/unwatchFile (uv_fs_poll, shared StatWatcher,
zero/missing/reappearance Stats, interval/ref/listener cleanup), actual watcher/
workbench/File/open-edit-save, native utility/renderer endpoint/auth handoff,
extension/PTY, further media/session/images, actual permission/notification/SCK/
physical-event/visible launch acceptance, webviews/remote and native platforms/
packages/releases. The full original goal remains active.

## 2026-11-08 — Native stat polling and actual Code-OSS missing-path recovery

- Resume pushed 272cc3f and implement the actual fs.watchFile/unwatchFile import
  required by the pinned baseWatcher. C++ owns genuine uv_fs_poll handles; maintained
  ESM owns shared absolute-path registrations, first-options policy, EventEmitter
  lifecycle and reference counts. No Node/V8 host, Firefly/LibWeb dependency in Mite,
  or Ladybird modification. Extract only the existing native stat projection into
  a shared lossless header; existing fs Stats conversion remains authoritative.
- Support actual metadata changes, initially missing zero/epoch Stats, suppressed
  repeated errors, deletion/reappearance with last-real previous history, BigIntStats,
  file URLs, uint32 intervals including zero, listener-specific/all removal and
  deferred stop. Match measured Node24 exact-true bigint, truthy persistent and
  Buffer rejection during path resolution; later registrations share first options.
- Bound native active plus closing watchers to 256. Stop removes lookup and callback
  roots before uv_close, retaining native Watch/service storage through real close
  completion and outstanding internal stat work. Admission errors preserve UV errno.
  Shutdown retires all active roots; unref controls liveness independently of GC.
- Real Node/Mite differential gates pass baseline/change/delete/recreate, missing,
  BigIntStats, validation, shared and duplicate listeners, references and natural
  exit. Separately labeled factory policy checks admission rollback/early delivery,
  listener/native reentry and stale callbacks. Genuine Mite capacity test proves
  closing handles still count, native drain restores admission, and 256 unreferenced
  active watches survive until orderly VM teardown without leaking native state.
- Actual embedded LibJS collection proves callback/watcher rooting while active,
  unwatch/GC inside a live native callback and final reclamation after close. The
  initial GC fixture accidentally retained its inspected WeakRef targets in a
  top-level-await frame; move active inspection into a completed helper frame,
  preserving the same final BOTH-collected assertion. No production leak was found.
- Vendor exact pinned baseWatcher.ts with its license/hash. Unchanged compiled
  suspension/recovery logic passes under Node/Mite at its actual 5007ms interval:
  real missing target, correlated ADDED event, resumed watch and stop/disposal.
  Verify 40 compiled dependencies. Harness supplies its abstract backend to report
  a missing target; this is not the complete Code-OSS watcher service.
- Add API contract/ADR0166, CTest differential/GC gates and refresh current startup
  blockers. The actual local-desktop/NLS entry still reaches app.ready and now
  passes watchFile, then fails linking the next missing export fs.watch. No window,
  renderer workbench or open/edit/save acceptance is inferred from that diagnostic.

Validation: all 107 runtime suites pass (39.70 seconds), including genuine new
polling/policy/capacity/GC tests. Pinned Code-OSS recovery component passes under
Node/Mite. Runtime/Firefly generated adapters and unmodified upstream pin verify;
maintained source formatting and whitespace checks pass. Final production and exact-object offscreen Firefly binaries rebuild. Site accepts 292 pages
and 20,679 local links/resources/fragments. No visible desktop test ran.

Remaining: native fs.watch and actual watcher/main continuation, workbench and
workspace open/edit/save, native utility/renderer endpoint/auth handoff, extension/
PTY, further media/session/images and real permission/notification/SCK/physical
acceptance, webviews/remote, native platforms/packages/releases. Fresh native
Linux/Windows polling acceptance remains unverified. Full original goal stays active.

## 2026-11-08 — Native filesystem events, canonical paths and Code-OSS watcher consumer

- Resume pushed abf9e93 and the actual fs.watch static import blocker. Add genuine
  C++ uv_fs_event ownership and maintained ESM FSWatcher policy, with individually
  owned handles rather than StatWatcher path sharing. Reuse private watcher path/
  genuineUV error helpers; previous stat polling keeps its behavior. Mite remains
  independent of Node/V8/Firefly/LibWeb and Ladybird stays unmodified.
- Preserve copied native filename bytes or null, supported Buffer encodings and
  rename precedence when the OS supplies both masks. Native path/name limits and
  separate 256 active/closing event handles bound ownership. Stop detaches roots
  before native stop/close; retained storage survives real close. Runtime shutdown
  drains native handles before VM retirement; ref/unref controls loop liveness.
- Implement native macOS/Windows recursive flags, with actual macOS nested-relative-
  filename acceptance. Linux recursion remains explicitly unsupported rather than
  pretending uv_fs_event supplies Node's separate directory-tree implementation.
  Private AbortSignal hooks resist public-listener mutation. Close emits once on
  nextTick; native asynchronous error closes/emits error without ordinary close.
  Unsupported non-null ignore filters fail explicitly; promise watching remains open.
- Exact Code-OSS consumers also demand realpath. Implement native uv_fs_realpath in
  the existing filesystem worker service, copying result bytes before request
  cleanup. Add sync/callback/promises and stable .native entry points, using existing
  path/Buffer/URL, encoding and callback-root ownership. Both ordinary/native forms
  use the OS canonicalizer; Node's JS symlink-walking fallback/error details differ.
- Actual Node/Mite event fixtures pass directory/file changes, fresh filename Buffers,
  create/rename, recursive names, validation, pre/post-aborted signals, close/ref/
  unref and externally driven process retention. Controlled production-factory
  checks separately verify masks/null, reentry, native errors, stale callbacks and
  admission rollback. Real native capacity/drain/shutdown and embedded collector
  tests prove active roots, close/GC during callback and final reclamation.
- Independent actual Node/Mite observations showed macOS appends can report rename
  for directory AND direct-file watchers. Tests retain genuine target-file/event
  assertions without inventing exact counts/classifications. Newly registered OS
  streams get an owner-loop startup interval before mutations, including Node.
- Canonical-path differential passes real symlinks, broken targets/loops, bytes/URLs,
  encodings, asynchronous callbacks/errors and native entry points surviving public
  wrapper replacement. Symlink-only tests explicitly report unavailable Windows
  privilege; non-UTF8 filesystem names are explicitly skipped on this macOS volume
  because the OS rejects creating them, not falsely reported as byte-name acceptance.
- Vendor exact pinned nodejsWatcherLib.ts with license/hash. The unchanged actual
  compiled consumer passes canonical path plus native create/update/delete,
  correlation/coalescing and disposal under Node/Mite; verify 39 compiled dependencies.
  This is the native event-consumer component, not the complete watcher service.
- Inspect actual hosted runs for abf9e93 through the GitHub connector. Source/style,
  Pages and pinned dependencies pass, but native Linux and macOS Release fail fresh
  links: CommonJSExports.cpp references undefined JS::Value::to_object. Include
  ValueInlines.h directly at the call site; recompilation/link succeeds and object
  symbol inspection confirms that missing external is gone. This is our adapter
  include fix, with no upstream change. Fresh hosted results remain pending.
- Add API contracts/ADR0167 and update current compatibility/startup priorities. Real
  local-desktop/NLS diagnostic reaches entry/app.ready, passes watch and realpath,
  then fails linking Electron net. Its consumers need isOnline, fetch and request;
  Mite HTTP/fetch alone does not establish Electron session/proxy/auth compatibility.

Validation: all 110 runtime suites pass (37.61 seconds). Actual Code-OSS watcher
consumer passes Node/Mite, alongside native GC/capacity and canonical-path gates.
Production and exact-object offscreen Firefly rebuild; actual ready diagnostic
identifies Electron net as next blocker. Runtime/Firefly generated adapters and
unmodified upstream pin verify; maintained formatting and whitespace checks pass.
After the inline include fix, all 11 focused CommonJS/filesystem/GC/checkout gates
pass (4.79 seconds). API/site generation accepts 295 pages. No visible desktop/physical network test ran.
Fresh native Linux/Windows event/realpath acceptance remains unverified; the previous
hosted macOS Release/Linux runs failed before tests at the now-fixed inline link seam.

Remaining: demanded Electron net behavior and actual main continuation, complete
watcher service/workbench/workspace open/edit/save, native utility/renderer port/
auth handoff, extension/PTY, wider session/media/image/permission acceptance,
webviews/remote and native platform/package/release deliverables. Linux recursive
watching, ignore filters and fs.promises.watch remain open. Full original goal stays active.

## 2026-11-08 — Native desktop net, utility transport and actual Code-OSS requests

- Resume pushed d5de3d9 and actual missing net export. Add maintained net policy over
  real Mite HTTP/verified TLS, not a Node/V8/Chromium host or raw node:http alias.
  Native macOS NetworkState uses passive NWPathMonitor and bounded actual system-
  proxy/PAC/WPAD snapshots. Initial state is explicitly pending; unavailable and
  malformed observations fail rather than inventing offline/proxy-free state.
- Native callbacks retain weak mutex-protected state, no VM/Qt pointer. Stop is
  idempotent. Exact controlled provider tests cover initial/worker/status/late-stop
  behavior and proxy enable/scoped/supplemental/unknown/type/depth/byte limits;
  genuine read-only native query and unavailable-provider gates pass. No network
  settings, permissions or physical transitions are changed/tested.
- Add private ModuleLoader.web_transport capability capturing original Fetch classes,
  stream adapters, cancellation and native HTTP/HTTPS/crypto dependencies before
  application entry. Public global fetch and module-field replacement cannot swap
  the embedding transport. Standalone Headers-only programs retain lazy networking.
  This general embedding hook adds no Firefly/LibWeb dependency to Mite.
- Implement net.fetch with genuine response bodies, metadata/clones/header guards,
  manual per-hop proxy checks, cancellation through body EOF/cancel, 64 exchange
  slots, HTTP(S), decoding and redirects. Strip credentials on any origin change,
  including ports. Electron45 reference actually forwarded explicit Authorization
  in these cases; Firefly intentionally retains stricter stripping. Redirected body
  replay, sessions/cookies/auth cache/shared cache/custom schemes/interception and
  proxy execution remain explicitly unsupported; do not claim defaultSession parity.
- Implement real buffered ClientRequest Writable uploads (16 MiB/64 retained requests),
  headers/write/end/abort, response Readable streams, redirect modes/synchronous
  followRedirect and lifecycle. Match actual Code-OSS agent:null as unspecified;
  reject custom agents/disabled TLS verification. Preserve actual status/headers/
  Set-Cookie; unavailable raw HTTP metadata/progress/chunked uploads fail explicitly.
  Shutdown uses captured internal cleanup, survives public destroy replacement and
  independently retires remaining exchanges/provider even after a throwing hook.
- Share policy with genuine utility roles before Qt/desktop startup. Register only
  owned net export, not main desktop APIs. Explicit parent Electron alias mode uses
  a private utility selector; both selectors reject missing fd3 before VM/GUI setup,
  strip internal args and preserve existing utility payload behavior. Stop JavaScript
  bodies while realm is alive, then retire passive native state. Plain Mite/fork
  children receive no Firefly net capability.
- Exact public main/utility and native Host/factory tests pass owned independent
  HTTP peers: POST/upload bytes, gzip, redirect credentials, pending/header/body abort,
  request callbacks, response pause/resume and public-global/module mutation isolation.
  Real owned TLS rejects selfsigned cert before HTTP. Separately, an optional actual
  HTTPS GET to example.com succeeds with default verification and nonempty body;
  it remains outside default CI, with no trust-store changes or custom CA override.
- Actual unchanged Code-OSS nodeRequest demanded legacy url.parse. Add Node24.15-
  derived maintained parse/Url/prototype format/parseHost, full MIT notices and exact
  upstream URL/query/license hashes. Preserve ports/numeric hosts/relative/auth/query
  semantics instead of wrapping WHATWG URL. New private ICU domain conversion avoids
  IPv4 normalization without changing existing WHATWG/domain helpers. All64 actual
  Node/Mite cases pass, including malformed UTF-16/percent/UTF8: extra tests found and
  fixed real raw-surrogate query differences. Resolve/module-format/deprecation
  behavior remains explicitly scoped. LF checkout rules protect all pinned bytes;
  upstream license whitespace is preserved verbatim, not silently reformatted.
- The next request dependency needed global crypto. Implement real WebCrypto
  getRandomValues/randomUUID using existing native OS uv_random and intrinsic integer
  view validation. Global crypto/Crypto and node:crypto.webcrypto/getRandomValues
  identities/receiver/DOMException/quota/offset/detached/OOB/shared/randomness cases
  pass Node/Mite; stronger shadowed-property validation is tested separately.
  subtle algorithms remain explicitly unsupported. Update two old crypto fixtures
  for the new public surface/host dependencies, preserving entropy rejection/error
  proofs rather than weakening them. New implementation remains standalone LibJS.
- Pin unchanged Code-OSS AgentHostWakeNetworkMonitor and nodeRequest sources/licenses.
  Actual wake/sleep consumer lifecycle passes real native status with32 compiled
  dependencies verified. Actual request consumer passes POST, manual secure redirects,
  decompression, stream adaptation and cancellation over real net.request, with61
  compiled dependencies verified. Neither establishes complete session/proxy/request
  service, extension host or workbench acceptance.
- Add API contracts/ADRs0168/0169 and registered native/policy/release gates. Final actual
  local-desktop/NLS ready diagnostic passes net and next fails linking globalShortcut.
  Its constructor import alone is not a global-key registration; genuine native hotkey
  ownership/callback/cleanup is the next main-startup API task.
- Inspect real hosted d5de3d9 runs: Linux Mite passes all110 suites, proving the earlier
  inline link fix. Style/Pages/dependency jobs pass; macOS Release is still running.
  Windows runtime has6 failures (fork duplex hang, short/long realpath fixture alias,
  event startup/reclamation, exec cleanup, WriteStream reclamation); Windows renderer
  fails our Reporter.cpp's POSIX stat call. Linux renderer fails context-api.html's
  cross-principal borrowed bridge call. Read-only audits identify follow-ups: Windows
  UV_NONBLOCK_PIPE for raw duplex fd3, portable crash path admission, canonical test
  cwd/bounded cleanup and shallow-driver GC diagnostics, child-realm authored bridge
  acceptance/revocation tests. Do not weaken guards or claim these platforms green.

Validation: complete 113-suite runtime regression passes (34.31 seconds). All new
WebCrypto/64 legacy URL cases and6 controlled networking groups pass; exact native
provider/unavailable/Host and registered main/utility loopback gates pass. Production
and exact-object offscreen Firefly rebuild. Both actual pinned Code-OSS net consumers
pass; separate utility lifecycle/parent-exit and all malformed/missing-fd boundary
checks pass. Final main-ready diagnostic records globalShortcut, not a workbench.
Runtime/Firefly generation and unmodified Ladybird pin verify; expanded Windows LF
checkout verification passes. Site accepts299 pages. No visible desktop, permissions,
physical network transitions or system configuration changes occurred.

Remaining: repair observed Windows/Linux renderer/runtime CI and verify fresh hosted
runs/releases; genuine globalShortcut and actual main/workbench/workspace open-edit-
save; transferred utility/renderer endpoints/auth, extension/PTY, session/proxy/cache/
WebCrypto-subtle and wider media/permission/platform/package deliverables. Initial
net is macOS/stateless only; full original product goal remains active.

## 2026-11-08 — Repair observed native CI portability and fixture boundaries

- Resume pushed 0ed30b5 and the observed d5de3d9 hosted failures. Request
  UV_NONBLOCK_PIPE specifically for Windows raw duplex fd3 in Mite fork and Firefly
  utility spawn. libuv's actual Windows implementation otherwise creates a
  synchronous child handle when ipc=false; pending reads can block writes. Retain
  raw framing/ipc=false rather than enabling handle-transfer framing. Add bounded
  child stderr/exit/received/write-callback diagnostics to the fork acceptance.
  Actual Windows duplex confirmation remains pending the next hosted run.
- Replace our Reporter's unavailable-on-Windows Core::System::stat call with a
  portable UTF-8/native-path admission helper. Require fully absolute existing
  directories, preserve directory-symlink policy, reject malformed UTF-8/NUL/limits,
  and translate only generic error conditions to errno. No creation/canonicalization/
  permission changes in admission; final report-store descriptor security is
  unchanged. Pure native owned filesystem probe passes locally. Wire it into
  release validation and Windows renderer CI; Windows capture remains unsupported.
- Canonicalize the realpath runner cwd: the Windows failure was in Node itself,
  comparing ordinary/native paths with short/long Windows aliases. Keep all API
  assertions. Retry only bounded Windows exec directory cleanup after functional
  comparison; killing cmd can leave its already bounded Python descendant holding
  cwd briefly. Do not kill process trees or extend operation timeouts.
- Give the first directory watch the same owner-loop startup turn as the existing
  file/buffer phases; retain target-event assertions and print actual native names
  on failure. Add test-only collectHostGarbageAtIdle: real collection occurs in the
  embedding driver after pump_once unwinds native dispatch frames, then resolves
  the rooted promise. Keep immediate collection during pending writes/live callbacks
  and BOTH final WeakRef reclamation assertions. This diagnoses conservative native
  stack retention without weakening the tests or changing production GC.
- The first full local headless batch passed repaired context-api and exposed the
  same parent/child-principal mistake in jobs. Audit all preload fixtures together:
  positive calls/arguments now originate in srcdoc-authored child wrappers; all11
  iframe load sites wait for about:srcdoc. Separate borrowed-parent negatives remain.
  The same child wrapper must work while live and reject after removal, including
  70-cycle cases. Retain surviving-parent Promise observation and independent page
  behavior. Fix jobs/async-frames/listeners/handlers/node-filters and strengthen
  frame/cycles/process coverage. No production boundary/principal guard is relaxed.
- The final complete actual headless helper batch passes every context, job, timer,
  observer, listener, handler, CustomEvent/ErrorEvent/MessageEvent, traversal and
  revocation gate, including sandbox/resource/source-drift checks. Static author/
  embedded script parsing also passes. No repeated visible desktop/window tests ran;
  unrelated Fontconfig/Metal warnings were not treated as the cause of bridge errors.
- Add ADR0170 and update crash/Windows/current-support documentation. These are
  locally accepted repairs and proposed Windows fixes, not fresh hosted platform
  acceptance. Native platform CI and the macOS package/release outcome still need
  follow-through; actual Code-OSS main startup next requires globalShortcut.

Validation: all 113 standalone runtime suites pass (35.06 seconds). New idle-GC
watcher/WriteStream gates, genuine Node/Mite realpath/exec/event/fork behavior and
owned portable crash-directory probe pass locally. Production/actual renderer
helpers rebuild. The complete sandboxed headless helper batch passes after the
fixture audit. Maintained formatting, upstream pin and whitespace checks pass;
site generation is verified before commit. Ladybird remains unmodified.

Remaining: confirm fresh Windows/Linux CI and macOS packages/releases, investigate
any remaining failures using actual native diagnostics, implement genuine global
shortcuts, then continue workbench/open-edit-save, utility/renderer endpoint/auth,
extension/PTY, broader session/platform/packaging requirements. No VS Code workbench
or native Windows desktop support is claimed. Full original goal remains active.

## 2026-11-08 — Native globalShortcut ownership and Code-OSS continuation

- Implement maintained ESM accelerator/callback ownership with native main-process
  Host and genuine exclusive macOS Carbon hotkeys. Resolve printable characters
  against the selected TIS Unicode keyboard layout, including punctuation layers;
  no US fallback or application-local Qt emulation. Keep Mite independent and
  Ladybird unmodified. Linux/Windows and offscreen admission fail explicitly.
- Bound live/retired handles to64, use monotonic logical/native tokens, reject
  physical duplicate chords and preserve inactive failed-cleanup ownership for retry.
  Queue token-only notifications onto the owner thread with bounded repeat coalescing;
  revoke before unregister, app/VM shutdown and Qt receiver destruction. The latter
  fixes a lifetime gap identified during review and has a concrete regression case.
- Retain failed-release tokens without callback roots. Retry each once before a
  later admission/unregisterAll; block admission on persistent failure and guard
  native-release reentry. Shared policy and exact native Host tests prove transient
  failure recovery without stale callbacks or leaked native handles.
- Six shared Node/Mite policy groups cover aliases, readiness, transaction rollback,
  callback reentry/early events, quotas and disposal. Exact native Host tests pass
  worker events, GC, stale delivery and receiver/controller/VM cleanup, including
  outstanding registration at shutdown. Exact provider with controlled Carbon/TIS
  peers covers selected layouts, exclusive flags, conflicts, reentry and failed cleanup.
- One optional real OS ownership-only probe passes exclusive Ctrl+Alt+Shift+F19
  admission, duplicate refusal, unregister/reacquire and stop/reacquire. No window,
  keyboard injection, AX request or physical keypress acceptance. Public exact-object
  offscreen readiness/query/refusal and windowless Electron baseline pass separately.
- Pin unchanged original GlobalKeybindingsMainService source/license. Its actual
  compiled consumer passes reconciliation, focused/current-command routing, conflicts
  and owned-only lifecycle cleanup through the maintained factory's controlled registry
  constructor seam. All64 compiled dependencies verify; genuine Firefly supplies the
  Electron import graph. No fake Electron host module or actual OS event claim.
- Actual local-desktop/NLS main-ready diagnostic now passes globalShortcut and next
  fails linking Electron dialog. Preserve the default allowServiceWorkers refusal,
  explicit local-desktop restrictions and lack of workbench/open-edit-save acceptance.
  Update API, matrix, current blockers and ADR0171; keep combined menu-accelerator
  example unsupported. Add controlled Host/provider/unavailable checks to release
  validation and Linux renderer CI; the actual OS registration test remains opt-in.

Validation: production Firefly and exact-object offscreen frontend rebuild. All114
runtime gates pass: initial113/114 plus the checkout-byte gate after staging its new
pinned source (the first run correctly rejected a file absent from the Git index).
Native Host/provider/unavailable/public, real ownership-only, Electron baseline and
original Code-OSS consumer pass. Logs: build/global-shortcut-{build,runtime,checkout,
public,native,electron,consumer,ready}.log. Fresh 8b58f5f native Linux runtime run42519989240 subsequently completed all113
suites (156.11 seconds). Windows runtime subsequently ended109/112: fork main/
validation/lifetime/failure passed but the Node orphan-marker fixture was missing;
fs.watch reported a truncated target filename (ts\\initial.txt). HTTP default-errors
produced correct400/431 output in Mite but Node oracle exited3221226505 after400.
Read-only upstream audit matches the watcher filename to libuv1.52.1 Windows
uv__relative_path slicing: an8.3 watched-directory prefix differs from the expanded
long child path. Upstream1.53.0 adds prefix validation and raw-relative fallback.
An owned adapter can normalize the watched path before uv_fs_event_start; do not
fabricate callbacks with basename/suffix stripping. Add real Windows short-alias/
recursive path acceptance and retain original error paths. This remains an inference
from source and diagnostics until native Windows proof.
Job128851195280/run42519989267 has the exact diagnostics. Renderer jobs remained
active and macOS packages pending when queried. Do not infer platform-wide green.

Next dialog audit: actual DialogMainService needs async message/open/save with
optional real BrowserWindow parent, original button indices and cancellation shapes.
Startup main.ts also calls showMessageBoxSync before normal readiness. Reuse owned
QObject lifecycle patterns, not HTML input byte snapshots: privileged dialogs return
absolute files/directories and nonexistent save paths. On macOS use NSOpenPanel/
NSSavePanel for mixed files+folders, multiselect and treatPackageAsDirectory; Qt public
file modes cannot silently narrow those requests. Explicitly reject unsupported
bookmarks/options. Keep sync warnings native/synchronous without recursively pumping
LibJS callbacks. Validate controlled Host/provider first and batch visible acceptance.

Remaining: implement genuine native dialogs and continue the actual main/workbench/
workspace open-edit-save path, transferred utility/renderer endpoints/auth, extension/
PTY and cross-platform/session/package deliverables. Physical shortcut delivery and
live input-source remapping remain unaccepted. Full original product goal stays active.

## 2026-11-08 — Owned main-process dialogs and real main service construction

- Add maintained ESM dialog methods showMessageBox/Sync, showOpenDialog,
  showSaveDialog and pre-ready showErrorBox, with strict optional BrowserWindow
  identity, owned snapshots, original button indices, checkbox/default/cancel
  handling and explicit unsupported options. Keep sync open/save, bookmarks,
  AbortSignal and other unimplemented methods unavailable. Match Electron's normal
  message-box readiness; only showErrorBox bypasses ready.
- Add exact native Host admission, monotonically increasing IDs,16 pending slots,
  bounded UTF-8/JSON snapshots, token-only worker completion and native parent
  revalidation. Parent closure rejects and cancels; stale/duplicate callbacks cannot
  retarget a request. Receiver destruction, app exit and VM shutdown revoke delivery
  before provider teardown. Native errors/results are bounded and malformed data
  fails explicitly. Results must contain absolute paths and consistent cancellation.
- Real Qt message boxes preserve original button mapping, literal/mnemonic labels,
  plain visible detail, checkbox state and default/escape actions. Public AppKit
  NSOpenPanel/NSSavePanel implement files/folders/multiple selection, package
  traversal, filters, defaults and nonexistent save destinations. Do not reuse HTML
  file-input byte snapshots or expose privileged paths to renderer globals. Other
  platforms' file providers are explicitly unavailable; offscreen UI is refused.
- Bound native operations process-wide, including canceled-but-not-yet-closed panels
  across provider lifetimes; only actual native completion frees its ticket. Reentrant
  stop/admission and failed startup never fabricate success or lose charged ownership.
  Exact controlled-provider tests pass failure/parent/duplicate/reentry/quota cases.
- Defer all bridge JS dispatch and Mite pumping throughout native synchronous modal
  execution, including initial-entry showErrorBox. Also defer same-Host completions
  until its synchronous provider returns. Exact Host test processes nested Qt events
  during sync, proving no recursive JS sink; it also covers real GC, worker events,
  parent destruction after queued success, capacity and outstanding VM shutdown.
  Fix the test driver's Qt liveness: continue while native Host requests remain, not
  just while libuv is alive; retain a15-second failure deadline and intentional
  outstanding-shutdown case. No production ownership assertion is weakened.
- Six actual Node/Mite policy groups pass options/overloads/budgets/absolute returned
  paths, early completion rollback, failed cancellation and shutdown/parent reentry.
  Public exact-object offscreen readiness/startup-error/refusal passes without UI.
  Unchanged pinned Code-OSS massageMessageBoxOptions passes with32 hashed compiled
  dependencies. Keep complete DialogMainService unaccepted; no fake Electron module.
- Actual local-desktop/NLS entry now passes dialog imports and constructs real main
  services. It next fails GPUProcessMainService refreshFeatureStatus calling missing
  app.getGPUFeatureStatus. This is beyond the prior static-export failure and still
  not a workbench/open-edit-save acceptance. Default worker refusal and explicit
  local-desktop restrictions remain. Add API contract/ADR0172/current-blocker updates.

Validation: production Firefly/exact-object offscreen build; exact Host (normal and
outstanding-shutdown), controlled native-provider and public facade pass. All115
runtime suites pass locally (27.73 seconds final run); tightened final factory cases also pass
focused Node/Mite policy. Original Code-OSS option consumer passes; exact offscreen message-widget gate passes. Source formatting,
generated inputs and unmodified upstream verify. Logs: build/dialog-{runtime,policy,
build-final,provider-build,public,consumer,ready,style,site,widget-final,final-verification,native-ui}.log.

Native UI acceptance: run one opt-in Cocoa batch only. It failed the first async
message-result assertion and stopped immediately; no subsequent open/save panel ran.
The generic first result log lacks actual values. Exact Qt6.11.2 source shows Cocoa
uses NSAlert and copies its suppression checkbox back into the Qt backing widget;
the probe only toggled/clicked that backing widget, so it may not have operated
the actual native alert correctly. This is source evidence for a probe defect, not proof that
the provider is correct or broken. Keep all real native UI/selection/save acceptance
unverified and do not repeatedly relaunch visible tests. The optional future probe
must operate actual public NSAlert/NSButton controls while retaining original native
callbacks and deadlines. Headless exact message-widget checks are a separate gate. That new gate initially
hung because its title-based selector could never match: exact Qt6.11.2
QMessageBox::setWindowTitle is intentionally a no-op under Q_OS_MAC. The timeout
fallback used the same invalid selector. Sampling the owned process proved a Unix
Qt event loop, not an active Cocoa helper. Fix fixture identity using unique owned
message text; assert actual buttons/checkbox/plain text/closure, not window titles.
Disable native dialog helpers before QApplication in this controlled widget gate and
wrap it in a10-second child-process deadline. The complete offscreen widget gate now
passes every original index, checkbox, cancellation, parent destruction and sync
exec case. Retain the real native UI failure as unaccepted; do not substitute this
headless success. Future Cocoa probe now captures actual NSAlert public controls,
avoids title matching and prints returned values, but is only compiled, not rerun.

Next GPU audit: pinned GPUProcessMainService only needs synchronous
app.getGPUFeatureStatus plus gpu-info-update and child-process-gone(type GPU);
getGPUInfo is not used by this pinned source graph. Missing gpu_compositing is
accepted as unknown/unavailable. Do not infer disabled GPU from absent compositor
device identity: upstream only fills it for Vulkan, while Metal may be active.
ForceCPUPainting in actual web_content_options is a genuine known CPU policy;
default GPU status needs owned telemetry or an honest unknown state. The existing
protected Application::process_did_exit override can observe real Compositor PID/
wait status in our subclass, retaining base handling; a disconnect alone does not
prove a crash. No GPU API is implemented by this audit.

Remaining: complete batched native dialog UI acceptance and record its precise scope;
then genuine GPU capability reporting and actual main/workbench/workspace open-edit-
save. Native utility/renderer endpoint/auth handoff, extension/PTY, workers/webviews,
Windows watcher/Node-oracle CI failures and platform/release deliverables remain.
Latest5dfd16e source/Pages/dependency gates are green; native runs were still active
or queued when queried. Older8b58f5f Linux renderer job128851164651 failed link on missing
Firefly::defer_theme_settings/activate_theme_settings (theme implementations only
selected underAPPLE). Older d5de3d9 macOS release job128832215863 passed menu policy
but failed native.mjs:114 quit-role dispatch; alpha publication was skipped. Preserve
real theme/native-menu semantics while repairing these gates; do not add no-op export
stubs or weaken assertions. No platform/release success is inferred. Full original goal stays active.

## 2026-11-08 — Restore portable theme startup and deterministic menu dispatch gate

- Repair the observed Linux link failure by moving actual deferred theme preference
  lifecycle into platform-neutral ThemeSettings.cpp/.h, compiled for every desktop
  runtime. It lazily resolves real WebView profile settings, applies interface and
  website color preference together, preserves unrelated fields and resets each
  launch toSystem. Mac themeSetSource delegates to that shared state. Linux still
  explicitly lacks nativeTheme/accessibility APIs; no no-op stubs or invented support.
- Add a pure native probe backed by genuine isolated WebView::Settings and persisted
  JSON: no early profile access, last pending selection wins, live changes, launch
  reset and independent-field preservation/reload pass. Link actual AK/Core/URL
  requirements explicitly and wire the gate into release and Linux renderer CI.
- Repair the macOS release menu gate's fixed-time assumption: require actual QAction
  lookup, wait with a bounded deadline for quits, retain exact quits===1, and report
  generation/command/trigger/signal/emission/delivery/pending counts on failure. Keep
  pending queued callbacks alive in the embedding driver. Set NoRole before menu
  insertion so Cocoa cannot infer a role before our managed role dispatch applies.
- Add an exact-source offscreen frontend for the menu probe. Its explicit --offscreen
  branch verifies actual offscreen platform/non-native bar; the existing default
  macOS native-menu assertion is unchanged and still requires hosted Cocoa acceptance.
  This allows local queued-role/lifetime testing without desktop menu/UI interaction.

Validation: real theme-settings lifecycle probe and complete menu QAction/LibJS
callback/role/state/stale-generation gate pass locally offscreen. Production Firefly
rebuilds; upstream remains unmodified. Logs: build/theme-menu-repair.log. The first
new menu build caught Qt's reserved signals macro in a diagnostic variable; rename
it native_signals. The first offscreen execution correctly rejected the default
native-menu claim; add a separate explicit offscreen contract rather than relaxing
that native assertion. No visible windows or system preference changes were made.

Remaining: fresh Linux link/runtime and hosted macOS native-menu/release validation;
continue genuine GPU policy reporting and real Code-OSS main/workbench progress.
No Windows desktop, alpha publication or usable Code-OSS claim. Full goal active.

## 2026-11-08 — Native-confirmed GPU policy and genuine Compositor lifecycle

- Add maintained ESM app.getGPUFeatureStatus and early disableHardwareAcceleration,
  with a private LibJS host backed by a small native policy. Read actual upstream
  web_content_options.force_cpu_painting after initialization. CPU requests alone do
  not claim observation; startup fails if native initialization ignores the request.
  Seal before profile setup, reject late changes, keep launch-only commandLine
  behavior and clear host knowledge on teardown. Mite remains desktop-independent.
- Report only confirmed gpu_compositing/rasterization disabled_software under
  selected CPU painting; pre-init and default-GPU snapshots remain explicitly{}.
  Fresh objects cannot alter native state; malformed/unconfirmed enabled fields fail.
  Publish one genuine unknown-to-known CPU policy info update after ready, none for
  unchanged/unknown state. getGPUInfo basic/complete explicitly returns a rejected Promise;
  no driver/device/Graphite/WebGL detection or fake Chromium success values.
- Override the existing protected process_did_exit in our native Application and
  preserve the upstream base handler. Forward actual Compositor PID, service role
  and POSIX wait-status classification through the existing owner-thread bridge.
  SIGKILL means killed, not OOM; missing status is unknown, not a crash. Normalize
  signal codes as128+signal and document the limited role/reason differences.
  Retain confirmed policy after child exit, matching the consumer's expected app
  snapshot behavior. Do not fabricate recovery info from cached device identity.
- Six actual Node/Mite policy groups pass known/unknown/lifecycle/malformed/error
  cases. Pure native policy tests prove real sealing and actual owned-child normal/
  abnormal/SIGKILL wait statuses, with synthetic fatal-signal classification separate.
  Genuine public offscreen early CPU/default unknown modes pass fresh snapshots,
  mutation isolation, late rejection and actual info-update count/no invented update.
- Pin unchanged original GPUProcessMainService source. Its genuine Firefly app-backed
  consumer passes native snapshot/copy/deduplicated refresh and listener disposal,
  with20 compiled dependencies verified. Clearly labelled synthetic app events test
  only that consumer's exit/cache logic; they are not native crash acceptance.
- Real offscreen acceptance creates one hidden local page, enumerates direct owned
  Compositor PID/executable/CPU args twice, sends only that PID SIGKILL and verifies
  actual app GPU PID/killed/137 event, exact-once delivery, retained CPU policy and
  native replacement carrying the same CPU arguments. No desktop/input/GPU-device
  claim. Initial fixture hit missing Mite process.kill before signalling anything;
  use existing real execFile('/bin/kill',['-9',validatedOwnedPid]) instead. Do not
  imply global process.kill support. Temp profile/process-group cleanup is bounded.
- Actual local-desktop/NLS main now constructs GPUProcessMainService and next stops
  in ProtocolMainService calling defaultSession.protocol.handle. This is genuine
  service-construction progress, not workbench/open-edit-save acceptance. Update
  API/compatibility/current blockers and ADR0173, keeping upstream unmodified.

Validation: all116 runtime suites pass (27.04 seconds). Production Firefly/exact
application offscreen frontend build; native state, genuine public modes, original
Code-OSS consumer and real owned Compositor exit/replacement gates pass. New GPU
state/public/process checks are wired into release validation; Linux CI adds native
state classification. Logs: build/gpu-{runtime,final-build,acceptance,consumer,process,
ready}.log. Fresh b3f4a67 source/Pages gates green, native renderer jobs active and
macOS release pending when queried. No fresh full platform/release success inferred.

Next protocol audit: unchanged ProtocolMainService keeps authorized legacy
registerFileProtocol('vscode-file') and headers, then handle('file',()=>Response.error())
SPECIFICALLY TO DENY ordinary file access; cleanup uses unhandle. Later managed-remote
handlers return asynchronous binary Response with MIME/status,404 invalid window and
500 remote failures. Never alias handle to a file-path callback or register builtin
file as a privileged custom origin. Existing transports only represent GET/HEAD,
fixed200 regular files, known length and authenticated custom schemes. Implement a
separate modern handler registry/generations/collisions, real Request/Response,
network-error termination and builtin-file interception BEFORE native file access.
Generalize bounded response metadata/chunks/EOF/credit and reader+Request.signal
cancellation while preserving legacy descriptor routes, CSP/CORS and native document/
navigation/generation authentication. Navigation/unregister/timeout/process/app death
must cancel late bodies. Integrate main net.fetch default-session dispatch/bypass
without silently changing standalone Mite fetch. Prioritize real file-deny plus
legacy-vscode-file navigation, original remote404/500/binary/MIME and actual streamed
credit/HEAD/abort/stale/GC acceptance. No whole-body/base64 IPC shortcut.

Remaining: genuine protocol.handle response/file-deny transport, actual main/workbench/
workspace open-edit-save and utility/renderer endpoints/auth, extension/PTY, workers,
Windows watcher/oracle CI repairs, native dialog acceptance and platform/releases.
Physical GPU/backend telemetry and broader process notifications remain unaccepted.
Full original goal stays active.


## 2026-11-08 — Credit-paced modern response transport foundation

- Preserve legacy file-response framing and the existing public file callback API.
  Add separate exact stream metadata with status 200..599, ordered byte-valued headers,
  optional length, HEAD/null-body rules, framing/header/size budgets and strict
  integral numeric fields. No path or native descriptor enters renderer metadata.
- Extend the real native resource server with asynchronous producers. Keep process
  admission leases through body completion; require actual consumer credit, at most
  one outstanding pull, and check the original pull maximum as well as later credit.
  Stage returned chunks behind transport backpressure; verify declared length/EOF,
  unknown total-size limit and stale per-pull generations. Detach state before
  reentrant cancellation; cancel unread HEAD/null-body producers without pulling.
- Adapt actual LibWeb PendingResponse/body channels to modern and unchanged legacy
  schemas. Preserve CORS and upstream forbidden-header filtering. Unknown lengths
  remain unknown; enforce received offsets and native navigation/body size budgets.
  Modern status phrases are not carried by this internal transport.
- Add a private maintained ESM response controller using captured genuine Request,
  Response and stream primitives. Demand-only reads, bounded copied chunks, owned
  AbortController, late-body cancellation, positive generations,64 pending responses,
  64-header/32-KiB metadata/64-GiB body limits and explicit 1-MiB source-chunk refusal.
  Keep this internal: no new public protocol methods or standalone Mite fetch change.
- Add actual native producer/metadata and Node/Mite controller gates. Extend genuine
  two-process JavaScript fetch acceptance with a 4-MiB binary 404 unknown-length body,
  HEAD and denial; extend real LibWeb tests with 404/500, null statuses, GC, length
  mismatch and unknown-length budget refusal. No visible desktop test is required.

Validation: all 117 runtime suites pass (20.76 seconds). Production Firefly builds;
all 14 new native producer modes, metadata schema, actual LibWeb responses, modern/
legacy JavaScript process-channel, native navigation and existing 23-mode file-server
regression pass. Whole-source formatting passes (254 Python, 859 JS/web/config,
484 native and 64 CMake files). Site build and 307 pages/21,729 local link checks
pass. Logs: build/protocol-stream-{runtime,final-build,acceptance,server-regression,
style,site}.log. No visible GUI tests. Fresh hosted platform acceptance remains open.
Initial quota fixture incorrectly expected a ninth per-request refusal; correct it
to the existing whole-channel revocation contract, keeping eight native admissions,
zero body pulls and eight producer cancellations. No production quota relaxation.
First compile caught a move-only stream copy and was corrected.

Remaining: connect the public default-session handler registry and VM/native body
bindings; enforce builtin-file interception for all authenticated owned page and
navigation scopes BEFORE native file access. Main net.fetch dispatch/bypass, public
handler/remote consumer acceptance, actual Code-OSS workbench/open-edit-save, platform
and release requirements stay outstanding. Code-OSS still stops at protocol.handle;
this transport foundation does not claim a working editor. See ADR 0174.
Full original goal active.


## 2026-11-08 — Public modern protocol handlers and genuine file interception

- Connect handle/unhandle to the genuine Request/Response controller, native route
  generations, asynchronous response heads and credit-paced Uint8Array chunks. Share
  legacy/modern collisions and default-session identity. Retain native opaque token
  strings; private numeric IDs never narrow u64 identities. Keep legacy descriptor
  routes and standalone Mite fetch unchanged. Main net.fetch selects handlers or the
  HTTP transport per redirect, supports bypass, abort and teardown; utility requests
  acquire no default-session capability. Strip credentials across custom hosts even
  when WHATWG URL reports both origins as null.
- Add native file policy generations to trusted startup/control messages. New helpers
  install policy before pages; late registration updates existing owned helpers and
  refreshes actual canonical document/page/URL/settings bindings. Active interception
  fails closed without a grant. Removal restores upstream file behavior. Updates are
  ordered before later main-issued navigation; already initiated renderer work is
  not atomically rolled back. Default unconfigured upstream helpers receive no new
  private argument. File never becomes a privileged custom origin.
- Intercept before native file loading and skip the main load binding's stat when an
  actual file response handler owns that URL. A narrow pinned Fetch seam handles
  authenticated file-Window programmatic CORS GET/HEAD after CSP/mixed-content checks
  but before non-HTTP CORS refusal. It supplies handler responses, never filesystem
  fallback; other origins, frames/workers and unsupported modes retain restrictions.
  Keep FileModules, URL origin classification and pinned Ladybird sources unchanged.
- Owner-close checks drop queued requests/pulls and replies immediately rather than
  waiting for the periodic sweep. Post-await guards prevent publishing resolved
  responses/chunks after abort, unhandle or shutdown. Four actual-controller race
  cases fail against the previous source and pass after correction. Strict native
  Host/policy and Node/Mite registry/network tests cover bounds, lifecycle and GC.
- Genuine public acceptance uses one hidden window and exact production application
  objects with the static offscreen frontend: real HTML, streamed 4-MiB binary responses with status 404,
  HEAD/500/header filtering, network-error file denial, main redirects/bypass,
  cancellation/fresh generations, legacy routes, native file restoration, late file
  interception, own file HTML/binary responses with status 201 responses, CSP denial BEFORE handler delivery,
  and exact-once pending-body/request-signal cancellation when the window closes.
  No visible desktop/UI test was performed. Native Cocoa/Linux/Windows acceptance is
  not inferred from this offscreen component acceptance.
- Pin unchanged original ProtocolMainService and managedRemoteResourceProtocol with
  licenses and source hashes. The real service passes actual main file denial,
  remote invalid-authority 404/failure 500/base64 binary/MIME 200, IPC registration and
  cleanup using genuine protocol/session/net, with 49 compiled dependencies verified.
  Environment/log/remote peers are controlled collaborators, not a real remote service.
- Re-run real local-desktop/NLS startup with an existing isolated user-data directory.
  It passes ProtocolMainService and next fails importing @vscode/policy-watcher:
  bindings.getFileName requires V8 Error.captureStackTrace before its .node/N-API addon.
  No workbench or workspace open/edit/save acceptance. Refresh API/session/network,
  compatibility and adaptation docs plus ADR 0175 to state this actual boundary.

Validation: all 118 runtime suites pass (18.45 seconds). Production Firefly and exact
object offscreen frontend build. Native policy/Host, modern/legacy public, original
consumer, export/readiness and complete native file/fetch/navigation regression gates
pass. Source formatting passes (256 Python/865 JS-web-config/488 native/64 CMake);
site 308 pages/21,797 local links pass. Logs: build/protocol-handlers-{runtime-final,final-build,public,legacy,consumer,
exports,regression-final,style,site,code-oss}.log. Earlier runtime line-ending test
needed the newly pinned vendor inputs staged before its checkout-index verification;
that gate then passes. Initial native compilation caught exit-time mutex destruction,
borrowed temporary view and an accidental unregister declaration replacement; fix all.
Initial main export fixture intentionally required handle to be absent; update it to
assert actual modern/legacy singleton/readiness and registration/removal. Initial
public fixture used unavailable executeJavaScript; replace it with actual page scripts.
Actual late file test exposed upstream's pre-scheme CORS refusal, leading to the
narrow owned interception seam above; the positive file/CSP fixture exposed the main
stat precheck, which now correctly defers to the registered response handler.

Next native policy audit (read-only, not implemented): installed pinned
@vscode/policy-watcher 1.7 creates a native watcher; stack-helper emulation alone would
only expose the unsupported Node addon. Implement an explicitly owned package
adapter backed by real native services, not empty policy data or disabled enforcement.
Code-OSS constructs NativePolicyService plus NativeManagedSettingsService on macOS;
both await the first asynchronous real snapshot. Upstream uses CFPreferencesCopyAppValue
and a serial FSEventStream of /Library/Managed Preferences; scalar policies query
exact CFString/CFBoolean/CFNumber types, while union definitions additionally require
CFPreferencesAppValueIsForced. Preserve pinned semantics explicitly. Queue bounded
plain snapshots onto Mite's owner event loop, never touch VM roots on native workers,
and revoke epochs/callbacks before disposal. Verify both unchanged consumers and
real read-only provider initialization before claiming further startup progress.
Windows needs a real registry/provider and Linux currently fails explicitly upstream.

Remaining: native policy integration, actual main/workbench/open-edit-save, native
utility/renderer port handoff/authentication, extension host/PTY/workers, Windows
watcher/oracle CI repairs, native dialog acceptance and platform/packaging/releases.
Full original goal remains active.


## 2026-11-09 — Real native policy extension and explicit Code-OSS package adapter

- Add the standalone Mite extension mite:policy-watcher with shared ESM/CommonJS
  exports, maintained ESM validation/diffs/disposal and a private owner-thread Host.
  It does not claim a Node builtin, N-API, V8 or Firefly/LibWeb dependency. The
  canonical namespaced resolver alias reaches an existing realm-local trusted host
  module. No policy observer starts merely by initializing or importing Mite.
- Implement a real macOS CFPreferences/FSEvents provider. Preserve pinned package
  1.7.0 semantics: exact CF scalar types; union definitions additionally require
  forced preferences; successful signed-long-long conversion becomes JS Number;
  UTF-8 C-string conversion matches the original. The serial native worker owns
  plain bounded snapshots only, never VM roots or application callbacks. First
  observed empty data remains distinct from no completed observation. Latest-state
  coalescing cannot overwrite a latched native failure. Windows/Linux provider
  creation explicitly returns unavailable, never a successful empty policy set.
- Owner libuv timers consume completed snapshots and dispatch on Mite's VM thread.
  Revoke callback roots before stopping/draining the provider and uv_close storage.
  Stop is idempotent and handles worker/owner reentry; repeated owner disposal drains
  any already initiated worker stop. Follow Apple Stop/Invalidate/Release ordering;
  never call Stop after a failed Start. Validate native inputs independently,
  including strict Unicode before conversion, declared snapshot keys/types and
  count/byte bounds. At most 16 watchers including closing handles, 256 definitions,
  4096-byte names, 64 KiB strings and 1 MiB definition/snapshot JSON. Normal errors fail
  explicitly; native read/data failure retires the watcher through uncaught-error
  reporting. Actual OS MDM change delivery remains unaccepted.
- Pin original policy-watcher 1.7.0 entry/types/package/license and both unchanged
  Code-OSS policy service sources, keeping notices intact and adding LF integrity
  coverage for Windows checkout. New compile --mite-policy-watcher is opt-in,
  requires verified prepared dependencies and modifies only a fresh compiled copy's
  exact audited package entry. Mite uses its real extension; ordinary Node/Electron
  retain the original addon branch. Record package, original/output and owned
  adapter hashes; refuse drift or versions other than 1.7.0. No original dependency
  tree/application/Ladybird changes, native-addon emulation or fake initial callback.
- Both unchanged NativePolicyService and NativeManagedSettingsService initialize,
  replace definitions and dispose against a unique unused actual preferences domain,
  with 46 compiled dependencies and exact conditional adapter provenance verified.
  No preference writes, injected empty snapshots or visible GUI tests. The public
  extension separately passes singleton resolution and actual asynchronous read-only
  initialization. Native provider controlled peers verify CF types, scalar/forced-
  union behavior, changes/removals/coalescing, startup failure, quotas/error latch and
  drained/reentrant stop. Nine exact-Host subprocesses verify real owner timers,
  malformed data/read/callback failures, GC, closing quotas, reentry and teardown
  before VM destruction. Controlled peers are distinct from real OS observations.
- Build a fresh diagnostic tree at build/code-oss-policy-final using the explicit
  adapter/NLS/local-desktop overlay. The actual main-ready startup now reaches
  ipc.net.serve during main.claimInstance and explicitly fails because node:net has
  no local Unix socket/named-pipe transport. Update API/compatibility/adaptation docs
  and ADR 0176. Keep generic feature pages linked to current startup status instead
  of repeating volatile first-error messages. No workbench/open-edit-save claim.

Validation: all 124 runtime suites pass (19.39 seconds), including native provider,
public read-only extension, controlled factory/adapter and exact Host gates. Real
Code-OSS policy consumers pass; production Firefly and exact-object offscreen app
build and observe the next real startup failure. Mite linkage has system frameworks
only, with no Node/V8/Qt/LibWeb. Formatting passes for 260 Python, 872 JS/web/config, 497 native and
64 CMake files. The site validates 310 pages and 21,947 links/resources/fragments. Logs: build/policy-watcher-{runtime-final,host,provider,public,consumer,
release-build,firefly-final-build,compile,code-oss,style,site}.log. No visible UI or
preference mutation. Fresh hosted Linux/Windows build and macOS package acceptance
remain separate requirements.

Corrections during validation: strict native string conversion needs explicit
AllowLonelySurrogates::No (the default accepts lonely surrogates); malformed native
input must be rejected before creating even a controlled provider. Native Host
probe noncapturing lambdas need explicit function-pointer selection. Apple permits
FSEventStreamStop only after successful Start; track that separately from stream
allocation. The initial snapshot-error queue could overwrite a read failure with a
later success; latch terminal failure and test that notification race. Preserve
exact native provider/root teardown through all failures. Bound union-array length
before reading elements; ignore custom iterators like the pinned native array
reader. Regression checks cover oversized getters and hostile iterators. Test sources now share
mite/tests/policy-watcher rather than two adjacent policy test folders.

Next slice audit (read-only): implement actual uv_pipe endpoints sharing existing
TCP/Duplex lifetime and pressure behavior, not a TCP substitute. The pinned libuv
has pipe_bind2/connect2 with NO_TRUNCATE; preserve path errors/ownership and avoid
DNS/TCP handling for local paths. Code-OSS requires serve(path), connect(path),
EADDRINUSE conflict, ENOENT and stale ECONNREFUSED handling; the application owns
stale unlink/retry, never delete a preexisting endpoint automatically in Mite.
Accept the unchanged ipc.net consumer using real separate OS processes and binary
payloads, plus socket lifetime/GC/error cases. Native Windows named-pipe execution
needs its own fresh CI evidence; no compile-only Windows claim.

Remaining: local IPC transport, actual main/workbench/open-edit-save, native
utility/renderer port transfer/authentication, extension host/PTY/workers, Windows
watcher/oracle CI repair, native dialog/platform/packaging/releases and real policy
change acceptance/providers. Full original goal remains active.


## 2026-11-09 — Native local streams unblock the Code-OSS instance claim

- Extend the existing standalone native network owner with typed TCP/pipe handles.
  Real uv_pipe_init(ipc=0), bind2/connect2 with NO_TRUNCATE and inherited accepted
  transport kind share byte ownership, reads/writes/shutdown, flow control, ref/unref
  and uv_close completion. Paths retain strict UTF-8 storage through connect; no
  manual unlink, TCP substitution, descriptor transfer or Ladybird modification.
- Preserve normal final close callbacks while revoking socket callbacks during VM
  shutdown, draining native requests before releasing storage. Limit shared active
  and closing handles to 4096, reject exhausted IDs/admission with native ENOSPC
  instead of a fatal assertion, and preserve code/errno for TCP and pipe admission.
  Encoded path quota is 32 KiB; shorter platform limits fail without truncation.
- Maintained ESM supports path/string/options overloads and negative numeric pipe
  names, bypasses DNS/TCP attempt events, retains shared cancellation/stream state,
  and keeps accepted pipe identity. Socket address is {}, IP properties undefined;
  Server.address retains the supplied path after close and failed binds like Node.
  TCP-only tuning is a no-op on pipes. Listener port/path precedence follows the
  Node reference independently of client path precedence. Empty/NUL/malformed UTF-16 paths reject;
  abstract names and readableAll/writableAll remain explicitly unsupported.
- Add genuine Node/Mite OS-endpoint differential gates: both overloads transfer
  3 MiB binary data over Unicode/space-containing paths with half-close and
  cork/drain pressure, existing listener and
  regular-file collisions preserve ownership, missing/stale/oversized endpoints
  preserve ENOENT/ECONNREFUSED/EADDRINUSE/EINVAL and do not truncate/unlink. Close
  waits for accepted peers and unref exits naturally. Pre/immediate listen/connect
  abort and established binary connection abort preserve cause, cancel events and
  release owned endpoints/peers and waiting close. Node transient pre-abort path
  creation and socket-state quirks are not overclaimed. Independent Python AF_UNIX
  clients exercise both runtimes. A native Win32 independent-client branch exists
  but is not evidence of Windows execution. Mite policy verifies malformed paths,
  unsupported permission options, 4096 admission/closing quota and post-close reuse.
- The unchanged pinned Code-OSS ipc.net input and 41 compiled dependencies are
  verified before real separate-process channel execution. Mite/Mite, Mite/Node
  and Node/Mite exchange VSBuffer payloads above 256 KiB, client identity, a second
  framed request/reply and disposal; no controlled transport or protocol substitute.
- Rebuild production Firefly and exact-object offscreen app. The actual adapted
  local-desktop/NLS/policy main-ready graph now passes ipc.net.serve/main.claimInstance
  and reaches CodeApplication.configureSession. Its first current failure is missing
  session.defaultSession.setPermissionRequestHandler; the error-reporting path then
  also calls unsupported Error.captureStackTrace. Entry import and app ready remain
  observed, not a workbench or open/edit/save pass. All desktop probes are offscreen
  with isolated profiles; no visible GUI test or user preference mutation.
- Update API, compatibility/adaptation docs and ADR 0177. Keep Windows/Linux pipe
  execution, real policy changes/providers, renderer/utility endpoint transfer and
  complete editor workflows explicitly unaccepted.

Validation: Release CLI/embedded host and both Firefly targets build. All 125 runtime
CTest suites pass (38.08 seconds), including existing TCP/HTTP/TLS consumers and the
new pipe gate; the final bounded-admission policy and original IPC consumer also
pass separately. Formatting passes for 261 Python, 883 JS/web/config, 497 native and
64 CMake files. The generated site passes 311 pages and 22,022 local links/resources.
Logs: build/local-ipc-{release-final-build,renderer-final-build,runtime-final,pipes-final,
consumer-final,code-oss-final,style,site}.log.

Next audit: LibWebCommon's UI permission prompt carries a top-level origin but
omits the authenticated requesting document identity; the earlier
WebContentPage.did_request_permission route still has that identity and upstream
trust/permissions-policy/user-activation checks. Cached PermissionManager decisions
can bypass the prompt, so request-only callback wiring cannot enforce Electron's
check handler. Build an owned source adapter at the authoritative document boundary
and preserve OS microphone/notification authorization. Clipboard has separate
activation/native transport gates; pointer lock, camera/local fonts and display
media have distinct absent capabilities at this pin. Implement genuine session
request/check enforcement tied to owned renderer/session/document/origin identities,
then re-run the actual pinned startup. Do not merely store inert
handlers to skip the security boundary. Remaining full-goal requirements still
include workbench/open-edit-save, extension host/PTY/workers, native port transfer,
platform/packaging/release acceptance and the previously recorded Windows CI repairs.
The original goal remains active.



## 2026-11-09 — Document-bound session permission checks and requests

- Implement maintained ESM Session.setPermissionCheckHandler and
  setPermissionRequestHandler on the frozen default session; owned
  webContents.session returns the same real singleton. Preserve ready/closed/type
  guards, synchronous strict-boolean checks and asynchronous single-use decisions.
  Default/null reset denies. Replacing either handler revokes pending ownership,
  intentionally stricter than Electron's non-null request-handler replacement.
- Reserve generations before native registration. Controlled reentrant stop during
  the first registration exposed stale enablement; now stop revokes the reservation,
  nested registration rejects and failed registration invalidates candidate closures.
  Invalid decisions deny before throwing; request exceptions deny before ordinary
  callback failure reporting. Native check exceptions now report/fail rather than
  being silently swallowed. An inactive-document completion now sends denial to
  its original request ID while the page remains open, draining the helper Page
  pending-callback root; dropping that reply would retain old settings/documents.
  This follows the inspected Page set/take lifecycle; no GC-stress claim. Media callback(true) denies and fails explicitly.
- Add native canonical-document permission ownership, independent of preload/file
  grants. Synchronous private IPC authenticates the sending client/page and active
  document namespace/local ID plus policy generation. Derive current frame URL,
  origin, embedding origin and main-frame identity natively. Deny busy/modal VM
  reentry and revalidate after JS checkpoints, including callback-driven close/nav.
- Reuse the existing native permission-request transport after original canonical,
  Permissions Policy, trust and notification-activation guards. Pending requests
  retain native page/document/epoch/deadline; retire before completion and deny stale
  owner or generation. Queued events revalidate before each delivery/defer. Grant
  records are exact-document, never origin-wide; close/nav/config/shutdown revoke,
  and frame-only replacement prunes stale records. Native feature-use gates consult
  this authority or a fresh check, with microphone query/use/capture always denied.
- Bound native ownership to 16 pending/window, 64 global and 256 document grants;
  31-bit generations/tokens never wrap. Enforce 64 KiB UTF-8 URL/origin fields and
  256 KiB metadata before callback/admission/queue allocation. Completion checks the
  30-second monotonic deadline itself, so a retained callback cannot grant merely
  because a long JS turn delayed the expiry timer.
- Compose pinned query-before-cache and independent request admission paths; custom
  opaque-origin outcomes never persist in PermissionStore. Notification request
  settlement uses its actual decision instead of re-querying the independent check
  handler; activation/delivery remains unaccepted. Four guarded upstream source
  compositions preserve unrelated bytes and existing IPC/navigation hooks. No
  Ladybird source/gitlink/manifest changes. Original IPC endpoint IDs are preserved.
- Enter owned WebContent denial mode before application content, send the captured
  policy in a guarded private helper-launch argument and ordered authenticated
  updates, and update existing out-of-process frame hosts. Helper queries validate
  their actual executing PageClient against their live authenticated connection;
  missing/stale policy fails closed. Separate out-of-process worker queries remain
  unintegrated and can retain upstream default status; no worker support claim.
- Real public offscreen acceptance passes main/same/cross-origin query ownership,
  actual geolocation request-handler denial, handler replacement/null reset,
  same-origin contents isolation and held decisions after navigation/owner close/
  replacement/reset. No location result, notification, capture, device API or OS
  permission prompt. The shared first query/request-denial path passes Electron
  45.2.3. The first close fixture inspected destruction too soon; it now waits for
  the real closed event and destroyed state before invoking the stale callback.
- A separate native-only frontend links exact production application objects and
  uses a real owned file document. Twelve cases check current/stale/forged native
  identities, genuine broker-to-ESM grant, wrong-document denial and replacement
  revocation. This is controlled native grant-model acceptance, not renderer device
  access. It adds no production JS probe API or fake document/provider.
- Re-run the actual prepared Code-OSS entry with local-desktop/NLS/policy adaptation.
  Entry/app ready and both permission registrations now pass. First actual failure
  is session.defaultSession.setDisplayMediaRequestHandler; Error.captureStackTrace
  remains a secondary error-reporting gap. No workbench/open-edit-save claim.
- Update API/session/Code-OSS docs and ADR 0178, including scoped defaults and
  unsupported clipboard/media/worker routes. Add both real component targets to the
  macOS release gate; fresh hosted execution remains required. Existing unchanged
  ProtocolMainService consumer still passes with 49 verified compiled dependencies.

Validation: Release CLI/embedded host, production Firefly, exact-object offscreen
frontend and native boundary frontend build. All 126 CTest suites pass (36.09s).
Final check-firefly-session-permissions and check-firefly-permission-boundary pass
against the hardened build. Node/Mite factory policy, source pins/reversal/drift,
startup policy guard and unchanged upstream checks pass. Formatting passes 266 Python, 890 JS/web/config, 507 native and 66 CMake files.
The generated site validates 312 pages and 22,098 local links/resources. Logs: build/session-permissions-{final-build,
acceptance-final,policy,electron,code-oss,protocol-regression,runtime-final,adapters,
upstream,style,site}.log.

Next: the pinned engine lacks getDisplayMedia; audit an explicit optional capture
feature deferral in the existing local-desktop experiment, preserving ordinary
Electron behavior and rejecting actual unsupported capture. Do not add an inert
Session setter. The subsequent unchanged configureSession registers security-
relevant webRequest filters; implement actual authenticated request/frame policy
before accepting those. Then continue actual main startup and workbench workflows.
Remaining: renderer notification activation/delivery and elapsed native deadlines,
full device/worker permissions, native utility/renderer port transfer, extension
host/PTY/workers, Windows watcher/oracle repairs, platform/package/release gates and
all previously recorded cross-platform requirements. Full original goal stays active.


## 2026-11-09 — Explicit optional display-capture deferral

The pinned renderer lacks getDisplayMedia. Extend only the already explicit
Code-OSS local-desktop application adaptation; do not add an inert public Session
setter or remove security filters. Both overlay and truthful LibJS runtime opt-ins
are required; ordinary Node/Electron behavior remains unchanged.

- Skip only configureSession's display-media handler registration in enabled local
  mode. Preserve both permission handlers and every webRequest filter unchanged.
- Pin NativeRecordingService at the existing Code-OSS revision, preserving license
  and exact upstream bytes. Report capture unsupported; recording, capture OS
  authorization and capture-settings entry points explicitly reject with
  ERR_CODE_OSS_LOCAL_DESKTOP_UNSUPPORTED before their real collaborators. Preserve
  the read-only status query. Disclose display-capture in frozen diagnostic status
  and the startup notice alongside webviews/remote/engine caches.
- Extend source-policy tests for hashes/reversal/drift and actual registration and
  recording method bodies on Node and Mite. Default branches preserve original
  calls; local branches reject before capture, prompts or settings launches. No
  OS capture or GUI tests are needed for this application-only policy change.
- Fresh 11,536-source build/code-oss-capture-final completes the actual entry and
  observes app ready with capture disabled. First failure is now
  session.defaultSession.webRequest.onBeforeRequest (undefined webRequest).
  Error.captureStackTrace remains secondary error reporting. See
  build/display-capture-code-oss.log. No workbench/open-edit-save acceptance.

Validation: extended test_code_oss_local_desktop.py passes with actual Node and
build/release-mite/bin/mite; pinned source and ordinary-byte reversal checks pass.
The previous accepted native permission slice (de57fbc, 126 regression suites)
remains unchanged; no new native frame/filter acceptance is claimed. Document the
application adaptation in ADR 0179 and refresh the current blocker inventory.
Targeted local-desktop and checkout-integrity CTest gates pass (2/2); scoped
formatting and 313 generated pages / 22,172 local links/resources pass. Logs:
build/display-capture-{overlay,compile,code-oss,policy,style,site}.log.
Ladybird remains unmodified.

Next: genuine authenticated request/frame policy for the unchanged webRequest
filters, then continue the real main entry. Native frame work is in progress and
unaccepted. Full capture, worker/device permissions, native utility/renderer ports,
workbench workflows and all previously recorded platform/package/release gates
remain required. The original goal stays active.


## 2026-11-09 — Canonical frame identity before application IPC

- Replace preload-only sender snapshots with read-only proxies backed by native
  canonical documents. Expose webContents.mainFrame before application IPC and
  webFrameMain.fromId; genuine IPC senderFrame uses the same proxy. The native
  document ID selects the sender; a packet URL cannot fabricate it. Retired queued
  senders do not invoke event listeners; unavailable invoke senders reject.
- New native registry derives current URL/origin, complete active hosted ancestry,
  actual managed-child processId and separate OS pid from canonical state. Native
  document/routing tokens are monotonic; frame-node IDs track real canonical nodes
  across document changes. Weak native records never keep old documents alive.
  Existing native ownership checks remain independent of these descriptive proxies.
- Bound snapshots to 256 frames/contents, 1024 tracked records/tree nodes, 4096
  visited candidates, 128 ancestor levels, 64 KiB fields and 2 MiB metadata. Invalid
  or exhausted data fails explicitly. Skip unhosted placeholders and descendants
  with missing ancestor hosts; an unhosted root returns no invented main frame.
- Maintained ESM validates snapshots, identities, scalar bounds and ancestry cycles;
  caches wrappers weakly and sweeps closed owners; every property read revalidates
  native ownership. Arrays
  are copied. Same-document URL changes retain identity; new documents/host loss,
  close and shutdown retire unavailable proxies. Routing methods/events and exact
  Chromium frame-host/BFCache behavior remain unsupported.
- Correct the first-bind coupling found during review: resetting preload IPC alone
  must not retire an unchanged canonical frame. Closing clears registry authority;
  navigation/crash is checked against the actual current document and live host.
  Update the standalone IPC peer to supply controlled canonical identity instead of
  constructing a frame from event URL data; malicious packet URLs are ignored.
- Actual offscreen Firefly acceptance passes a native main frame before any
  application IPC, real main/same/cross-origin metadata/ancestry, internal process
  versus OS pid semantics, identity lookup, hash retention, child-document
  replacement, canonical senderFrame aliases and actual owner closure. The hidden
  Electron 45.2.3 shared fixture passes. It must wait for actual frame retirement
  after the window closed event; Firefly document lifetime is intentionally tested
  separately from Chromium frame-host reuse.
- Add read-only API reference, ADR 0180, IPC/current status updates and the macOS
  release component gate. No global engine IPC header expansion, new dependency
  or Ladybird modification. Standalone Mite remains independent of Firefly/LibWeb.

Validation: Release CLI/embedded host and production/offscreen Firefly build.
All 127 runtime CTest suites pass (81.87 seconds), including six new Node/Mite
frame-policy groups and existing IPC/protocol/permission gates. Real Firefly and
Electron frame batches pass; the final exact-object component gate passes after
weak-cache cleanup. No device or visible desktop UI actions. Formatting passes
for 268 Python, 896 JS/web/config, 510 native and 67 CMake files. Site validation
passes for 315 pages and 22,313 local links/resources/fragments. Logs:
build/frames-{final-build,cli-build,policy-final,public,electron-final,component-final,runtime-final,style,site}.log.

Next: implement actual webRequest request/response interception using this native
frame authority. The fresh adapted Code-OSS entry still stops at onBeforeRequest;
no filter was added just to accept registration. Prefer the existing private byte
bootstrap for a separate authenticated typed control session, without expanding
engine endpoint headers again. Cover HTTP/cache/custom/file before transport,
headers before CORS, native navigation separately, bounded cancellation/teardown
and truthful main/utility networking scope. Last-listener replacement must match
Electron; a filter allowance must never become a filesystem grant. All previous
workbench, native port/extension-host/PTY, worker/device, Windows repair and
platform/package/release requirements remain open. The full original goal is active.


## 2026-11-09 — Enforce document and main-process webRequest decisions

- Implement defaultSession.webRequest.onBeforeRequest/onHeadersReceived with one
  listener per stage, include/exclude/type filters, null removal, strict bounded
  decisions, timeout/abort/replacement cancellation and explicit unsupported
  stages/redirects. Revalidate canonical frames before and after callback getters;
  no URL/payload-generated frame identity. Main net.fetch/net.request consult the
  same policy at each real hop and head, including custom routes and redirects.
- Add a private cookie/PID-authenticated per-helper byte control channel with typed
  generation/page/navigable/document identity. Native admission and completion
  independently check the real owner and ongoing top-level navigation worker.
  Keep the hidden navigation operation call in an exported owned LibWebView
  adapter; no upstream endpoint/header/source changes. Native odd logical request
  IDs and main even IDs are stable across stages, monotonic and never reused.
- Suspend actual Fetch after upstream security checks but before transport. Hold
  HTTP response heads before CORS; owned protocol/file responses wait before CORS
  and body credit. Preserve original body streaming/transfer ownership and caller
  stop steps. New helpers consume launch policy before VM initialization; existing
  helper updates are ordered asynchronous messages, not a distributed barrier.
- Bound 64 global/16 helper pending decisions, 30-second deadlines, logical IDs,
  clients, payload/header sizes and outgoing queues. Validate framing headers on
  both peers; unchanged copied framing fields work, edits/removals fail explicitly.
  Fix a codec contradiction discovered by real header traffic and extend native
  probes to every kind/flag combination, including positive header-stage messages.
- Active worker Fetch fails closed without touching owner-thread state. Policy
  reads use an atomic snapshot. Because upstream WebSocket handshakes bypass
  Fetch, an owned fingerprinted composition fails those handshakes explicitly
  before connect while interception is active. Full socket interception is open.
- Prevent silent utility bypass with a native temporary exclusion: filters cannot
  be installed with live utility children, and active filters prevent new utility
  launches, including reentrant argument getters. Real parent-mediated utility
  request forwarding remains necessary for Code-OSS and is the intended next
  implementation, not a permanent restriction.
- Exact production-object offscreen tests pass real HTTP endpoint-zero cancel,
  include/exclude/image filters, last/null semantics, ACAO rewrite before CORS,
  header cancellation/reset, canonical cross-origin frames and stable IDs,
  main net.fetch/net.request, custom file-provider cancellation, actual utility
  exclusion and WebSocket upgrade endpoint-zero denial. The shared Electron
  45.2.3 fixture passes its five supported checks. Native schema/startup probes and
  six controlled Node/Mite policy groups pass. Existing genuine frame, permission
  and public protocol regression batches pass offscreen. No visible desktop,
  device request, release or fresh Windows/Linux acceptance is claimed.
- A fresh real adapted Code-OSS entry now passes the original webRequest setters.
  A 3-second diagnostic was insufficient to reveal the next failure; extend the
  owned diagnostic with a bounded ELYTRA_CODE_OSS_OBSERVE_MS override. The 15-second
  observation reaches later main services and fails with synchronous require of
  ES modules and process uncaughtException registration; Error.captureStackTrace
  also fails in error reporting. No workbench/open-edit-save is accepted.
- Add detailed API/ADR/upstream-update docs, current compatibility/status and the
  macOS release component gate. Ladybird pin and gitlink remain unchanged and clean.

Validation: production/offscreen Firefly, native schema/startup probes and public
component gates pass. All 128 runtime CTest suites pass (36.70 seconds).
Formatting passes for 272 Python, 903 JS/web/config, 528 native and 69 CMake files.
Site validation passes for 317 pages and 22,452 local links/resources/fragments.
Logs: build/web-request-{codec-fix,component-final,component-accepted,policy-final,
electron-reference,frames,permissions,protocol,code-oss-observed,runtime-final,
style-final,site,upstream}.log. Early failing logs are retained, not counted as passes.

Read-only follow-up identifies the handled synchronous-ESM path: @vscode/deviceid
requires its nested uuid 14 ESM entry; base/node/id.js catches this and generates a
fallback UUID. The next fatal startup call is ErrorTelemetry.installErrorListeners
registering process.on('uncaughtException'), followed by unhandledRejection. The
trace has reached application-service initialization, not a running workbench.

Next: implement actual native EventLoop exception/rejection dispatch before
accepting those process listeners, then resume real Code-OSS startup. Preserve
normal fatal defaults and listener/monitor ordering; do not enable inert events.
Synchronous ESM require and Error.captureStackTrace remain separate runtime gaps.
Replace utility exclusion with parent-forwarded decisions before claiming shared/extension utility operation.
Worker/unbound navigation policy, other webRequest stages, display capture,
workbench open/edit/save, extension host/PTY, Windows repair and native platform/
package/release requirements remain open. The full original goal stays active.


## 2026-11-09 — Native process exception and rejection events

- Resume the paused error-event slice and implement uncaughtException,
  uncaughtExceptionMonitor, unhandledRejection and rejectionHandled through the
  actual LibJS/libuv loop. The private builtin bootstrap installs a rooted native
  dispatcher before admitting these process subscriptions. No inert EventEmitter
  registration or application-side replacement of Code-OSS ErrorTelemetry.
- Route timer/immediate/nextTick/microtask/native I/O callback failures through
  monitor then exception listeners, preserving the original value and origin.
  Missing handlers keep fatal status 1; a throwing monitor/exception handler exits
  7 without recursively invoking itself. Rejection-notification listener failures
  instead use the ordinary recoverable exception path. Mark delivery busy so a
  nested host pump cannot reenter the VM during application listeners.
- Rejection tracking consults actual LibJS handled state, preserves reason/Promise
  identity, suppresses same-turn catches and emits one later rejectionHandled.
  Notification batches root values across GC; previously reported Promises are
  weak and their metadata is removed after late notification. Actual idle host GC
  proves a notified Promise and ephemeral reason can both be reclaimed.
- Give Runtime's ESM entry completion one explicit native owner before driving
  jobs. Entry rejection/TLA failures go directly to uncaughtException with origin
  unhandledRejection even when ordinary rejection listeners exist; do not report
  again after recovery. CJS/eval throws retain synchronous origin and continue
  remaining owned work after a handler accepts them. Pending TLA status 13 and
  pre-application bootstrap/terminal exit policies remain distinct.
- Native Error promotion needed an engine-specific identity check: LibJS stack
  storage differs from V8 own properties. Preserve intrinsic Errors plus ordinary
  objects with an own stack property; wrap primitive/default reasons with
  ERR_UNHANDLED_REJECTION. A real differential test caught the original mismatch.
- The full tick regression caught premature rejection delivery after ESM jobs
  scheduled nextTick catches. Defer notifications until both queues are drained;
  the original same-turn ESM fixture now passes. The index-based checkout hash
  failure was resolved by staging the matching maintained/generated sources,
  preserving the real Windows LF/integrity test rather than weakening it.
- Add real Node/Mite/embedded-host callback, rejection, entry and fatal child tests.
  The latest target passes 29 shared behavior cases plus one embedded weak-GC
  reclamation gate (30 checks). Node and host callbacks perform actual GC; CLI
  behavior runs are explicitly labeled without a collector. No fake no-op GC or
  silent skipped lifetime check. Force-GC fixtures preserve exact original values
  through monitor/handler/rejection callbacks and future catches.
- The fresh 15-second actual adapted Code-OSS trace passes ErrorTelemetry and
  reaches CodeApplication.initChannels. Its next fatal dependency is powerMonitor
  speed-limit-change registration. The synchronous ESM device-ID fallback and
  Error.captureStackTrace gaps remain. No workbench/open-edit-save acceptance.
- Update detailed process/error API, ADR 0182, timer/compatibility/current status
  and the resumable work log. Native process error events do not add LibWeb,
  Node or V8 to Mite. Ladybird remains pinned and unmodified.

Validation: Release Mite/embedded host and production/offscreen Firefly build;
all 129 runtime CTest suites pass (38.16 seconds), followed by the expanded process
error target (4.42 seconds). Formatting passes for 273 Python, 913 JS/web/config,
528 native and 69 CMake files; site validation passes for 319 pages and 22,591
local links/resources/fragments. No visible desktop tests or device prompts. Logs:
build/process-errors-{checkpoint-build,firefly-final,policy-accepted,final-target,
runtime-final,code-oss,format,generate,upstream,site}.log. Earlier failing logs
record the harness collector assumption, intrinsic Error identity and checkpoint
ordering fixes; they are not counted as accepted runs.

Next: implement a genuine native CPU speed-limit event source for the actual
powerMonitor consumer, then rerun the main entry. Error.captureStackTrace,
synchronous ESM require, utility parent-forwarded request decisions, worker and
full session coverage, workbench workflows, extension/PTY, Windows repair and
native platform/package/release requirements remain open. The original goal stays
active; this process-event slice does not finish it.


## 2026-11-09 — Observe macOS CPU limits before the first published value

- Fix the next actual Code-OSS dependency using the existing genuine macOS CPU
  provider. Notification availability is independent of a current published
  CPU_Speed_Limit snapshot; IOPMCopyCPUPowerStatus may report not found before any
  value is published. Always register the passive kIOPMCPUPowerNotificationKey
  source and advertise its observation capability. This is not an inert listener.
- Preserve strict query behavior, validated integer limits in 0..100, weak/stopped
  callback ownership, notify_cancel cleanup and explicit registration failures.
  Missing/invalid readings emit nothing; never fabricate a 100-percent event.
  No hardware throttling, thermal stress or power transition is induced or claimed.
- Extend the native provider probe to require the observation capability even
  when its independent actual IOKit query is unavailable, then register/cancel
  the real passive source. Existing host validation/epochs/teardown and actual
  JS/Host/OS query/subscription gates pass, as does the standalone controlled
  power policy test. Production/offscreen Firefly rebuilds. Native formatting
  passes for both changed files. Mite's accepted 129-suite baseline is unchanged.
- A fresh actual adapted Code-OSS trace now passes initChannels and reaches its
  first CodeWindow/BrowserWindow construction attempt. It rejects unsupported
  BrowserWindow backgroundColor; no native workbench window or open/edit/save
  acceptance is inferred from that attempt. ESM require fallback and
  Error.captureStackTrace remain visible secondary runtime gaps.

Logs: build/power-cpu-{observer-build,policy,style,code-oss,site}.log. Linux/Windows
power providers remain unavailable; actual physical CPU-limit delivery remains a
separate acceptance gate. Ladybird is unchanged. Site validation passes for
319 pages and 22,592 local links/resources/fragments.

Next: implement genuine window backgroundColor behavior for the pinned consumer,
then continue its actual first-window path. Full workbench, utility request
forwarding and all prior native platform/runtime/package requirements remain open.
The original goal remains active.


## 2026-11-09 — Opaque native window and view canvas backgrounds

- Implement BrowserWindow backgroundColor, getBackgroundColor/setBackgroundColor
  and WebContentsView.setBackgroundColor with real native backing. Keep public
  webContents color methods absent, matching Electron. Main ESM supports opaque
  Electron hex/ARGB, comma RGB/HSL and native named colors, uppercase native
  readback, invalid-string white fallback and real TypeError for non-string input.
  Nonopaque colors fail explicitly without changing state; no transparency claim.
- Pass constructor RGB before OS show; update the actual final upstream Qt widget
  via owned helper functions, never a fake subclass. Owned native UI registry and
  ViewImplementation composition set native fallback; destruction removes opaque
  registry keys without dereferencing already-destroyed view bases.
- Deliver complete bounded native context/generation/RGB snapshots through the
  existing trusted BiDi state channel. Metadata records have empty source and are
  stripped/validated before preload execution and its independent budget. No
  DOM/CSS mutation, page setter, engine endpoint header or Ladybird source change.
- Compose the real Document canvas blend and PaintingRustBridge canvas input,
  preserving author CSS and ordinary iframe rules. Apply only registered active
  top-level contexts; invalid/stale/conflicting generations reject atomically.
  Native changes invalidate actual recording and repaint. Navigation/helper
  changes resend current per-view defaults; independent child views stay separate.
- Four controlled Node/Mite color groups and a hidden Electron 45.2.3 reference
  pass. Exact production-object offscreen public tests pass constructor/load,
  formats/types, setter/navigation, child API shape and alpha rejection. Native
  pixel acceptance passes initial fallback plus six genuine committed CPU canvas
  stages: blank, live update, authored override, retained authored paint after a
  base change, navigation and independent child canvas. These are compositor
  bitmap pixels through public native preview, not a physical CALayer/GPU claim.
- Preserve the diagnostic history accurately: the first probe used a protected
  upstream method; the next fixture kept the window hidden and produced no frame;
  macOS QWidget.grab then read chrome rather than its separate page CALayer. The
  corrected fixture explicitly requires the offscreen plugin, shows virtually
  after arming actual frame swaps and reads public native CPU previews. Valid
  producer diagnostics proved context/RGB delivery; no renderer guard workaround
  was needed. All temporary diagnostics were removed.
- Production/offscreen builds and the final combined color/frame/permission gates
  pass. Existing non-GUI isolation and public protocol regressions pass. All 130
  local runtime CTests pass (38.26 seconds). Source pin/reversal/drift guards pass;
  physical GPU/CALayer, zero-flash and fresh Linux/Windows acceptance remain open.
- Fresh real adapted Code-OSS startup passes backgroundColor and next rejects
  BrowserWindow minWidth. It still has no accepted workbench; the actual first
  window is not inferred from a constructor attempt. Error.captureStackTrace,
  the handled synchronous ESM device-ID fallback and broader runtime gaps remain.
- Add API/ADR/upstream maintenance docs and a macOS release component gate. Mite
  remains independent of renderer libraries; Ladybird remains pinned and clean.

Validation logs: build/background-color-{build,probe-build,public,trace,cpu-pixels,
component-final,policy,runtime,isolation,protocol,code-oss,format,site,upstream}.log.
Early failing fixture/diagnostic logs are retained and are not accepted runs.
Formatting passes for 277 Python, 920 JS/web/config, 538 native and 72 CMake
files; site validation passes for 321 pages and 22,731 local links/resources/fragments.

Next: implement genuine constructor window constraints for the pinned minWidth/
minHeight consumer, then resume the real first-window path. Other window/preload
options, workbench/open-edit-save, utility forwarding, workers, extension/PTY,
Windows repair and all native platform/package/release requirements remain open.
The original goal remains active.


## 2026-11-09 — Native window minimum and maximum size constraints

- Add BrowserWindow minWidth/minHeight/maxWidth/maxHeight and native-backed
  getMinimumSize/getMaximumSize/setMinimumSize/setMaximumSize. Constructor limits
  precede the first show and clamp initial bounds. All owned outer/content resize
  paths enforce actual native hints with checked frame/coordinate arithmetic.
- A window-owned controller binds genuine QWindow minimum/maximum hints and
  rebinds platform windows. Constructor useContentSize uses real frame margins;
  runtime setters always use outer DIP. Minimum wins conflicts while preserving
  requested maximum for subsequent minimum reset. Applying hints restores current
  native geometry, matching observed macOS Electron setter behavior.
- Inputs deliberately follow Firefly's strict integer 0..16384 geometry policy;
  invalid values fail before allocation/mutation. Each zero axis is independently
  unbounded and reported as zero, including partial constructor maxima. This
  differs from Electron's invalid-value coercion and partial-constructor INT_MAX.
  Positive outer limits smaller than native decorations report the effective
  frame-plus-one-client-DIP floor, rather than Electron's smaller requested value.
- Shared public cases pass hidden Electron 45.2.3, Firefly's static offscreen
  frontend and actual macOS Cocoa with show:false. Native offscreen acceptance
  checks real QWindow hints and direct native geometry enforcement in three
  stages, rather than trusting JavaScript snapshots alone. No visible desktop
  windows were launched. Interactive drag, mixed-DPI and fresh Linux/Windows
  acceptance remain open.
- All 131 local Release CLI CTests pass, including real Node/Mite constraint-policy
  cases. Production Cocoa, offscreen and exact-object native probe builds pass.
  Add the offscreen gate to the macOS release builder and document public methods,
  units, conflict/reset behavior, input differences and acceptance limits.
- Fresh adapted real Code-OSS startup passes native minWidth/minHeight and next
  stops at BrowserWindow experimentalDarkMode. The device-ID ESM require fallback
  and missing Error.captureStackTrace remain visible. No workbench/open-edit-save,
  extension host or terminal run is accepted.

Validation logs: build/window-constraints-{build,cli-build,runtime,public,cocoa,
electron-reference,code-oss,gate,geometry,format,site,upstream}.log. Formatting
passes for 279 Python, 925 JS/web/config, 542 native and 73 CMake files; site
validation passes for 321 pages and 22,734 local links/resources/fragments.
Independent read-only review confirmed the documented decoration-floor edge.
Ladybird remains unmodified.

Next: implement actual remaining window-option behavior, beginning with the
observed experimentalDarkMode consumer, then resume the first-window path.
Utility forwarding, workers, extension/PTY, Windows repair and native platform/
package/release requirements remain open. The full goal remains active.


## 2026-11-09 — Cocoa first-mouse policy and legacy window flag admission

- Add BrowserWindow acceptFirstMouse with real Cocoa behavior: false/default
  suppresses first-click delivery to content; true delegates to Qt's original
  acceptsFirstMouse implementation and retains native input-transparency guards.
  This uses bounded runtime subclasses on owned NSViews, never a process-wide
  Qt class method replacement or an upstream patch. Other native views retain
  their original classes, method implementations and callbacks.
- A content-owned QObject tracks QWindow/native surface identity and weak NSView
  lifetime. Native destruction restores the installed class while still owned;
  actual surface recreation automatically rebinds the current policy. Content
  mouse-button events fail closed while a replacement policy is unavailable,
  with bounded explicit diagnostics if rebinding fails. Exact identity checks
  prevent a retained old NSView from masquerading as the current native handle.
- Attached WebContentsViews inherit their current host's setting. Detach revokes
  the binding; cross-host moves configure the new native view before committing
  ownership or showing it. Failed attachment restores old policy, geometry and
  stacking, or closes the child if native rollback fails. Independent review
  identified the missing rollback restack, which is now fixed.
- Native capability/type validation precedes preload I/O/window allocation.
  Explicit true/false on offscreen/non-Cocoa backends fails unsupported. Omitted
  headless options remain usable without claiming OS activation behavior.
- experimentalDarkMode is a known inert legacy boolean, matching the pinned
  Electron 45.2.3 reference. A complete source archive scan finds no consumer;
  public true/false constructor cases also pass Electron. This is not a theme
  setter, and actual appearance remains nativeTheme's responsibility. Other
  supplied types and unknown window options continue to fail explicitly.
- Genuine hidden Cocoa callbacks pass both policies, view isolation, unchanged
  unowned Qt classes/IMPs, reinstall, reparent and teardown. Two actual QWidget
  destroy/create cycles produce distinct native NSViews and automatically
  restore false/true policies without reinstalling. Initial fixture failure
  assumed Qt's KVO subclass survives teardown; independent unmodified Qt controls
  prove normal NSKVONotifying_QNSView-to-QNSView cleanup. Corrected assertions
  require that baseline plus complete absence of our policy class in retired
  native inheritance chains. No production workaround was needed for that test.
- Public hidden Cocoa/offscreen and Electron cases pass constructor admission and
  child moves. Their existing detached-owner difference remains explicit:
  Firefly returns null, while Electron retains the last host in this case.
  All 132 local Release CLI CTests pass, including Node/Mite option policy.
  Existing offscreen WebContentsView pages/PIDs/viewport/binary IPC, ordering,
  reparent/detached navigation and host/child lifetime acceptance passes, as does
  the public/native window-size constraint regression gate.
  Cocoa/offscreen application and native probe builds pass. Add the hidden Cocoa
  gate to the macOS release builder; document API behavior, limits and ADR 0184.
- Fresh real adapted Code-OSS entry reaches constructor option validation and
  next rejects titleBarStyle, before native workbench window allocation. Native
  window background/constraints/input behavior is verified by separate component
  tests, not by that stopped startup. Missing Error.captureStackTrace and the
  handled device-ID synchronous-ESM require fallback remain visible.

Validation logs: build/window-input-{build-final2,probe-build,gate,
electron-reference,offscreen,runtime,regression,code-oss,format,site,upstream}.log.
Earlier failed build/fixture diagnostics are retained and are not accepted runs.
Formatting passes for 281 Python, 928 JS/web/config, 546 native and 74 CMake files;
site validation passes for 322 pages and 22,805 local links/resources/fragments.
Native callback tests show no windows and inject no OS input; actual inactive
window activation, broader renderer interaction and fresh Linux/Windows execution
remain open. Ladybird remains unmodified. The full project goal remains active.

Next: genuine titleBarStyle:hidden with native macOS buttons and content geometry,
then continue the first-window path and remaining webPreferences/runtime gaps.
Workbench/open-edit-save, extension host/PTY, utility forwarding, workers, Windows
repair and all platform/package/release requirements remain unfinished.


## 2026-11-09 — Preserve Cocoa KVO identity in first-mouse views

- The first actual visible renderer test for the pending hidden-titlebar slice
  exposed a native presentation crash: AppKit NSDP_getComputedPropertyValue
  asserted during QCocoaWindow::safeAreaMargins while showing the content view.
  Callback-only hidden acceptance had missed this path.
- The scoped first-mouse subclass inherited Qt/KVO's -class implementation, which
  assumes it is the outer runtime class. Its additional isa layer broke the
  reported class identity (the Qt diagnostic printed a nil class), invalidating
  AppKit's computed-property lookup.
- Capture the actual reported class before installation and provide an owned
  -class method returning that same identity. Actual object_getClass identity
  still names the scoped policy class; original Qt/KVO classes and methods remain
  untouched. Both first-click values, guarded true delegation, lifecycle cleanup
  and the bounded cache remain intact. Independent review found no new confirmed
  identity/lifetime defect.
- Extend the hidden native gate with exact reported-class identity, actual
  NSView.safeAreaInsets and QWindow.safeAreaMargins reads, real native resizing,
  and the same checks after automatic false/true view recreation. All pass.
  Compiling the unchanged preceding adapter against this new regression fails
  deterministically at the class-identity assertion, before unsafe property access.
- All 132 local CLI CTests belonging to this committed scope pass (the pending
  title-bar test is excluded). Updated native/app/probe builds pass. The pending
  title-bar's exact-application visible renderer gate also now passes real CPU
  toolbar pixels, full viewport, native button hit targets and live HTML regions.
  This confirms the original presentation crash is fixed; it does not establish
  a Code-OSS workbench or broad activation/fullscreen/platform parity.

Validation: build/first-mouse-identity-{build,native,before,runtime}.log and
build/title-bar-gate-final.log. The first title-bar GUI attempt aborted; it is
retained in build/title-bar-gate.log as a failure, not an accepted run. After the
hidden regression passed, one successful targeted visible batch completed.
Ladybird remains unmodified. Hidden-titlebar implementation/docs still form the
next separate logical step. The full goal remains active.


## 2026-11-09 — Hidden Cocoa title bar with native controls and full renderer content

- Add BrowserWindow titleBarStyle:default/hidden. Hidden configures genuine titled
  AppKit windows with Qt ExpandedClientAreaHint/NoTitleBarBackgroundHint before
  native creation. A window-owned Cocoa controller hides the caption and reapplies
  it after actual NSWindow recreation. Close/minimize/zoom remain real native
  controls; hidden wins over frame:false, matching the pinned Electron behavior.
- Explicitly disable Qt's top-level safe-area layout inset for hidden style so the
  actual renderer fills the full client area under the native controls. Geometry
  and min/max constraints use actual frame margins without a fixed title height.
  Both frameless and hidden-style windows enable the existing bounded HTML CSS
  app-region adapter. Default framed windows preserve ordinary native behavior.
- Strict type/style/backend admission precedes preload I/O/native allocation.
  Unknown modes (hiddenInset/customButtonsOnHover), titleBarOverlay and non-Cocoa
  hidden styles remain unsupported; the default is portable unchanged chrome.
- Shared public native geometry/content-size/frame:false and constrained-resize
  cases pass hidden Electron 45.2.3 and Firefly Cocoa. Offscreen rejection and
  invalid input/window-count preservation pass. Native hidden probes verify real
  buttons and hit targets, full layout, independent default windows, Qt-local
  drag/no-drag behavior and actual NSWindow recreation with old-caption cleanup.
- The exact-application Cocoa runtime probe briefly presents one real window.
  Actual accepted renderer CPU pixels verify the HTML toolbar at the top of the
  viewport; native coordinates verify full-window content, actual AppKit hit
  testing reaches the enabled wired window buttons, and author CSS produces live
  drag/no-drag regions. The fixture then hides/closes itself. No OS input is
  injected and no native button action is invoked. Its first run found the
  presentation crash fixed separately in 222cc36; the accepted subsequent run is
  green. Fullscreen, broader activation/input and fresh platform parity remain open.
- All 133 local CLI CTests pass, including actual Node/Mite style admission.
  Existing offscreen WebContentsView real pages/PIDs/viewport/binary IPC,
  reparent/order/detached navigation/lifetime and public/native size constraints
  pass. Add the targeted native/renderer gate to the macOS release builder;
  refresh API/compatibility/development docs and ADR 0185. Ladybird is unmodified.
- Fresh adapted Code-OSS default startup next rejects titleBarOverlay during
  constructor option validation. A separate temporary profile using Code-OSS's
  own window.titleBarStyle:native setting reaches webPreferences.v8CacheOptions.
  Both retain app readiness but workbenchAccepted:false and no editor window.
  This optional chrome configuration changes diagnostic priority; it does not
  implement the overlay API or reduce the full project scope. Missing
  Error.captureStackTrace and handled device-ID synchronous ESM require remain.

Validation: build/title-bar-{build,probe-build,gate-final,electron-reference,
offscreen,runtime,regression,code-oss,code-oss-native-profile,format,site,upstream}.log.
The earlier title-bar-gate.log is a failed run and is not accepted. The full goal
remains active.
Formatting passes for 283 Python, 933 JS/web/config, 551 native and 75 CMake files;
the subsequent identity-fix files also pass their focused style check. Site
validation passes for 323 pages and 22,878 local links/resources/fragments.

Next: preserve a reproducible native-titlebar Code-OSS diagnostic profile and
address genuine runtime/webPreferences semantics along that startup path.
Window Controls Overlay/other styles remain explicit framework work. Workbench
open/edit/save, extension host/PTY, utilities/workers, Windows repair and all
platform/package/release acceptance requirements remain unfinished.


## 2026-11-09 — Reproducible isolated native-titlebar Code-OSS diagnostics

- Add probe-only --native-title-bar using Code-OSS's own supported native-caption
  preference. Every opt-in creates a fresh short UUID profile (0700 directory,
  exclusive 0600 settings file) plus a private extensions directory; caller
  settings and extensions are never reused or overwritten. The retained paths
  and selection are printed explicitly for diagnostic inspection.
- Consume only a real probe option before -- and inject actual Code-OSS profile
  options before the terminator. Literal post-terminator arguments remain intact.
  Duplicate flags and conflicting user-data/extension options reject before any
  profile creation. Default invocations preserve their arguments/profile choice.
- Node 24.15.0 and actual standalone Mite pass isolated/default/conflict/terminator
  and entry-failure cases, including unchanged existing settings/extension
  sentinels and distinct private profiles. Add the non-GUI case to CTest and
  document the reproducible command. No desktop acceptance is claimed by this tool.
- Profile and completion records retain workbenchAccepted:false. Native caption
  is a diagnostic configuration, not Window Controls Overlay support or a reduced
  project objective. The observed native-profile blocker remains v8CacheOptions.

Validation: build/code-oss-native-profile-{policy-final,style}.log. The prior
complete runtime baseline remains 133 passing suites; capture-stack implementation
and its new suite are still a separate pending worktree step. The full goal stays
active, with editor/open-edit-save and all remaining platform/API work unfinished.


## 2026-11-10 — Real captured stacks and lazy source CallSites

- Install Error.captureStackTrace in the standalone and embedded Mite realm before
  user entry. It snapshots actual public LibJS VM frames, trims by exact function
  identity, samples captured-stack limits, and installs proper non-enumerable
  getter/setter descriptors. Lazy name/message formatting, arbitrary cached values,
  recapture, frozen-target assignment and inherited/borrowed receivers pass the
  real Node 24.15.0 behavior checks; target Proxies reject without invoking traps.
- Native WeakMap target records own cached JS values and frame data. Native
  functions capture direct traced GC pointers; no JS value or borrowed execution
  context is hidden in C++ storage. Separate branded WeakMap CallSite records own
  only capture-time source/name metadata, so retained source sites do not retain
  otherwise unreachable capture owners.
- Add genuine lazy Error.prepareStackTrace callbacks with the original Error
  constructor receiver and actual target/source-site array. Arbitrary results
  cache; exceptions allow retry. A realm guard makes same/other captured-stack
  reentry use default formatting; record guards reject recursive name/message
  formatting explicitly and reset after failure.
- Seven source CallSite methods provide actual names, filenames, source positions,
  engine-native flags and display text. Borrowed methods select the real receiver;
  forged/Proxy receivers fail TypeError without traps. Unsupported V8 metadata
  throws ERR_MITE_UNSUPPORTED_API. Actual tests disproved using the interpreter's
  caller_is_construct return-control flag for general constructors, so isConstructor
  remains explicitly unsupported pending a genuine public/owned Rust ABI seam.
- Node/Mite/embedded-host differentials pass normal/constructor-function identity
  trimming, lazy/cache/error behavior, limit accessor suppression/inherited data,
  source metadata, private-intrinsic protection, formatter reentry and bindings-style
  caller-file discovery. Real Node/host GC retains active/lazy/arbitrary cached
  data and retained source sites; idle host GC reclaims capture owners and cached
  payloads, including owners of retained CallSite arrays. CLI checks do not fake GC.
- The first default-capture-only Code-OSS trace exposed its bindings helper using
  prepareStackTrace and leaving that hook installed if formatting threw. Implementing
  genuine source sites fixes that consumer rather than silently accepting an inert
  hook. The final actual isolated native-titlebar entry no longer reports missing
  captureStackTrace or unsupported prepareStackTrace. It now reveals absent native
  @vscode/spdlog binding searches, the handled device-ID ESM require fallback, and
  still rejects webPreferences.v8CacheOptions before window creation. App readiness
  is observed, workbenchAccepted remains false; no editor/open-edit-save is claimed.
- All 135 local Release CLI CTests pass. Current production Firefly and CLI/embedded
  hosts build. Detailed API, compatibility and ADR 0186 document captured-only
  limits/hooks, engine-specific formatting, unavailable V8 introspection, async
  stack limitations, native-error/fatal fallback behavior and fresh platform work.
  No Ladybird source, pin or license notice changed; Mite remains independent.

Validation logs: build/error-stack-{sites-accepted,runtime-final,metadata-build-final,
firefly-final-build,code-oss-final,format,site,upstream}.log. Earlier failed builds,
constructor-metadata assumptions and unsupported-hook diagnostic logs are retained
and are not accepted runs. Formatting passes for 285 Python, 948 JS/web/config,
555 native and 75 CMake files; site validation passes for 325 pages and 23,016
local links/resources/fragments. No visible UI batch was run in this step.

Next: narrowly audit/adapt Code-OSS's V8-only cache policy without claiming a V8
cache in Firefly; retain and implement real zoom, autoplay, paste, sandbox and
other observable renderer semantics. Native-titlebar diagnostics remain isolated;
Window Controls Overlay, native addon/logging gaps, synchronous ESM require,
workbench editing, extensions/PTY/utilities/workers, Windows repair and all
platform/package/release requirements remain open. The full goal stays active.


## 2026-11-10 — Audited Code-OSS disabled V8 window-cache adaptation

- Add the exact pinned windowImpl.ts reference, manifest hash and license-preserving
  reversible application overlay. Only experimental local-desktop builds change
  this source. Ordinary Node/Electron retains the original cache preference.
- On verified Mite/LibJS local-desktop policy, omit only v8CacheOptions:none.
  Enabled cache requests, unknown engines, missing/inconsistent policy and conflicting
  diagnostics reject explicitly. General Firefly webPreferences validation remains
  strict; no V8/Blink cache implementation is claimed. Other preferences retain
  their original values, identities and evaluation order.
- Record the omitted preference in an immutable process diagnostic, included by
  the probe at entry completion and final observation when reached. Exact-source
  drift checks refuse changed or duplicated seams before output creation.
- Actual Node 24.15.0 and Mite pass 18 behavioral scenarios plus pinned-source,
  reversible-overlay, opt-in, provenance and prewrite rejection checks. Register
  the focused CTest; all three targeted cache/local-desktop/profile suites pass.
  The prior complete runtime baseline remains 135 passing suites; this adds one
  registered suite, without claiming a fresh full-suite run.
- Compile a fresh actual 11,537-input Code-OSS tree with prepared dependencies,
  NLS and policy watcher. A bounded isolated native-titlebar diagnostic reaches
  app readiness and passes the former V8 cache rejection, then rejects
  webPreferences.spellcheck before window creation. The process exits before its
  observation timer; no final observation record or editor acceptance is claimed.
  Native spdlog lookup errors and the handled synchronous ESM require fallback
  remain. Source trees and Ladybird are unchanged; no visible editor opened.
- Document the adaptation and validation command. Targeted formatting and diff
  checks pass; the documentation site builds and validates.

Validation: build/code-oss-window-cache-{overlay,compile,tests,regression,profile,
configure,ctest,start,site}.log. Next: genuine spellcheck preference semantics,
then remaining renderer preferences and real logging fallback. Workbench loading,
open/edit/save, extensions, PTY, workers, Windows and release acceptance remain
unfinished. The full goal remains active.


## 2026-11-10 — Genuine disabled native spelling preference

- Support webPreferences.spellcheck:false in the shared BrowserWindow and
  WebContentsView validator. It reads the option once, validates a boolean and
  admits false only when a trusted native query reports the actual spelling
  provider absent. Missing/malformed capability queries fail closed; true remains
  explicitly unsupported. Existing isolation, sandbox, preload and argument
  policies remain intact. Omission leaves spelling disabled, an intentional
  difference from Electron's enabled default.
- Upstream Ladybird already propagates provider absence as disabled spelling and
  writing suggestions to every page/helper, and returns empty request responses.
  Firefly installs no provider. Both native factories additionally check actual
  provider absence before preload I/O or allocation, including omitted defaults.
  No Ladybird changes, fake dictionary or inert preference admission is involved.
- Actual Node/Mite tests pass four shared policy groups. Hidden public Firefly and
  Electron 45.2.3 windows and child views load real HTML documents with false;
  Firefly rejects enabled/nonboolean requests before allocation. An exact-object
  offscreen frontend installs a real native SpellChecker, verifies both explicit
  false and omitted window/view requests reject without extra native widgets or
  provider calls, removes the provider and verifies exact-count recovery. These
  checks do not claim renderer spelling RPC, OS dictionary or underline acceptance.
- Production Firefly, shared-object offscreen frontend and native provider probe
  build. Both selected Release CTests (new spelling policy and cache adaptation)
  pass. The prior complete runtime baseline remains 135 passing suites; two new
  suites are now registered, without claiming a fresh complete run. No visible
  GUI batch was launched; all created test windows stayed hidden/offscreen.
- A fresh bounded isolated native-titlebar Code-OSS start reaches app readiness,
  passes spellcheck:false and now rejects webPreferences.zoomFactor before native
  window creation. Native spdlog lookup errors and the handled synchronous ESM
  require fallback remain. No editor/open-edit-save acceptance is claimed.
- API, both compatibility matrices, adaptation guide and ADR 0187 record supported
  behavior, default difference and unfinished enabled/per-view/session controls.

Validation: build/spellcheck-{build,final-build,cli-build,public,native,ctest,style,
code-oss-start,site}.log. Next: genuine zoomFactor/renderer zoom and the remaining
window preferences, then real file logging fallback and workbench acceptance.
Extensions, PTY, workers, Windows/platform packages and releases remain unfinished;
the full objective remains active.


## 2026-11-10 — Native main-process and constructor browser zoom

- Add real webPreferences.zoomFactor for windows and child views, factor/level
  webContents methods and property aliases. The main/preload APIs read and modify
  one actual upstream view state. Strict numeric validation uses the real 0.3..5
  factor range, maps levels as 1.2^level, rejects without coercion/mutation and
  validates native construction before preload I/O or native allocation.
- Actual initial renderer checks exposed upstream tab initialization resetting
  constructor zoom. Add a scoped owned-view membership hook to the existing exact
  canvas source composition: only registered Firefly views skip browser host zoom
  settings. Keep all unowned upstream behavior and exact reversible source bytes.
  Registry entries store no factor and are removed through opaque QObject cleanup.
  The first build also exposed missing shared-library exports for registry calls;
  corrected with the existing WEBVIEW_API convention before accepted builds.
- Node/Mite numeric/preference suites and coordinator source/hash/drift/prewrite
  checks pass. All 139 local Release runtime CTests pass. Production and offscreen
  Firefly build; real hidden document tests measure layout/DPR, constructor factors,
  runtime factor/level methods/properties, preload-main coherence, independent
  child state and navigation/location.reload. Existing preload zoom and all seven
  CPU canvas stages pass. No visible desktop window was launched in this step.
- Electron 45.2.3 passes the shared numeric/layout/reference cases. Actual reference
  checks exposed ESM readiness deadlock in the initial test fixture, fixed by using
  the established whenReady callback. They also show constructor getter timing
  and reload differences: Firefly preserves current view zoom while this Electron
  fixture reapplies constructor zoom. Tests and API/ADR 0188 record these explicitly.
  Origin/session propagation, persistence, visual limits and fresh platforms remain
  unfinished; no full Electron zoom parity is claimed.
- Actual isolated native-titlebar Code-OSS startup now passes zoomFactor and stops
  at webPreferences.autoplayPolicy before window creation. Native logging and the
  handled synchronous-ESM fallback remain; no editor/open-edit-save is accepted.
  Refresh the current status page and both compatibility/API guides accordingly.
- User requested another Ladybird update. Official remote fetch selects candidate
  cdcb9a308fc9d9f34e3ebd7c5a7e6b5a63972cdf (859 commits after our accepted pin).
  Read-only audit finds explicit GC capture migration, RemoteView/Hub architecture,
  build/dependency changes and 24 changed direct adapter inputs. This is not an
  accepted engine update yet. Retain the accepted pin while preparing an isolated
  review; no Ladybird source modifications are allowed.

Validation: build/zoom-{policy,policy-ctest,source,owned-view-final-build,
cli-final-build,runtime-final,public-accepted,preload-regression,canvas-regression,
code-oss-start,style,site}.log. Earlier failed initial-view/export/reference tests
are retained as rejected runs. Next: isolated Ladybird candidate port/build and
security/renderer revalidation, then autoplay and remaining Code-OSS dependencies.
The complete product/platform/package/release objective remains active.


## 2026-11-10 — Ladybird cdcb9a candidate review (NOT ACCEPTED)

Work continues in build/ladybird-update-2026-11-10 on the isolated
codex/ladybird-update-2026-11-10 branch. Accepted master is 22f6dcd with the
97ad425 Ladybird pin and the previously accepted runtime built at 1d18d82.
The candidate is not merged or released. The first local checkpoint 0e0ce1e
covers standalone Mite, dependencies, capture ownership and ICU packaging.
The next coherent step covers the real Hub/frontend/renderer integration below.

Implementation:

- Official upstream cdcb9a308fc9d9f34e3ebd7c5a7e6b5a63972cdf is 859 commits
  after the accepted pin. Manifest, indexed gitlink and clean checkout agree.
  Bootstrap uses the unchanged canonical vcpkg manager/baseline. ICU 78.3#3,
  mimalloc 2.2.7#1 and the shared overrides match the SDK. The aggregate Cargo lock
  retains exactly the candidate's versions/checksums plus our aggregate crate;
  its 124-package audit passes. Mite remains independent of LibWeb/Firefly/Qt.
- LibJS now requires explicit GC captures. Native callbacks expose actual JS
  edges; borrowed owners revoke an independent lifetime anchor before teardown.
  The real retained-function regression collects while live, during native
  destruction and after deletion. SDK error literals use the new UTF-16 contract.
- The no-JIT SDK has a JIT-only method without its feature guard. An owned exact
  source composition adds only cfg(feature=jit), preserves Cargo/build inputs
  and routes every other module back to upstream. Correct nested-module routing
  fixes the initial Unicode/calendar diagnostics without patching their bodies.
  Warnings stay fatal and JIT stays disabled. See ADR 0189.
- Select upstream's full external ICU data branch because Mite needs the native
  CJK converter mappings. Stage the actual 33,107,232-byte file and hashes;
  no SDK path or environment override may supply missing runtime resources.
- Fix the existing quadratic fork JSON framing reader with one-pass scanning and
  bounded geometric assembly. The original 768KiB fixture and eight-second
  watchdog pass unchanged; controlled fragmented/Unicode/limit/EOF/reentry/quota
  tests verify linear-copy bounds. Generated runtime bundles are reproducible.
- Embed the genuine in-process Hub, share the existing Core/Qt loop and provide
  an owned minimal frontend without browser stores/startup tabs. Resolve actual
  canonical view/page/frame/document authority for preload/IPC/permissions and
  network paths. Preserve renderer launch isolation, pending sandbox selection,
  native GPU-exit observation and real prepare/setParent/finish view reparenting.
- Reviewed source compositions preserve exact unrelated bytes, callback ordering,
  OS permission denial, request/CORS gates, canvas/zoom behavior, AX tree/delta
  ownership and native file provenance. Regenerated IPC endpoints preserve all
  245 client and 254 server original SDK message IDs. Vendor sources stay clean.
- Enforce the same composed RemoteView header across the complete target consumer
  closure, including aliases and object sources, while keeping standalone
  Ladybird's original layout. The actual compiler-database guard checks 107
  commands; real CMake fixtures reject conditional/mixed-layout mistakes.
- Weak callback proxies clear before incremental destruction of native GC capture
  boxes. Retire a weak-dead proxy's native endpoint only when no Promise is
  pending; preserve pending endpoint revocation. Existing callback counts,
  64-slot reclamation, GC and pending-result assertions remain unchanged.
  ADR 0190 records the Hub, ABI and collector decisions.

Actual acceptance in this checkout:

- Complete standalone Debug CLI/source aggregate: all 145 suites pass in 130.07 seconds
  (build/candidate-runtime-145.log). The same reused tree now builds Release,
  whose full 145-suite run also passes in 54.99 seconds
  (candidate-cli-release-build.log and candidate-cli-release-145.log). The previous 141-suite run and 140-suite
  standalone checkpoint are historical, not different current totals.
- Full native Release Firefly and offscreen binaries compile/link. Logs:
  candidate-firefly-final-link.log and candidate-header-fixed-build.log.
  Strict candidate compiler checks also cover 58 bridge/helper, 21 native Host,
  27 additional Qt/native, 40 composed-source and 38 isolation/protocol TUs.
- Production/diagnostic helper world and real byte-channel gates pass: early
  author-script ordering, default-deny control, isolation, frames, calls/Promises,
  private errors/rejections, timers/observers/listeners, event payloads and native
  traversal/principal callbacks. The complete helper gate passes again after the
  callback retirement fix (candidate-helper-final-acceptance.log); the earlier
  candidate-helper-acceptance.log contains the rejected resource fixture too.
- Full native isolation passes after correcting incremental callback retirement
  (candidate-isolation-fixed.log). Real callback release/revoke assertions were
  not removed or weakened. The original rejected run is retained separately.
- All ten selected public offscreen suites pass: frames, session permissions,
  exact native permission boundary, webRequest, canvas pixels/background, zoom,
  WebContentsView, real window constraints, disabled spellchecking and CPU/GPU
  policy (candidate-offscreen-acceptance.log).
- All eleven additional public offscreen integration suites pass: sandbox enable,
  IPC channel/binary/outbound, preload process/getters/zoom, application paths,
  command-line policy, tracing and modern response/file protocols with genuine
  native process identity and generation/cancellation checks
  (candidate-public-offscreen-acceptance.log).
- All six headless document resource cases pass, including equal-epoch replay
  denial and legitimate higher-epoch refresh with six actual resource requests
  and old-channel retirement (candidate-document-refresh-acceptance.log).
- Renderer-selected ICU staging, independent bundle copies, Linux hardlink layout,
  tar/ZIP relocation and actual native CJK decoding pass without environment
  overrides (candidate-renderer-icu-package.log). Payload and artifact-provenance
  unit checks pass (candidate-package-rules.log); these are not full app smoke.
- The pure native controlled Host/sandbox cases pass in candidate-pure-accepted.log
  (its aggregate stopped at the original isolation assertion). The remaining
  notification/desktop-capture/power-monitor/webRequest/protocol targets pass
  as a terminal successful run in candidate-remaining-native-acceptance.log.
  Controlled providers prove native admission, delivery, GC and teardown; they
  do not prove real OS notifications, dialogs, permissions or sleep events.
- Style checks cover 308 Python, 961 JS/config, 573 native and 82 CMake files.
  The final callback-registry formatting correction passes its focused check.
  Site validation passes 329 pages and 23,289 local links/resources/fragments.

Rejected evidence and corrections:

- Initial standalone shutdown GC visited a retired ImmediateQueue; revocable
  anchors and actual retained-function destruction/GC regression fixed it.
- Initial legacy encoding failed after SDK converter trimming; real full ICU
  data restores all 1,657 reference comparisons. Initial fork failure was late
  delivery from quadratic copying, not a child crash; no deadline was increased.
- The notification composition left a callback decision unnamed because an
  identity replacement silently did nothing. Name the exact final parameter,
  reject identity replacements and compile all four actual permission TUs.
- The first public window crashed in ViewState assignment because original and
  composed RemoteView headers produced different layouts. Fix consumer include
  precedence and verify actual header selection rather than changing zoom.
- The resource duplicate fixture incorrectly sent a newer allowed epoch to an
  unserved endpoint. Preserve production policy; test true replay denial and
  higher-epoch refresh separately. ADR 0123 now links its ADR 0175 supersession.
- The new sandbox launch probe needed an explicit AK link. Its genuine native
  latch/options tests now execute; syntax-only success was insufficient evidence.

Update acceptance and remaining work before master:

- The local Release package, real helper/resource/dylib/ICU loading, targeted
  Cocoa/presentation and complete current Designer smoke gates now pass, as scoped
  in the follow-ups below. Retire temporary package copies after their review
  evidence is complete; keep the accepted builds and original Finder metadata.
- Obtain fresh hosted Windows CLI and Linux renderer/macOS package evidence.
  Linux CLI succeeds at abd65d1; Windows exposed another OpenSSL socket include
  needing the owned boundary described below. Each subsequent source still needs
  its own fresh platform result. Native Windows desktop remains unfinished.
  Local vcpkg objects warn of a
  macOS 27 build target versus the 14.4 executable minimum, so this host cannot
  establish back-deployment support. No warnings are hidden to claim it.
- Update current docs and CI evidence, then consider master promotion. No new
  alpha release and no working Code-OSS editor are claimed. Accepted native-titlebar
  Code-OSS startup still stops at webPreferences.autoplayPolicy; its actual value
  is user-gesture-required, whose media/WebAudio enforcement needs real work.

The integration checkpoint f5e7b5a is pushed on the isolated update branch.
Direct workflow dispatch via gh could not run because the local CLI has no API
login; no remote workflow was claimed from that failed command. Extend the four
existing native workflow push filters to codex/ladybird-update-* so candidates
receive the same acceptance without a pull request or API dispatch credentials.
Release publication conditions and workflow token permissions are unchanged.
Actual remote runs still need observation after this metadata step is pushed.
Validation: pinned Prettier parses all four workflow files; exact comparison
proves only their branch filters changed. The native gate, permission and
publication bodies are byte-identical (candidate-ci-scope-validation.log).

Observed native CI for21b5568: Windows42763478980, Linux CLI42763479045,
Linux renderer42763478994 and macOS42763479125 all started from the candidate
push. The macOS job129654408607 failed before compilation in check_macos_cpu.py:
the old exact flag list rejected the new genuine mite_nojit_build_<hash> marker.
Alpha publication was skipped. Linux has the same stale validator shape.

Derive the additional allowed marker only from the actual generated
mite-adapters/rust-nojit/lib.rs SHA256, after the optional origin marker. Keep
target CPU, fatal warnings and flag order exact. Both CPU regression suites pass
(two tests each, including missing/wrong/duplicate/reordered/stale/mutated-root
cases). Actual generated roots/flag order were inspected in both local SDK
trees; their baseline remains OFF, so this is not represented as baseline CPU
execution acceptance. A new hosted baseline run is required. No native runtime
or package test was skipped to bypass the stale validator.

The old Linux CLI job129654406193 completed compilation and passed142/143
suites. Only file-write-stream/errors rejected the already documented pre-abort
open difference: Node printed existsSync=true at close while Mite correctly
printed false. Node may observe either value before its pending open completes;
Mite deliberately never opens a pre-aborted target. The runner already allowed
only Node's final empty abort.bin, but its stdout comparison was still too strict.
Require Mite's complete original false/null/pending/zero-byte observation, allow
only that one Node close-time boolean to vary, and keep every other line, event,
descriptor, final file and return/error check exact. Actual local Node/Release
Mite validation passes all seven writer groups, policy and short-write cases
(candidate-write-stream-oracle.log). Production code and deadlines are unchanged;
API documentation now explains both Node observations. Hosted validation remains
pending until the corrected runner is pushed and executed.

The old Windows native job129654457257 failed compiling EventLoop/Runtime/
ImmediateQueue: new explicit-GC headers include AK/Time before libuv/Winsock,
revealing two timeval declarations. Add one owned platform/UV.h boundary: load
actual AK time types, then scope the same Winsock-only dummy_timeval spelling as
upstream AK/Windows.h around libuv. Do not import its cpp-only SystemApi initializer
into runtime headers, change type layouts or fake _TIMEVAL_DEFINED. All16 owned
Mite/Firefly direct libuv includes now use this boundary; vendor sources are clean.

Three compile-only include-order fixtures are dependencies of the normal mite
build. Windows branches assert actual SDK timeval layouts, the imported Winsock
select function pointer type and macro restoration. The maintained source guard
and its negative fixtures reject direct libuv-header bypasses. Local Release
Mite and Firefly/offscreen rebuilds pass; the complete CLI/source aggregate now
passes147/147 in53.47 seconds (candidate-uv-release-build.log,
candidate-uv-release-147.log, candidate-uv-firefly-build.log). These are Mac
checks; the real WinSDK assertions still require the next hosted clang-cl build.
No native Windows acceptance is inferred from local syntax or source tests.

Package snapshot21b5568 stages from clean Release/Release inputs with all10
physical helpers and full ICU data. Strict nested signatures and original helper
entitlements/flags pass. The stripped Designer bundle is263,625,085 bytes
(251.4MiB), of which the empty Firefly app baseline is227,835,245 bytes(217.3MiB).
This is the default local Release profile, not a size/LTO or fresh-platform claim.

Add a diagnostic-only --helper-directory selector before helper initialization;
keep production app flags and default diagnostic selection unchanged. The
selector rejects missing/duplicate/relative/symlink/incomplete directories and
checks physical helpers/resources and lookup candidates before launch. Extend
the protocol gate so positive cases genuinely use the selected package, then
observe a live actual child through the OS: parent/executable identity, mapped
liblagom libraries and ICU mapping/open-file evidence. No page-supplied path or
file-existence-only check substitutes for actual loading.

A temporary copied Unicode-path Designer.app passes strict signing, production
pre-GUI runtime dylib loading without SDK overrides and the actual packaged Mite
CJK decoders (candidate-package-relocation-loader.log). A second temporary copy
passes genuine headless production WebContent origin/configuration and actual
libproc/lsof executable/dylib/ICU evidence with the selected package paths
(candidate-packaged-helper-relocation-final.log). Both temporary copies are gone.
The unchanged default/runtime helper cases and new three-case path-policy unit
suite pass (candidate-package-probe-default-accepted.log). Linux path checks
follow its manifest-backed firefly-runtime/bin, sidecar resources and root lib
inventory; those non-native fixtures do not establish Linux execution. Site
validation passes329 pages/23,291 links (candidate-package-site.log). Native
Designer HTML/Cocoa smoke remains a separate acceptance requirement.

Retain only current/accepted builds and compact evidence. Master 22f6dcd removed
obsolete staging/cache copies, reducing the project from 66.8 to 33.6 GiB; do not delete
its active review worktree, shared installed dependencies or running Designer app.
The full product/platform/VS Code/release objective remains active.


### Candidate follow-up: native shutdown and remaining hosted failures

The next hosted Linux CLI run, 42765988282 at 359fd82, passes. The other
three native workflows failed: Windows 42765988289 still tests the old direct
libuv includes (the local da3ef59 correction needs pushing); macOS 42765988615
reached compilation of a remaining capturing test callback in the power-monitor
public probe; Linux renderer 42765988473 stopped in independent X11 window-manager
readiness before renderer compilation. These results do not accept the update.

A targeted local Cocoa batch initially stopped before presentation: two pure Qt
probes unnecessarily included GC registration without linking the GC runtime.
Removing those unused includes makes both genuine probes link. The first hidden
file-input case then exposed a real exit crash: a deferred IPC strong reference
destroyed ConnectionFromUI after its Hub Application had already been destroyed.

The owned source composition now retires the original Application callbacks while
the Hub is alive, closes both real IPC endpoints and makes late destructors and
disconnect handling harmless for retired endpoints. Original upstream cleanup
bytes and endpoint class layouts are preserved; fingerprint/reversal/drift and
missing/duplicate seam controls pass. The unchanged hidden file-input regression
now passes trusted native input, Qt acceptance, SelectedFile IPC, four-MiB text
and byte contents, basename privacy, cancellation and navigation with clean exit
(candidate-hidden-file-picker-lifecycle.log). No visible desktop test was run
in this follow-up; the accepted Designer app remains available for manual testing.

A separate Qt numeric-collation probe catches two ICU implementations being
linked into pure Qt executables. The initial link-order correction passed the
full frontend graph, but still failed the smaller picker graph; do not treat
presence of the shared Unicode dylib alone as proof that Qt calls its ICU copy.
Actual linked symbols and both native collation probes are the acceptance gate.
On macOS with the actual shared/external-data Unicode target, present that real
library before static archive selection through the linker's needed_library
option, retaining its normal target dependency and rpath. Both actual native
collation graphs now pass. nm confirms both import ucol_open_78 and
udata_openChoice_78 rather than defining a second implementation in the executable.
The Release and Linux renderer workflows include the collation target; Linux
execution remains unverified.

The genuine retained-endpoint regression now passes both separate processes:
natural Host shutdown and replacement endpoint identity. It creates actual
Hub/UI transports, retains real endpoint references after Application destruction,
checks repeated shutdown and late disconnect handling, and releases genuine
EventReceiver queued strong references at final thread-queue destruction. Native
weak endpoint owners must expire without running queued callbacks. The replacement
case also checks the newer endpoints' singleton, settings and browser callback
stores. No VM, visible windows or desktop platform is used
(candidate-hub-lifetime-accepted.log).

Rejected fixture iterations attempted a second process-wide Application and then
used a live event loop after destroying its queue. Neither is an SDK-supported
test setup. The accepted fixture uses one Application per process and exactly
one final queue destruction; no production restart or event-pumping workaround
was added. The prior genuine hidden-picker crash and its unchanged passing
regression remain independent evidence. Both lifetime and collation gates are
included in the existing Release/Linux renderer workflows. Fresh hosted results,
targeted visible Cocoa acceptance and packaged Designer startup still remain.

The remaining probe callback migration covers power-monitor, locale and menu.
All native JS callbacks now declare GC::captures; borrowed fixture state uses
revocable anchors and traces the actual mutable handler roots. The audit finds
no remaining raw captures in the native test factories. All 26 relevant actual
candidate translation units pass strict compiler syntax checks. Genuine local
Release builds pass for the three probes and the menu offscreen graph; actual
power factory/Host/passive OS queries, locale guards and offscreen Qt menu actions,
queued callbacks, roles and stale-generation teardown pass
(candidate-icu-captures-build.log, candidate-test-captures-syntax.log,
candidate-locale-capture.log, candidate-menu-capture.log). Physical power
transitions and a fresh hosted macOS run remain separate requirements.

For the Linux X11 driver, wait for Openbox's official post-initialization startup
callback before creating the independent non-Qt control client. Its advertised
root property is published earlier than event startup/window management in
Openbox 3.6.1; the observed unmapped client is consistent with an early event
being stranded in its private queue. The existing ten-second bound, fresh-server
requirement and real mapped/decorated control acceptance remain unchanged.
Five non-GUI tests pass: actual marker command/order, missing callback, exited
manager, unmapped client and missing decorations, including process/temp cleanup.
The native Linux Xvfb gate still needs hosted execution after pushing; these
controlled tests do not establish actual X11 acceptance.

After registering the new Hub connection composition guard and non-GUI X11
ordering tests, the complete standalone Release aggregate passes 149/149 in
109.83 seconds (candidate-final-cli-149.log). The actual Ladybird checkout remains
clean at the manifest/gitlink pin. Repository-wide style checks pass for 314 Python,
961 JS/web/config, 579 native and 82 CMake files; the subsequently added native
lifetime fixture and workflow/build integration also pass focused style checks.

### Candidate follow-up: Cocoa acceptance and current Designer shell

Linux CLI run 42892218480 succeeds at exact source abd65d1. Windows, Linux
renderer and macOS package workflows are still running at this checkpoint.
The engine remains a candidate; no new alpha or working Code-OSS is claimed.

One batched local native macOS run passes all four required component gates
(candidate-cocoa-current-acceptance.log): hidden file input, real mouse/key input,
viewport resize, native painted capture, navigation/close, genuine owned renderer
crash reporting, hidden first-mouse/native view recreation and actual AppKit safe
areas, hidden title-bar geometry/recreation and real toolbar pixels, native button
hit targets and HTML drag regions. The captured page was visually inspected.
The accepted Designer was not running before this batch; no unrelated app was closed.

Restaging from clean abd65d1 Release inputs passes strict nested signing. But the
reused Designer assets contained a November 5 Firefly shell: its old smoke code
returned success after three markers and never performed the current native
minimize/HTML-close assertions. The full unchanged acceptance correctly rejects it.
An upstream demo pin alone did not establish current maintained sample files.

Add shared designerShell metadata: six current maintained source hashes, the
deterministic controls overlay and actual final index digest. The builder records
it after bundling; both native packagers verify the actual files against current
owned sources and their record before creating output. The Designer acceptance
driver also rejects a stale shell before launching any server or GUI. Generic
Linux apps with unrelated metadata remain usable. This is stale-artifact detection,
not an authenticated upstream source or native execution certificate.

Ten controlled tests pass, including stale/missing files, source updates,
tampered bytes with a rehashed manifest, index/overlay drift, generic metadata,
both packagers leaving no output on rejection, and Finder metadata preservation.
The actual obsolete local stage is independently rejected. A genuine pinned
Designer rebuild reuses the existing source/dependencies, builds 19 entry points
and five workers, omits 139.4 MiB of module/build payload, and passes all six
current shell hashes and index verification. Source checks exclude only basename
.DS_Store; all other tracked/untracked changes still block. Original Finder data
is preserved in place (candidate-designer-stale-stage.log,
candidate-designer-current-build.log, candidate-designer-shell-tests.log,
candidate-designer-shell-current.log).

The registered firefly-designer-shell CTest passes its ten cases; the previous
complete runtime aggregate remains 149/149. Focused source/CMake formatting and
diff checks pass. The fresh full packaged Designer smoke is still pending at
the shell-guard implementation commit. The packaging guide/sample README also correct stale Linux wording:
the extracted package gate passed at 197cd2a on Ubuntu 24.04 x64 Xvfb/Openbox,
verified again against successful run 42379262065; a newer engine needs new evidence.

The following actual package acceptance closes that pending local gate: clean
d45560e3543d7c7a05822b91db87e104a931f236, Release CLI/renderer, production bundled
Designer, stripped native symbols and strict nested ad-hoc signatures. The full
unchanged native Designer test passes editing/round-trip, Monaco, principal-world
privilege absence, window capability/server security, actual HTML controls,
native minimize and HTML close. Its retained output includes all stage and
required success markers with empty stderr
(candidate-current-packaged-designer-acceptance.log, designer-last-ui.log).
This is actual macOS app execution, not just server or file-existence checks.
Framework is 227,865,540 bytes (217.3 MiB); Designer is 263,643,704 bytes
(251.4 MiB), before archive compression. The staging location was reused and its
Finder metadata restored; superseded package contents were removed. The existing
canonical upstream clone and installed build dependencies were reused.

Site build and validation pass 329 pages and 23,294 local links/resources/fragments
(candidate-designer-shell-site-check.log). Native Linux archive acceptance and
macOS CI remain pending for this candidate; the scoped local macOS result does
not accept Windows or complete the whole project.

Make the shared overlay writer explicitly emit UTF-8/LF bytes on Windows too.
A real file-I/O regression starts from Unicode/CRLF input and checks the final
bytes; the shell suite now passes eleven cases. This keeps the persisted index
digest independent of native text-mode newline conversion. The packaged macOS
index already has those same bytes, so no additional GUI run is needed.
The full test run also exposed the notice test importing an unused re-export from
package_macos after formatting removed it. Import the actual native_package.notices
owner instead; license-copy and tampered-license rejection assertions are unchanged.

### Candidate follow-up: shared Windows socket boundary for OpenSSL

Windows run 42892218435, native job 130096084744 at abd65d1, compiles past the
previous libuv conflict but fails TLS Client.cpp through TrustStore.h:
OpenSSL ssl.h/e_ostime.h imports WinSDK winsock2.h after AK/Time, redeclaring
timeval. The libuv-only boundary did not protect that separate public header path.

Generalize the owned boundary to platform/WindowsSockets.h. On Windows it loads
actual AK time declarations and imports real WinSDK Winsock using the same scoped
dummy_timeval spelling as AK/Windows.h. UV delegates to it, and all eight owned
OpenSSL include groups import it first. No fake time-layout macro, SDK source
edit, cpp-only SystemApi import or new Firefly dependency enters standalone Mite.
The boundary is empty on POSIX. A genuine WinSDK-only sandbox probe keeps its
independent socket import; that narrow exemption does not allow OpenSSL bypasses.

Six additional compile-only SSL header orders join the normal Mite target, along
with the existing three libuv orders. They cover GC/runtime/SSL-first, UV-first,
upstream Windows boundary coexistence and the real public TrustStore header.
Native Windows branches assert actual SDK field/layout, Winsock select/SSL_new
function signatures and macro restoration. Maintained-source guards and five
negative/control cases pass. Windows execution of those ABI assertions remains
pending a fresh clang-cl CI result; local builds do not establish it.

Actual local Release Mite plus all nine compile fixtures build successfully.
The complete runtime aggregate passes 150/150 in 113.09 seconds, including real
crypto/TLS differential checks and the unchanged license-copy negative controls
(candidate-windows-sockets-cli-build.log,
candidate-windows-sockets-cli-150-final.log). The rejected first aggregate retained
149 passing suites and the stale notice-import failure; no runtime assertion was
removed. Actual Firefly/offscreen and safe-storage backend probe build/link pass
(candidate-windows-sockets-firefly-build.log). No extra GUI or OS keychain test
was run for an empty POSIX header branch. Repository-wide style passes 316 Python,
961 JS/web/config, 581 native and 82 CMake files, plus focused checks for the final
overlay/notice changes. Ladybird remains clean at the manifest/gitlink pin.
Fresh hosted Windows and renderer/package acceptance, then master promotion,
remain required. The full product/VS Code/native Windows/release goal stays active.

### Candidate follow-up: public GC headers for standalone consumers

Linux CLI run 42896891703 succeeds at 4b6705a. Windows run 42896891726,
job 130112322078, now compiles and links the complete mite-runtime library,
including the corrected OpenSSL paths. It then fails building the independent
ExecutionContext embedding probe: public Realm/ExecutionContext headers expose
GC::Heap, which now includes pthread.h, while the static LibJS target keeps GC's
usage requirements private. The runtime had received those include directories
through other dependencies; this standalone consumer correctly exposed the gap.

Propagate LibGC through our LibJSStandalone interface, matching upstream's
existing shared-build interface. Its real public LibThreading dependency supplies
the actual Windows pthread include/library requirements. Do not add a fake header,
attach unrelated Mite services to the probe or alter vendor targets/sources.
Actual local Release Mite and the standalone embedding probe build/link, and
its genuine null deallocation/realm ownership/moved-context test passes
(candidate-public-gc-headers-build.log,
candidate-public-gc-headers-acceptance.log). CMake formatting/diff checks pass.
Windows compiler and runtime acceptance still require the next hosted result.
The follow-up comment layout matches the pinned CMake formatter; the interface
dependency and native validation scope are unchanged.

The autoplay feature is being developed separately; its new untracked files are
not part of this engine-fix commit. The engine review branch keeps only the
required update fixes so its platform CI can complete independently.

## 2026-11-10 — Native autoplay media component for Code-OSS

Work continues separately on codex/autoplay-policy-2026-11-10 in the existing
review directory/build trees. The engine CI branch remains independently pushed
through aae76b4; its source is not mixed with this unfinished feature. Master still
uses the accepted engine. No full app/source clone or extra SDK build was added.

Implement a real LibWeb media policy component for the Code-OSS constructor's
user-gesture-required value, plus no-user-gesture-required. Public preference
admission stays rejected until WebAudio and authenticated transport are complete.
ADR 0191 records the exact pinned Chromium reference and remaining requirements;
document-user-activation-required is not implemented by substituting sticky state.

The owned native policy accepts bounded atomic context/generation/value snapshots.
Eight fingerprinted HTMLMediaElement seams preserve every unrelated upstream byte
and all class layouts: initialization/destruction, adoption, play, explicit load,
audible change, trusted native controls and admission before SDK file/allowlist/
sticky bypasses. Element state has no GC edges; actual destruction erases it.

Actual Release LibWeb/module/probe build and twelve no-GUI/no-audio component groups
pass (autoplay-media-native-canonical.log): original unmanaged SDK behavior;
explicit file/HTTP override; transient versus sticky and independent elements;
load/source persistence; video-only mute exception and unmute reblocking; volume
unlock; genuine Page media-menu dispatch; adoption/policy changes; separate
contexts/atomic invalid snapshots; real GC; a real remote-top/local-child graph;
and actual local-opener one-play authorization without a permanent child unlock.
These exercise genuine play Promises and pause state without playable media or
audio output. Native C++ activation is not physical-input or cross-process proof.

The first link correctly exposed missing direct LibURL linkage and an SDK-hidden
control method. Link its actual dependency and dispatch the real exported Page
context-menu path instead of exporting a test shim. Independent headless clients
also reuse native allocator namespaces; the fixture assigns real native IDs using
the SDK reconstruction API. Different BiDi UUIDs deliberately remain. Inspection
found those UUIDs differ from remote top-level IDs, so production policy lookup now
uses canonical native namespace/local IDs consistently. The remote fixture proves
that mapping without simulating IPC. Four source-preservation/drift/refusal tests
and strict compiler checks pass. Ladybird remains unmodified.

Remaining: actual Rust AudioContext gating/lazy graph startup, activation and pending
Promise cleanup, trusted before-script defaults/revocation/frame transport, public
option validation and actual helper/app acceptance. VS Code still stops before a
window at autoplayPolicy; this component alone does not advertise that API.

## 2026-11-10 — Pinned WebAudio Rust build routing

Route the real libweb_bindings_rust crate root and web_audio module through owned,
fingerprinted sources without modifying upstream Cargo manifests, features,
generated bindings or notices. In this commit the AudioContext implementation is
byte-identical to the pinned SDK. Unknown, repeated, missing or drifted modules
fail explicitly. Native compiler argv/exit transport preserves Unicode, empty
arguments and quoting; the existing no-JIT/origin/cache chain remains intact.

An exact generated-root cfg fingerprint follows the origin and no-JIT markers.
Both CPU validators enforce its actual source hash and ordering without relaxing
CPU, warning or frame-pointer requirements. Their six test groups pass. The
native driver test uses real clang++ and rustc for routing/module resolution and
negative checks; it does not claim WebAudio execution or Windows acceptance.

The actual Release libweb_bindings_rust-build target passes
(autoplay-rust-routing-native-build.log). All nineteen emitted Cargo commands
carry the three exact ordered markers and -Dwarnings; the real Cargo dependency
file includes the owned AudioContext leaf, whose bytes equal upstream. The actual
LibWeb/media probe links this Rust output and all twelve media groups pass.
A separate check-firefly-autoplay-rust-routing target makes the source/driver
checks repeatable without coupling standalone Mite to LibWeb.

Pinned Chromium inspection corrects the next implementation step: user-gesture
WebAudio authorization queries transient activation at construction, resume or
source start, only for cross-origin-to-outermost contexts. A generic activation
notification must not unlock pending contexts. ADR 0191 now records that rule and
both independent scheduled-source and buffer-source entry points. Actual lazy
renderer creation, promise cleanup and authenticated public transport remain
unfinished; no public autoplay value is admitted and Code-OSS still cannot start.

## 2026-11-10 — Windows engine test portability after successful native build

The aae76b4 native Windows job compiles and links Mite and runs 147 suites:
138 pass, nine fail. Five failures are fixture portability defects: escaped
Windows paths in the UV guard; Rust-generated forward-slash source paths;
CRLF conversion in Hub source-integrity input; CRLF in the X11 marker fixture;
and Designer byte mutations that accidentally rewrote all line endings.
Preserve exact bytes/path representations instead of relaxing the assertions.

Local UV guard (five cases), Hub integrity, X11 ordering (five cases), Designer
shell (eleven cases), and native Rust no-JIT driver/nested-module/argv checks pass.
No Ninja, desktop or audio tests were run for these fixture-only changes. The
remaining Windows fork/reference pipe, watcher GC and filename failures still
need investigation. Native Windows desktop support remains unfinished.

## 2026-11-10 — Wait for actual native menu dispatch in hosted acceptance

The older macOS Release job at abd65d1 reaches the native menu test after its
preceding runtime/protocol gates pass, then fails the new-window dispatch check.
The same fixture remains in aae76b4. It assumes a fixed 15 ms delay is sufficient
for native dispatch although the quit test already uses bounded observation.
Use a two-second bounded wait for the actual callback count, retain the exact
callback/checked assertions, and report real native generation/signal/delivery
counters on failure. No sleep duration is increased and no success is synthesized.
JS formatting and diff checks pass; hosted native execution is still required.
No local GUI test is launched for this timing-fixture correction.

## 2026-11-10 — Native Windows short-path watcher admission

The hosted Windows fs.watch failure reports ts\initial.txt instead of initial.txt.
Inspection of pinned libuv's Windows backend identifies the actual cause: it
expands a change path to its long spelling but subtracts the original watched
short-path directory length (the hosted temp root includes RUNNER~1).
Expand only native short-name aliases with bounded GetLongPathNameW queries before
uv_fs_event_start. Preserve junction/symlink identity and full recursive filenames;
do not strip basenames or silently rewrite notifications. Translate actual Win32
failures through libuv and fail before allocating an event handle.

Add a real Windows short/long-alias comparison using Win32 paths to the same
directory, actual watchers for both spellings, Unicode root names and recursive
nested-name assertions. Filesystems without distinct 8.3 aliases report that exact
missing test capability. Native Windows execution remains pending hosted CI.
Actual local Release Mite build and complete Node/Mite event watcher comparison
pass (windows-watch-path-build.log, windows-watch-path-local-acceptance.log).
Strict native syntax and UV boundary checks pass. No GUI tests were run.

## Resume

1. Read this file, the specification, and `git status` / recent commits.
2. Initialize dependencies with `git submodule update --init --recursive`.
3. Follow the README build/test instructions, then take the next unchecked slice.
