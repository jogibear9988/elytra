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

Admit only default/hidden, reject unsupported styles and unavailable backends before
preload I/O/native allocation. This slice does not provide titleBarOverlay, Window
Controls Overlay environment variables, hiddenInset or arbitrary traffic-light
positions. Windows/Linux need their own genuine platform implementations.

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
