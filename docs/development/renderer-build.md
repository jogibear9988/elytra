# Renderer embedding build

The renderer probe is a separate upstream-root build, injected through CMake's
project-include/deferred-target hooks. It does not modify Ladybird or add
LibWeb, Qt, or Firefly to the standalone Mite graph.

```sh
python3 tooling/bootstrap.py
python3 tooling/build_renderer.py --jobs 4
python3 tooling/test_renderer.py --probe build/renderer/bin/firefly-renderer-probe
# Or build and exercise the renderer together:
cmake --build build/renderer --target check-firefly-renderer --parallel 4
```

The first configure installs the pinned browser dependency manifest, including
its overlay ports, graphics/media stack and Qt. This is substantially larger than
the Mite build. Upstream's native build prerequisites still apply. Use a separate
build directory; do not point it at build/debug or build/dev. The renderer uses
Release without LTO or JS JIT for the initial integration exercise. Its helpers
and resources currently come from the companion Ladybird build/bundle; this is a
source-tree development layout, not a redistributable Firefly package.

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
python3 tooling/test_window.py --probe build/renderer/bin/firefly-window-probe
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
build/renderer/bin/firefly examples/firefly-hello-world/main.cjs

# Install the exact Electron behavioral reference into ignored build/:
python3 tooling/bootstrap_electron.py
cmake --build build/renderer --target check-firefly --parallel 4
```

Firefly runs the real Mite entry module. Both CJS and ESM can import app and
BrowserWindow from firefly. The reference is Electron 45.2.3, with npm dependencies
locked under tooling/electron-reference; it is never a Firefly runtime dependency.
The generated browser+Mite manifest stays outside the upstream checkout. Direct
test invocation accepts --firefly and --electron paths via tooling/test_firefly.py;
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
executables use the development Ladybird build. The normal Firefly app continues
to use the original helper. No distribution bundle is produced.

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
0027–0039. Listener dispatch is guarded before receiver/Event wrapping, with
function/object/getter/proxy lifetime, error privacy, cancelation and live-page
controls. Native listener/signal release checks additionally cover duplicate/capture/once/
abort/explicit removal, GC with a rooted signal, late registration after logical
detach and continuing page signal behavior. Script-valued event-field policies
remain open; the gate does not enable public preload.

The existing listener TU now includes a generated EventTarget.inc whose only source
edits rename the listener/handler free definitions. Bodies/internal calls/licenses
remain intact; the original handler call must still reach the public gate. The
callback hook target runs a preservation/signature regression test. SDK/helper
handler tests cover principal listener replacement, strict false/error behavior and
in-flight lifetime. Native SDK beforeunload tests cover guarded ToString, errors,
last-owner detach and cancellation/returnValue order. Shared native handler storage
and raw callback getters remain unsafe for public preload; this is dispatch-only
coverage, not an isolated handler implementation.
