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
Mite is a working native LibJS runtime; the current macOS Release CLI has 65
passing CTests, and native Linux/Windows CLI baselines have passed hosted CI.
Each new change still needs its own fresh platform acceptance. Mite remains
independent of Firefly, LibWeb and graphical libraries.

Firefly has a partial working macOS desktop runtime and an existing macOS ARM64
alpha release. At 19ad113 the native Linux renderer/helper/window, independent
X11 geometry/clipboard and relocated public application gates pass. This is an
experimental Ubuntu 24.04 x64 Xvfb/Openbox slice, not broad Linux desktop/package
acceptance. Linux screen/theme/distributables and native Windows desktop remain
required and unfinished.
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

## Resume

1. Read this file, the specification, and `git status` / recent commits.
2. Initialize dependencies with `git submodule update --init --recursive`.
3. Follow the README build/test instructions, then take the next unchecked slice.
