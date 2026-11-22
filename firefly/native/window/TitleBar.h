/** Native Cocoa caption visibility on real full-size titled windows, not frameless emulation. */
#pragma once

#include <QPoint>
#include <QRect>
#include <optional>

class QWidget;

namespace Firefly {
bool title_bar_available();

// Hidden requires the expanded-client and transparent-titlebar Qt flags set
// before native creation. The owned controller reapplies caption policy on rebind.
bool configure_title_bar(QWidget& window, bool hidden);

bool set_title_bar_button_position(QWidget& window, std::optional<QPoint>);

std::optional<QPoint> title_bar_button_position(QWidget& window);

QRect title_bar_control_bounds(QWidget const& window);
}
