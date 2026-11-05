# 0020: Isolated LibWebView embedding probe

Status: headless build/runtime gate verified on native macOS ARM64; no public
Firefly API compatibility claim yet.

## Source evidence at the pinned Ladybird revision

The unmodified source separates the UI-side LibWebView::Application and
ViewImplementation from Services/WebContent, RequestServer and Compositor.
HeadlessWebView supplies an existing view suitable for text/layout/screenshot
integration checks. UI/Qt/WebContentView is a QWidget/QRhiWidget-backed
ViewImplementation accepting a generic QWidget parent; its native presentation
paths use IOSurface on macOS, D3D11 on Windows, and Vulkan/Wayland on Linux.
UI/Qt/EventLoopImplementationQt adapts Core notifiers, timers and process exits to
Qt. It is an integration candidate, not proof that embedding or native Windows
already works in Firefly.

Services/WebContent/CMakeLists.txt selects RendererSandboxMacOS.cpp or
RendererSandboxLinux.cpp on those systems, and RendererSandboxUnimplemented.cpp
elsewhere. The latter currently returns success without enforcing an OS sandbox.
LibIPC has a native Windows named-pipe/socket transport with handle-broker tokens
and size/attachment checks; LibCore also has a Windows event loop. These are useful
building blocks, not a completed Windows security boundary.

## Integration decision

Keep the Mite build graph and executable independent. Configure upstream as the
root project in a separate directory and add our executable through
CMAKE_PROJECT_ladybird_INCLUDE plus cmake_language(DEFER include).
This preserves upstream source paths, generators, helper placement, resources,
signing rules and pinned browser dependencies without copying its CMake graph or
patching the submodule. Deferred execution cannot create a subdirectory, so the
target module is included at the root directory's end instead. Development builds
skip install rules: upstream AK's shared-library installation path assumes the
imported mimalloc/simdutf targets are runtime libraries, whereas this vcpkg triplet
provides interface/static targets. This does not establish a distribution layout.
The same upstream integrity guard runs before configure
and as an executable build dependency. The added frontend also participates in
upstream's Skia confinement check so that Skia stays in the Compositor process.

Use C++ for the initial LibWebView adapter because its callbacks, ownership and
process-launch APIs are already C++. A thin Application subclass selects a
headless view, temporary profile, no browser-process coordination and no SQL
profile persistence. It preserves the default helper sandbox flag. Our callbacks drive the existing headless view, wait for load
completion and fail on timeout, renderer crash or screenshot rejection. This is
intended to perform the first actual HTML/CSS/DOM/JS and compositor exercise;
no Mite builtins are installed in WebContent.

This is deliberately a first embedding gate. It does not establish Mite-driven
window lifecycle, preload/IPC/contextBridge isolation or Electron API behavior.
The probe's UI process uses Core's own event loop; integrating Mite's libuv queue
with the eventual window event loop is still required. The companion browser
bundle supplies helper paths/resources in this development layout. Redistributable
packaging and a Firefly-owned native window are separate remaining work.

## Native Windows path and acceptance gates

Use clang-cl and the existing Qt/D3D presentation and IPC adapters as the starting
point, without WSL. Validate a native Mite build and the renderer probe on Windows.
Before exposing renderer content as a supported Firefly application, add an actual
restricted renderer-launch/sandbox implementation in our platform adapter and
verify process lifetime, brokered handle rights, file/network restrictions and
malicious IPC there. Replace/select only our build/launcher adapter; keep Ladybird
source unmodified. A no-op upstream sandbox is not an acceptable production gate.

Next native-window work should reuse upstream presentation and event-loop source
through small build adapters where feasible. QWidget availability or a successful
headless build alone is insufficient evidence for BrowserWindow. Actual rendering,
input, resize, navigation, close and renderer-crash behavior must be observed.

## Required verification

Run the actual probe with a local fixture and assert LibJS changes the DOM while
process/require/Buffer/Mite hooks are absent and node:fs import rejects. Check a
real compositor PNG, then observe distinct WebContent, RequestServer and Compositor
OS children during a live session. Keep the profile temporary and retain default
sandboxing. Review the image, not just the exit status; a PNG file/header alone does not
prove the expected page was painted. This only tests selected capability
absence, not the full sandbox or IPC attack surface.

Native macOS runtime results, native Windows/Linux results, Mite linkage checks
and remaining build failures must be recorded honestly in PROGRESS.md.

## Observed result

The native macOS ARM64 build and all three runtime checks pass. The actual PNG
was visually reviewed, and the navigated WebContent PID matches an observed OS
child. Runtime testing corrected our companion path to TARGET_FILE_DIR (the
upstream parameter is named binary_path but represents a directory), and explicit
LibIPC/LibURL linkage resolves symbols used directly by our frontend. No upstream
source changed. The process observer must cover the full session because a spare
WebContent can launch before the renderer that navigates. Build dependency
warnings about a newer macOS deployment target remain; no older-system or package
support is inferred. Windows/Linux and full sandbox validation are outstanding.
