/** The trusted main host supplies readiness and the actual selected engine language. */
#pragma once
#include <QString>
#include <functional>

namespace JS {
class Realm;
class Object;
}

namespace Firefly::Locale {
// Supplied callbacks are trusted native functions; they must not retain JS/GC values.
void install_host(JS::Realm&, JS::Object&, std::function<bool()>, std::function<QString()>);
}
