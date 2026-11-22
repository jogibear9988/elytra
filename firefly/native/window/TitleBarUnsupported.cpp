/** Ordinary native chrome remains available; custom hidden Cocoa titlebars are unsupported. */
#include "TitleBar.h"

namespace Firefly {
bool title_bar_available()
{
    return false;
}

bool configure_title_bar(QWidget&, bool hidden)
{
    return !hidden;
}

bool set_title_bar_button_position(QWidget&, std::optional<QPoint>)
{
    return false;
}

std::optional<QPoint> title_bar_button_position(QWidget&)
{
    return {};
}

QRect title_bar_control_bounds(QWidget const&)
{
    return {};
}
}
