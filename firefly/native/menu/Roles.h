/** Native macOS menu semantics; ordinary Qt platforms reject these roles explicitly. */
#pragma once
#include <QString>
class QMenu;

namespace Firefly::MenuRoles {
bool available();
bool activation_is_keyboard();
bool dock_available();
bool install_dock(QMenu&);
void clear_dock();
bool bind(QMenu&, QString const& role);
void clear();
bool perform(QString const& role);
}
