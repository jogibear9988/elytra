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
