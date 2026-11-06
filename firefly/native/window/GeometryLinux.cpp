/** Center the actual outer frame in the window's current screen work area, in DIP. */
#include "Geometry.h"
#include <QScreen>
#include <QWidget>
#include <QWindow>

namespace Firefly {
bool center_window(QWidget& widget)
{
    widget.winId();
    auto* window = widget.windowHandle();
    auto* screen = window->screen();
    if (!screen)
        return false;
    auto work = screen->availableGeometry();
    auto frame = window->frameGeometry();
    // Do not resize oversized windows or assume symmetric native decorations.
    window->setFramePosition(work.topLeft()
        + QPoint((work.width() - frame.width()) / 2, (work.height() - frame.height()) / 2));
    return true;
}
}
