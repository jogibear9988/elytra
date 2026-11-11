/** Trusted theme controls and teardown-scoped change delivery. */
#pragma once
#include <LibJS/Runtime/Object.h>
#include <functional>

class QObject;

namespace Firefly {
// Supplied callbacks are trusted native functions; they must not retain JS/GC values.
void install_theme_host(JS::Realm&, JS::Object&, std::function<void()>);

void watch_system_theme(QObject&, std::function<void()>);
}
