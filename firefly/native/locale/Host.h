/** The trusted main host supplies readiness and the actual selected engine language. */
#pragma once
#include <QString>
#include <functional>

namespace JS {
class Realm;
class Object;
}

namespace Firefly::Locale {
void install_host(JS::Realm&, JS::Object&, std::function<bool()>, std::function<QString()>);
}
