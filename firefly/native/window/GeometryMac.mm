/** AppKit's native centering is intentionally above the mathematical vertical midpoint. */
#include "Geometry.h"
#include <AppKit/AppKit.h>
#include <QWidget>

namespace Firefly {
bool center_window(QWidget& widget)
{
    @autoreleasepool {
        auto* view = reinterpret_cast<NSView*>(widget.winId());
        auto* window = view.window;
        if (!window)
            return false;
        [window center];
        return true;
    }
}
}
