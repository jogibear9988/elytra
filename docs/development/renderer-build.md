# Renderer embedding build

Historical prototype sections below retain their original scope; current public
[CommonJS preload/contextBridge](../api/context-bridge.md) uses the separate
production helper described at the end of this page.

The renderer probe is a separate upstream-root build, injected through CMake's
project-include/deferred-target hooks. It does not modify Ladybird or add
LibWeb, Qt, or Firefly to the standalone Mite graph.

```sh
python3 tooling/bootstrap.py
python3 tooling/build_renderer.py --jobs 4
python3 tooling/tests/test_renderer.py --probe build/renderer/bin/firefly-renderer-probe
# Or build and exercise the renderer together:
cmake --build build/renderer --target check-firefly-renderer --parallel 4
```

The first configure installs the pinned browser dependency manifest, including
its overlay ports, graphics/media stack and Qt. This is substantially larger than
the Mite build. Upstream's native build prerequisites still apply. Use a separate
build directory; do not point it at build/debug or build/dev. The renderer uses
Release without LTO or JS JIT by default. Pass --lto to enable C++ link-time
optimization explicitly; a compiler/linker without IPO support fails configuration
instead of silently producing an unoptimized comparison. This does not enable Rust
cross-language LTO or the JS JIT. Changing this option rebuilds the affected native
objects; use the same native test gates and compare stripped packages.

