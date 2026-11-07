# ADR 0093: Native Linux application integration after renderer probes

Status: scoped native X11 application acceptance passes at `19ad113` in
[run 42316853517](https://github.com/jogibear9988/elytra/actions/runs/42316853517).
General Linux desktop, OS-service completeness and distributable packages remain unfinished.

The Linux renderer path already selects native LibWeb processes, Linux sandbox
sources and an owned resource layout. The application target previously stopped
at an unconditional macOS guard. Connecting Mite to the existing Linux frontend
is required to progress from rendering probes to a usable native application.

Use the existing C++/Qt bridge and the same LibJS-powered Mite runtime. Introduce
one platform service installer, keeping platform branches out of BrowserWindow
and IPC dispatch. The macOS installer forwards to its existing native display/
theme services and owner-scoped watchers. Linux selects the actual X11 clipboard
and checked window geometry/centering. This direct integration needs no new FFI
boundary or duplicate JavaScript runtime.

The first Linux application slice deliberately refuses unfinished display/theme
services. Such calls throw the explicit unsupported API code; they do not return
invented identifiers, boolean preferences or successful empty snapshots. Native
theme source getters/setters also refuse while that provider is unavailable.
This is a partial vertical slice; those native OS services remain requirements.

Keep one combined Linux runtime manifest: original renderer graph and exact Qt
provider, plus Mite's existing libuv/llhttp pins. Shared upstream input helpers
prevent a circular dependency between the renderer and full runtime generators.
Check manifest identity at configure/build time; do not alter Ladybird source.
The runtime helper uses upstream's actual Linux sandbox and share/Lagom layout.
macOS bundle paths and platform providers remain available through the same seam.

The Linux app requires an explicit xcb environment and DISPLAY before constructing
the GUI. Missing-display and Wayland startup fail. Help and the environment probe
remain non-GUI operations. Local macOS compilation exercises the actual Linux
binding translation unit, but cannot certify native Linux runtime behavior.

Hosted acceptance must first pass every existing renderer/helper/window/isolation
gate, then compile/run the actual Mite-driven application on its own Xvfb server.
The app serves HTTP assets, uses early isolated preload/contextBridge IPC and
native clipboard, and keeps its real renderer live for an independent /proc
observer. Require native parent/PID identity and measured seccomp/no-new-privileges
state before responding to the main process. A JS success marker alone is not
accepted as proof of sandbox activation. Do not equate activation with complete
containment or broad Electron compatibility.

Linux package layout/signing, further native OS services, physical displays,
Wayland, full Linux Electron comparisons and native Windows integration remain
unfinished. The [development guide](../development/linux-renderer.md#experimental-mite-driven-x11-application)
records exact commands, evidence boundaries and current acceptance status.
