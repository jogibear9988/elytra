/** Select native service bindings without placing platform branches in the window/IPC bridge. */
#pragma once
#include <AK/Error.h>
#include <functional>

class QObject;

namespace JS {
class Realm;
class Object;
}

namespace Firefly {
ErrorOr<void> verify_desktop_environment();

void install_desktop_services(
    JS::Realm&, JS::Object&, QObject&, std::function<void(char const*)> notify);
}
