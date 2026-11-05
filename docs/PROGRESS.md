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

## Resume

1. Read this file, the specification, and `git status` / recent commits.
2. Initialize dependencies with `git submodule update --init --recursive`.
3. Follow the README build/test instructions, then take the next unchecked slice.
