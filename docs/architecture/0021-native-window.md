# 0021: Reuse the upstream Qt presentation widget in a Firefly-owned window

Status: native macOS window gate implemented; public Firefly API and Mite main
process integration remain outstanding.

## Decision and source evidence

Compile a curated set of Ladybird UI/Qt sources directly from the pinned,
unmodified submodule into our separate window executable. WebContentView already
implements native presentation, input, viewport changes, accessibility, selections
and crash overlays. Its macOS path presents Compositor-provided IOSurfaces through
Core Animation; this gate does not replace the renderer with screenshots or a web
engine from another project. Reuse EventLoopImplementationQt and its event target
to run Core notifiers/timers/IPC on the Qt loop. The existing standalone Mite build
is unaffected. C++ avoids an unnecessary FFI boundary for these C++/Qt callbacks.

Our Application subclass owns QApplication and the Core loop, temporary profile
and host window. It disables browser-process coordination and SQL persistence.
It loads the requested URL rather than showing Ladybird's first-run welcome page,
which upstream prepends for a fresh non-headless profile. It preserves default
helper sandbox flags and installs no Mite host globals in WebContent. The companion
Ladybird development bundle still supplies helper executables and resources.

The upstream Menu.cpp callback assumes a tabbed Ladybird Application instance and
accesses its active tab after certain actions. That static cast is inappropriate
for an independent embedder. Our small ContextMenu.cpp supplies the one toolkit
seam called by WebContentView instead: menu/action snapshots, nested menus, weak
action references and a check of current enabled/visible state before activation.
It does not compile or impersonate Ladybird's browser Application. Live menu
mutation, shortcuts/icons and Electron Menu APIs are not provided by this adapter;
context-menu behavior needs further dedicated tests before a compatibility claim.

The source list and ARC settings live in our CMake adapter. An upstream update may
change these interfaces or dependencies; native build/runtime gates must pass
again after updating the synchronized pin. No sources are copied or patched.
The frontend participates in upstream's Skia confinement check.

## Verification and limits

The native fixture uses Qt-injected mouse press/release and key events that travel
through the actual WebContentView input translation and IPC to LibWeb. DOM text
must acknowledge the click, typed `a` and an 800x500 CSS viewport after resizing.
A QScreen capture of the native window, rather than a renderer screenshot, is
saved and must be visually reviewed. It may have higher pixel dimensions on HiDPI
displays. Navigation to a second local document must execute its script, and an
actual window close must end the loop. A separate process test identifies the
loaded WebContent as a direct OS child, kills that PID and requires a renderer
crash error with exit code 1. Timeout, missing binary or failed capture is failure.
These are toolkit input tests; they do not establish OS input automation, complete
IME/accessibility behavior, all close/beforeunload semantics, or full IPC security.

The target is currently enabled only on macOS. Native Windows/D3D and Linux
presentation dependency wiring and tests remain to be implemented, alongside the
Windows sandbox gate from ADR 0020. No platform support is inferred from Qt APIs
alone. Native packaging/signing and older macOS versions also remain unverified.

The next vertical slice must connect this native view to the actual Mite runtime
and implement the Electron-style app/BrowserWindow lifecycle. This executable is
an internal gate, not a Firefly application or a replacement for that requirement.
Mite's private libuv loop still needs deliberate integration with the GUI loop;
window lifetime must keep the main process alive, and callbacks must enter its
rooted job/microtask queue safely. Preload, contextBridge and renderer IPC remain
separate required security work.
