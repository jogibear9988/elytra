/** No substitute registration or fake application hiding on unsupported platforms. */
#include "Roles.h"

namespace Firefly::MenuRoles {
bool available()
{
    return false;
}

bool activation_is_keyboard()
{
    return false;
}

bool dock_available()
{
    return false;
}

bool install_dock(QMenu&)
{
    return false;
}

void clear_dock()
{
}

bool bind(QMenu&, QString const&)
{
    return false;
}

void clear()
{
}

bool perform(QString const&)
{
    return false;
}
}
