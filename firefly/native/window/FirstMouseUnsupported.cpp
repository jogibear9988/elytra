/** No verified first-mouse backend exists on other platforms. */
#include "FirstMouse.h"

namespace Firefly {
bool first_mouse_available()
{
    return false;
}

bool install_first_mouse(QWidget&, QWidget&, bool)
{
    return false;
}

void clear_first_mouse(QWidget&)
{
}

}
