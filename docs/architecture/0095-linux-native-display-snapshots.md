# ADR 0095: Native X11 display snapshots before public screen integration

Status: implemented component; native hosted acceptance pending. The public Linux
screen API remains explicitly unsupported until its integration and actual app
acceptance pass. This does not change macOS display behavior or Ladybird sources.

## Context

Linux windows, clipboard and the Mite-driven app have scoped native X11 acceptance.
Public display services are still missing. Qt provides native DIP geometry and a
cursor, but screen names are not promised to be unique native identities. Invented
indices or hashes would make Electron display identity and change comparison
unreliable. The provider must fail when it cannot establish a real output mapping.

## Decision

Use a small C++ Qt/XCB RandR adapter, independent of LibJS/LibWeb and the JS heap.
Require the real xcb GUI application thread and RandR 1.3. Match each Qt screen to
one connected/active native output name, then use that server's real output XID.
Reject missing/ambiguous/reused identities, resource races and reflected displays
that cannot be represented by the existing rotation field. Convert RandR rotation
from counterclockwise into Electron's clockwise degrees.

Names are a checked association, not the identity. The mapping can fail on native
backends that do not expose matching Qt/output names; fail explicitly rather than
silently substitute a synthetic record. XIDs belong to the current X server and
are not persistent hardware identifiers across server restarts.

Use Qt geometry, work area, device scale, refresh rate and cursor in DIP, relative
to the primary display's top-left origin. Other screens may have negative origins.
Read-only operations create no windows and never move the cursor. Return explicit
ERR_FIREFLY_DISPLAY_UNAVAILABLE error records without fabricating fallback data.
Omit internal-panel, color and component-depth metadata until native evidence
supports them; an eDP/LVDS-looking name alone does not prove a physical panel.

Keep this component independently buildable/testable in the native-services graph.
The public VM binding/error translation, owner-scoped notifications and main-process
API acceptance form the next integration step. No new public API is advertised by
component compilation alone.

## Validation and remaining work

The native no-GUI probe must prove explicit refusal under QCoreApplication.
The isolated Xvfb/Openbox gate reads xrandr --current --verbose independently for
output IDs and physical geometry, X11 EWMH properties for work area and Xlib for
pointer position. It compares actual provider results at Qt scale factors 1 and 2.
The window manager must first manage a separate non-Qt control window; no fixed
sleep substitutes for observed readiness. The provider itself creates no window.

This gate currently targets one virtual output at an unrotated zero origin.
Physical/multiple/mixed-DPI outputs, panel identity, rotations, mirroring, hotplug,
work-area notifications, Wayland and Electron comparisons remain required; neither
policy fixtures nor a virtual single-display gate prove those behaviors. Pending
public integration must retain explicit unsupported failures until completed.

Sources: [Qt QScreen](https://doc.qt.io/qt-6/qscreen.html),
[Electron Display](https://www.electronjs.org/docs/latest/api/structures/display),
and the native RandR/XCB headers installed by the pinned Linux build toolchain.
