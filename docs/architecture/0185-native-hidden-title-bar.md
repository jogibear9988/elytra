# ADR 0185: Hidden Cocoa caption with real native controls

Status: implementation; acceptance and remaining limits are recorded in PROGRESS.

Code-OSS requests titleBarStyle:hidden during first-window construction. This
style needs full-size author content while retaining actual macOS window controls;
aliasing it to frame:false would remove those controls and change the contract.

Set Qt's ExpandedClientAreaHint and NoTitleBarBackgroundHint before native window
creation, preserving the title/close/minimize/maximize hints. The hidden style
wins over an explicit frame:false. Cocoa then owns the real titled NSWindow,
full-size content view, control hierarchy and hit testing. A small content-independent
QObject adapter applies NSWindowTitleHidden, checks the actual native style, and
reapplies caption visibility after window/surface lifecycle changes. It does not
replace AppKit controls or mutate process-wide classes.

Qt normally enables WA_ContentsMarginsRespectsSafeArea on top-level widgets. Disable
that automatic inset explicitly for hidden style so the layout and real renderer
viewport cover the full client rectangle underneath the controls. Geometry and
size constraints continue to use native frame margins, with no fixed caption-height
subtraction. Native frame presence and custom HTML dragging are separate concerns:
both frameless and hidden-style windows install the existing bounded drag-region
adapter; normal framed windows retain their native behavior.

Admit default and hidden styles. Unsupported styles and unavailable backends
fail before preload I/O and native allocation. The Cocoa controller also moves
the existing three AppKit buttons as a group. It converts their geometry to
window coordinates, checks both the content area and native control container,
and keeps AppKit's original positions for reset. On recreation it restores the
retiring buttons it still owns and applies the configured position to the new
surface. Geometry outside the control container is rejected so buttons remain
reachable. The same controller reads the real button frames and converts their
union into the full-window content coordinate space. The native drag hit test
excludes that rectangle, independently of author drag regions. The shared
provider is linked by the runtime and standalone chrome probe.

The boolean Cocoa titleBarOverlay option publishes bounded native window
geometry through the existing authenticated helper session. Records are keyed
to the native top-level navigable and removed before the upstream preload
executor. The renderer attaches an EventTarget to the secure top-level
document's Navigator and keeps only weak associations with that document.
Native size and button changes publish new geometry and queue a trusted
event in the same document. Child navigables are excluded.

CSS geometry still needs a per-document seam in LibWeb's Rust style engine.
HiddenInset, custom fullscreen placement and Windows/Linux custom captions
remain open.

Native tests inspect actual NSWindow masks, caption/background properties, standard
buttons and full-size content layout. Qt-local drag events exercise the native
ChromeWindow filter without posting OS input. A separate exact-application renderer
probe presents one brief window to verify actual toolbar CPU pixels, live HTML CSS
drag/no-drag regions and native button hit targets above the full renderer surface.
This is a targeted visible acceptance batch, not a repeated desktop test loop.

References: [Electron custom title bars](https://www.electronjs.org/docs/latest/tutorial/custom-title-bar),
[Qt window flags](https://doc.qt.io/qt-6/qt.html#WindowType-enum),
[Qt 6.11.2 Cocoa window integration](https://github.com/qt/qtbase/blob/v6.11.2/src/plugins/platforms/cocoa/qcocoawindow.mm),
[Qt 6.11.2 QWidget safe-area layout](https://github.com/qt/qtbase/blob/v6.11.2/src/widgets/kernel/qwidget.cpp).
