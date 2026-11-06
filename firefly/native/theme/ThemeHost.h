/** Trusted theme controls and teardown-scoped change delivery. */
#pragma once
#include <LibJS/Runtime/Object.h>
#include <functional>

class QObject;

namespace Firefly {
void install_theme_host(JS::Realm&, JS::Object&, std::function<void()>);

void watch_system_theme(QObject&, std::function<void()>);
}
