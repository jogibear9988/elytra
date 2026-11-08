/** Main-only LibJS bindings borrow selected native policy for the application's lifetime. */
#pragma once
#include "State.h"

namespace JS {
class Realm;
class Object;
}

namespace Firefly::GPU {
void install_host(JS::Realm&, JS::Object&, State&);
}
