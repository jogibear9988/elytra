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

This builds and runs a real, synchronous LibWeb/LibJS realm test without opening
a window. It checks shared DOM data with separate globals/prototypes/expandos,
selected constructor paths, GC and deferred owner teardown. It also tests the
internal BridgeData snapshot adapter, including cyclic data, frozen target-realm
containers, trap-free rejection and copy budgets (ADR 0028). It does not enable
public preload or contextBridge. See ADR 0026 for ownership and the remaining
async/membrane/IPC gates. The same gate now tests synchronous data-only function
endpoints, error sanitation, reentry and in-flight revocation (ADR 0029).

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
across 70 cleanup cycles and rejected missing/duplicate entry tokens. A missing executable or failed
fixture fails the gate. Only a compiled harmless script is installed: this is not
public preload/contextBridge, async revocation or validated IPC. See ADR 0027.
