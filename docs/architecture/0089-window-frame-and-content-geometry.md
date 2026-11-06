# ADR 0089: Separate outer-frame and content geometry

Status: implemented macOS slice; validation recorded in PROGRESS.md.

## Decision

Keep BrowserWindow geometry in a commented ESM base under firefly/core/window,
preserving EventEmitter inheritance and owner lifetime checks. Native geometry
stays in firefly/native/window. Use Electron names and DIP coordinates shared
with screen; no renderer/main privilege boundary changes are needed.

Use actual QWindow frame geometry/margins for decorated versus client bounds.
Apply checked 64-bit arithmetic before narrowing back to Qt integers. Outer
dimensions subtract real margins; tiny frames retain at least one client DIP.
Content-sized construction and setters add native decoration naturally.

QWidget may defer hidden platform-window resize until show, so update QWidget
and QWindow together before returning. Preserve native default placement when no
constructor coordinates are supplied. Use QWindow x/y signals for move events,
including hidden windows; QWidget move events can be deferred until display.
Scope signal connections to the existing Bridge QObject/queue.

The earlier constructor/getSize/setSize used client dimensions for the outer
API. Correct this observable mismatch consistently rather than preserving an
oversized native frame. Partial outer rectangle updates keep omitted native fields; content bounds
require a complete rectangle, matching the measured Electron signature.
Use native NSWindow.center for macOS centering rather than a mathematical
midpoint (the platform intentionally places windows higher). Explicitly reject
animation, unknown rectangle fields and coordinate overflow.

The [pinned Electron macOS implementation](https://github.com/electron/electron/blob/v45.2.3/shell/browser/native_window_mac.mm)
also delegates Center to NSWindow.center; we use that native API directly.

## Validation boundaries

The pure native probe covers decorated/frameless translation, negative origins,
small-frame minima and integer boundaries without a GUI application. Node/Mite
tests exercise the actual full API factory against a labeled synthetic native
provider. Separate shared Electron/Firefly fixtures use actual hidden native
windows for construction, frame/content setters, positions, centering and move
events. Compare rectangle values independently of JSON key order.

OS constraints, mixed-DPI/physical multi-monitor behavior, fullscreen/maximized
restore/normal bounds, exact event counts and Windows/Linux desktop acceptance
remain open. Ladybird stays unmodified and standalone Mite gains no Qt dependency.
