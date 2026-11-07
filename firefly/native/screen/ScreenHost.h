/** Trusted VM bindings and teardown-scoped display change notifications. */
#pragma once
#include <functional>

class QObject;

namespace JS {
class Realm;
class Object;
}

namespace Firefly {
void install_screen_host(JS::Realm&, JS::Object&);

void watch_screen_changes(QObject&, std::function<void()>);
}
