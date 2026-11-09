# ADR 0184: Per-content Cocoa first-click policy

Status: implementation; validation and remaining limits are recorded in PROGRESS.

Code-OSS supplies acceptFirstMouse during first-window construction. Qt 6.11.2's
actual QNSView acceptsFirstMouse callback returns YES for an ordinary input view;
the old WA_MacNoClickThrough attribute has been removed. Merely accepting the
option would leave false and Electron's default false incorrect.

Apply a scoped Objective-C runtime subclass to each owned native content view,
with per-instance policy metadata. A false policy returns NO; a true policy calls
the original implementation directly to preserve Qt's platform/input-transparency
checks. Other Qt views retain their classes and method implementations. No global
method exchange, Qt private header or Ladybird patch is required. The small class
cache is bounded; registered classes live for the process lifetime.

A content-owned QObject controls the binding, observes native surface and widget
lifetime changes, and uses weak NSView references. Rebinding restores the previous
class only while it is still our installed class. Foreign class mutations must
not be overwritten. Child attachment inherits the current host's policy; detach
revokes it, and failed attachment restores the previous owner or closes the child
if native rollback fails.

Native capability admission precedes preload I/O/window creation. Explicit requests
on non-Cocoa backends reject as unsupported. Cocoa omission installs false; a
headless omitted option has no OS first-click behavior to enforce. This does not
claim Linux/Windows click-through compatibility.

The accompanying experimentalDarkMode handling is explicitly legacy compatibility,
not a fabricated theme control. The complete Electron 45.2.3 source archive has no
consumer of that option. Known boolean values can therefore be accepted without
changing appearance, while unknown options continue to reject.

References: [Qt 6.11.2 QNSView mouse handling](https://github.com/qt/qtbase/blob/v6.11.2/src/plugins/platforms/cocoa/qnsview_mouse.mm),
[Electron 45.2.3 native macOS windows](https://github.com/electron/electron/blob/v45.2.3/shell/browser/native_window_mac.mm),
[Apple NSView first-mouse contract](https://developer.apple.com/documentation/appkit/nsview/acceptsfirstmouse(for:)).
