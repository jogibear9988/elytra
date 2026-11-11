/** Private main-VM menu boundary, shared by the desktop bridge and native fixture. */
#pragma once
#include "ApplicationMenu.h"
#include <LibJS/Runtime/Object.h>

namespace Firefly {
void install_menu_host(JS::Realm&, JS::Object&, ApplicationMenu&, ApplicationMenu* dock = nullptr);
}
