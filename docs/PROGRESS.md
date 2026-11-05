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
reporting, and native libuv timers with cancellation and ref/unref support. ESM, JSON and CommonJS file loading are implemented; Node-style package resolution is implemented in part; the events, process, path, buffer, filesystem, stream, string_decoder, net, dns and HTTP server builtins are implemented in part; other built-ins and remaining Firefly services are unimplemented.

Validation: native Debug build with Apple Clang 21 / Rust 1.98.0; all 27 CTest
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

`tooling/test_renderer.py` passes all three real runtime checks: LibWeb text with
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

Next: implement generation-aware function/Promise membranes and validated IPC,
with resource/sender/frame limits, alongside Windows process/sandbox feasibility.
Broaden host APIs and real-package coverage around
those vertical slices. CommonJS named-export detection, synchronous
require(ESM), eval CJS globals and require.resolve options remain unsupported.
Timer refresh, timeout warnings, immediates and process events remain
tracked as unsupported in the compatibility matrix.

## Resume

1. Read this file, the specification, and `git status` / recent commits.
2. Initialize dependencies with `git submodule update --init --recursive`.
3. Follow the README build/test instructions, then take the next unchecked slice.
