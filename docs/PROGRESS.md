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

## Current work

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

## Resume

1. Read this file, the specification, and `git status` / recent commits.
2. Initialize dependencies with `git submodule update --init --recursive`.
3. Follow the README build/test instructions, then take the next unchecked slice.
