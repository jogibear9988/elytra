/** Private main-VM binding for the immutable process/native-helper origin policy. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Firefly::Protocols {
void install_registration_host(JS::Realm&, JS::Object&);
}
