/** Native path bindings belong to the main process only. */
#pragma once

namespace JS {
class Realm;
class Object;
}

namespace Firefly::AppPaths {
class Paths;
void install_host(JS::Realm&, JS::Object&, Paths&);
}