The separate --optimize-size option selects the checked C++ size profile (-Oz or
/O1); Rust and prebuilt dependency profiles stay unchanged. The driver verifies
the final optimization flag in generated C++ commands, since upstream also adds
-O3. The [local framework/sample profile passes](package-size.md#checked-c-size-profile-result);
defaults remain off pending hosted profile acceptance. The owned
null-deallocation adapter in [ADR 0058](../architecture/0058-null-safe-execution-context-deallocation.md)
keeps moved-from execution-context cleanup safe under this compilation mode.
Helpers and resources currently come from the companion Ladybird build/bundle; this is a
source-tree development layout. The separate experimental
[release packager](releases.md) stages physical helpers/resources and internal
libraries into relocatable macOS bundles; native Windows/Linux and production
signing/notarization remain open.

The probe defaults to text mode, always selects a temporary profile, and does not
coordinate with an existing browser process. It supports upstream's headless text,
layout and screenshot flags. Text/screenshot runs isolate XDG configuration, and a timed-out test kills its
probe process group on POSIX. Test pages check DOM/LibJS behavior, absence of Mite
host globals, refusal of node:fs imports, a real compositor PNG and separate
WebContent/RequestServer/Compositor child processes. A missing probe is a test
failure. Native Windows process/sandbox validation is explicitly outstanding.

The source added so far is an embedding probe, not a BrowserWindow or Electron
API implementation. Native macOS ARM64 build/runtime checks passed, including visual review of
build/renderer-artifacts/page.png. See docs/PROGRESS.md for the tested host and
limits. Dependency deployment-target warnings preclude claiming support for older
macOS versions without further native tests.

## Native window gate (macOS only)

```sh
python3 tooling/build_renderer.py --target firefly-window-probe
cmake --build build/renderer --target check-firefly-window --parallel 4
# Or exercise an already built executable:
python3 tooling/tests/test_window.py --probe build/renderer/bin/firefly-window-probe
# Open a document manually (30-second probe deadline):
build/renderer/bin/firefly-window-probe file:///absolute/path/to/index.html
```

The test opens real windows briefly. It injects Qt mouse/key events, verifies DOM
results after resize and navigation, closes the window, then kills an observed
WebContent child to check crash handling. It writes the native-window capture to
build/renderer-artifacts/window.png. Visually review that image; PNG metadata
alone cannot establish correct native presentation. The adapter reuses upstream's
actual native WebContentView, not the headless view. Windows/Linux window targets
are not yet implemented. See ADR 0021 for the curated source list, context-menu
limits and the still-required Mite/Firefly API integration.

## Actual Mite-driven Firefly application (macOS development build)

```sh
python3 tooling/build_renderer.py --target firefly --jobs 4
python3 tooling/build_hello.py
build/renderer/bin/firefly build/firefly-hello-world/main.mjs

# Install the exact Electron behavioral reference into ignored build/:
python3 tooling/bootstrap_electron.py
cmake --build build/renderer --target check-firefly --parallel 4
```

Firefly runs the real Mite entry module. Both CJS and ESM can import app and
BrowserWindow from firefly. The reference is Electron 45.2.3, with npm dependencies
locked under tooling/electron-reference; it is never a Firefly runtime dependency.
The generated browser+Mite manifest stays outside the upstream checkout. Direct
test invocation accepts --firefly and --electron paths via tooling/tests/test_firefly.py;
a missing executable fails. FIREFLY_ELECTRON_REFERENCE is the corresponding CMake
cache override. The combined graph also retains check-firefly-renderer and
check-firefly-window. Configure with target firefly before invoking these targets;
configuring a probe alone disables the Mite-driven target.

The functional gate includes native windows and the Electron comparison. Re-run
the visible window gate on an unlocked desktop and review its image. The latest
capture attempt failed because macOS reports the session locked; that remains an
open acceptance gate. See the compatibility matrix and ADR 0024 for the supported
subset, polling driver, process boundaries and remaining API/platform work.

## Internal isolated-world gate

```sh
cmake --build build/renderer --target check-firefly-isolation --parallel 4
```

This builds and runs real LibWeb/LibJS realm tests without opening
a window. It checks shared DOM data with separate globals/prototypes/expandos,
selected constructor paths, GC and deferred owner teardown. It also tests the
internal BridgeData snapshot adapter, including cyclic data, frozen target-realm
containers, trap-free rejection and copy budgets (ADR 0028). It does not enable
public preload or contextBridge. See ADR 0026 for ownership and the remaining
async/membrane/IPC gates. The same gate now tests synchronous data-only function
endpoints, error sanitation, reentry and in-flight revocation (ADR 0029), plus
Promise results, microtask ordering, pending limits and late-result revocation
(ADR 0030). Owned-world Promise job guards also exercise reactions, bound/proxy
callbacks, thenables, async/await, async generators, late resolution, GC and
unregistered/live-world controls (ADR 0031). In-flight job tests also cover last
external owner release, logical/deferred physical detach and independent target
Promise settlement (ADR 0032). Source rejection-tracker tests verify foreign
Promise constructors, later handling, original page tracking and zero source reason
getter calls (ADR 0033). A separate owned LibWeb compile adapter now exercises
Report-style queueMicrotask callbacks, in-flight lifetime and error redaction
(ADR 0034). After changing this common library seam, also run check-firefly-renderer
and check-firefly to validate unregistered callback behavior. The timer-map
adapter additionally checks native interval/timeout ownership, GC tracing, actual
stop state and detach release (ADR 0035).

## Actual renderer early-world prototype (macOS only)

```sh
cmake --build build/renderer --target check-firefly-renderer-helper --parallel 4
codesign --verify --strict build/renderer/FireflyHelper.app/Contents/MacOS/WebContent
```

Use the existing configured renderer graph. This builds a separate sandboxed
WebContent helper with the original bootstrap/service source and our compile
adapters. Resources are physically staged in FireflyHelper.app; other service
executables use the development Ladybird build. This diagnostic helper is never
selected by the normal Firefly app or packager. This target produces no distribution bundle.

The gate checks the original helper as a negative control, first author-script
ordering, isolated globals, frozen data-only copies, srcdoc frames, matching
helper PIDs, synchronous function calls and revoked removed-frame references
across 70 cleanup cycles, Promise fulfillment/rejection, retained removed-frame
Promise rejection, blocked removed-frame Promise jobs, self-removing frames during
reaction/await/generator jobs, source rejection isolation with page reject/handle
controls, queueMicrotask lifetime/error routing and rejected missing/duplicate entry
tokens. The upstream-helper control shares the adapted LibWeb but registers no
owned worlds; its delegation behavior is checked separately. Native timer cleanup
also requires a source interval to stop while an existing page interval and the
document remain live. A missing executable
or failed fixture fails the gate. Only compiled harmless scripts are installed;
public preload/contextBridge, complete source-job revocation and validated IPC
remain open. MutationObserver coverage additionally checks source wrapper/prototype
identity despite page poisoning, explicit disconnect/re-observe accounting, native
node/record cleanup and continued page delivery after source detach. See ADRs
0027–0046. Listener dispatch is guarded before receiver/Event wrapping, with
function/object/getter/proxy lifetime, error privacy, cancelation and live-page
controls. Native listener/signal release checks additionally cover duplicate/capture/once/
abort/explicit removal, GC with a rooted signal, late registration after logical
detach and continuing page signal behavior. Script-valued event-field policies
remain open; the gate does not enable public preload.

The existing listener TU includes a generated EventTarget.inc adapting only
explicit free entry names, four handler-map addresses, GC tracing, storage-owner
listener realm and principal document scope entry. All other upstream bytes remain
intact; the preservation test reverses every declared edit and rejects seam drift.
Handler SDK/helper coverage includes source maps with private getter identity,
page/two-world order and independent cleanup, principal inline text attributes,
full native erase/rearm, detached callback-cycle GC, foreign callbacks and native
beforeunload conversion. This does not enable public preload or establish a general
membrane for script-valued event fields and other callbacks.

CustomEvent native/generated adapters preserve all bytes outside constructor/init/
trace/getter seams and retain bindgen output ordering. The custom-event hook target
checks source preservation/drift. SDK/helper tests require frozen bounded copies,
cache identity/reset, no getter/proxy/function leaks, writing-world provenance,
writer GC/revocation and target-cache release. Same-world/page controls remain
ordinary browser behavior. Original native payload retention and other event fields
remain open; public preload is still rejected.

ErrorEvent native/generated adapters share the EventValues cache with a protected
special onerror fifth-argument path in EventTarget. Both event constructor bindings
now record JS writer realms explicitly; native construction has unknown origin.
The error-event hook target checks exact seams and byte preservation. SDK/helper
checks cover getter/argument identity, reverse data, function/accessor rejection,
reader stack redaction and native page errors during a Source dispatch. Cross-world
Error instances and other non-data payloads remain unsupported.

MessageEvent data has native tracing/init and generated constructor/getter adapters,
with exact source/preservation tests. SDK checks cover data snapshots, cache/GC,
init no-op/reset and basic origin/source/empty ports behavior. The helper exercises
actual two-way postMessage, DataCloneError for functions, source detach with queued
page delivery and source-listener cancellation. Native messaging stays upstream;
full structured-clone/port parity and IPC sender validation are not claimed.

Principal callback context tests additionally check function/object/proxy listeners,
handlers, onerror and non-Promise Report callbacks entered beneath a Source caller.
Page stacks/jobs/timers must remain independent; Source locals must survive saved-
stack GC and last-owner detach until their caller unwinds. EventTarget's generator
now renames three free definitions, including the exception reporter, while internal
calls retain the guarded public names (ADR 0044). Other WebIDL modes are not covered.

NodeFilterAdapter.cpp substitutes one native NodeFilter TU. Its generated include
renames only accept_node's definition, keeping internal traversal calls guarded.
The node-filter hook target checks byte preservation and missing/duplicate entries.
SDK/helper tests cover callback Node worlds, receiver, getter/proxy lookup, ToInt32,
private errors, revocation before wrapping, last-owner/GC and frame removal during
conversion. Principal getter/body/conversion jobs remain independent of Source.
The raw .filter property boundary is still open (ADR 0045).

NodeIteratorBindingAdapter.cpp and TreeWalkerBindingAdapter.cpp retain original
bindgen dependencies and route only filter_getter returns. NodeFilter's constructor
notification supplies weak provenance to WorldJobGuard. The traversal-filter hook
target checks both generated sources for byte preservation and getter/return drift;
the native hook additionally checks the constructor seam. SDK/helper getter tests
cover own/page identity, null, cross-world denial without traps, callable/object
variants, writer GC and weak TreeWalker/filter collection (ADR 0046). Native
NodeIterator document retention is not a metadata reclamation claim.


## Production application preload helper

Building firefly also builds FireflyRuntimeWebContent, staged at
build/renderer/FireflyRuntimeHelper.app/Contents/MacOS/WebContent with physical
resources. It shares the narrow bootstrap/session adapters, but runs ProductSession
instead of the compiled diagnostic probes. Development helper siblings can be
symlinks; the packager copies physical executables and preserves helper entitlements.

check-firefly now runs tooling/tests/test_public_preload.py against the actual app and
pinned Electron. The release driver repeats that suite against the packaged
Firefly executable before archiving. The hello-world app has a CommonJS preload;
its published desktop API has no main-process IPC privileges. See the
[contract](../api/context-bridge.md) and [ADR 0068](../architecture/0068-public-commonjs-preload.md)
for limits, failure behavior and the pinned Qt preparation hook. Native
Windows/Linux renderer/package acceptance and broader callback parity remain open.

## Internal IPC wire-data gate

check-firefly-isolation now also runs check-firefly-wire. Its native producer and
consumer execute in separate processes: the producer VM is destroyed before the
consumer constructs its own JS objects. SDK tests additionally cover malformed
wire graphs, exact bounds and frozen/mutable reconstruction. Run it alone with:

```sh
cmake --build build/renderer --target check-firefly-wire --parallel 3
```

This is data serialization evidence, not public IPC. Request routing, sender/frame/
document authentication, pending-work budgets and navigation/crash revocation still
need integration. See [ADR 0070](../architecture/0070-bounded-ipc-wire-data.md).
