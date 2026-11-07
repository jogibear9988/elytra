/** Private trusted main bindings; never installed in preload or page realms. */
#pragma once
#include "Files.h"
#include <LibJS/Runtime/Object.h>

namespace Firefly::Protocols {
void install_file_route_host(JS::Realm&, JS::Object&, NonnullRefPtr<FileRoutes>);
}
