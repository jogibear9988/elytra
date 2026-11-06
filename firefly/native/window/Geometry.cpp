/** Read real frame margins, including for hidden native windows; no guessed title-bar height. */
#include "Geometry.h"
#include <QWidget>
#include <QWindow>

namespace Firefly {
QRect window_bounds(QWidget& widget, bool content)
{
    widget.winId(); // Materialize native decorations without showing/activating the window.
    auto* window = widget.windowHandle();
    return content ? window->geometry() : window->frameGeometry();
}

bool set_window_bounds(QWidget& widget, QRect bounds, bool content, bool position)
{
    widget.winId();
    auto* window = widget.windowHandle();
    auto margins = window->frameMargins();
    auto rect = content_rectangle(bounds, margins, content);
    if (!rect)
        return false;
    widget.resize(rect->size());
    // QWidget can defer resizing a hidden platform window until show(). Keep the
    // native QWindow in sync so immediate hidden-window queries include the frame.
    window->resize(rect->size());
    if (position)
        window->setFramePosition(rect->topLeft() - QPoint(margins.left(), margins.top()));
    return true;
}
}
