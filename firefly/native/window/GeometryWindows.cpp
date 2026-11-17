/** Move the actual native frame within the current Windows work area, in Qt DIP. */
#include "Geometry.h"
#include <QScreen>
#include <QWidget>
#include <QWindow>

namespace Firefly {
bool center_window(QWidget& widget)
{
    widget.winId();
    auto* window = widget.windowHandle();
    if (!window || !window->screen())
        return false;
    auto work = window->screen()->availableGeometry();
    auto frame = window->frameGeometry();
    window->setFramePosition(work.topLeft()
        + QPoint((work.width() - frame.width()) / 2, (work.height() - frame.height()) / 2));
    return true;
}
}
