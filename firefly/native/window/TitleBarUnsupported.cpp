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
}
