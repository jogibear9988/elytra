/** Cocoa click-through policy on owned native content views, never a process-wide method swizzle.
 */
#pragma once

class QWidget;

namespace Firefly {
bool first_mouse_available();

// Both widgets must belong to the current Cocoa UI thread and the same native
// window. Reinstalling updates the existing content-owned policy after reparenting.
bool install_first_mouse(QWidget& window, QWidget& content, bool accepts);

void clear_first_mouse(QWidget& content);
}
