/** Translate native frame/content rectangles without mixing physical pixels and DIP. */
#pragma once
#include <QMargins>
#include <QRect>
#include <optional>

class QWidget;

namespace Firefly {
std::optional<QRect> content_rectangle(QRect requested, QMargins frame, bool content);

QRect window_bounds(QWidget&, bool content);

bool set_window_bounds(QWidget&, QRect, bool content, bool position = true);

bool center_window(QWidget&);
}
