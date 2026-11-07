/** Private main-VM bindings for opt-in local native crash diagnostics. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Firefly::CrashReporting {
void install_host(JS::Realm&, JS::Object&);
}
