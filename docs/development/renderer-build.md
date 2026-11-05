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
