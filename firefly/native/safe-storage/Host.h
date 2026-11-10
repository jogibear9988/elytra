/** Native checks are independent of the public JS facade; no renderer receives these hooks. */
#pragma once
#include "Backend.h"
#include <LibJS/Runtime/Realm.h>
#include <functional>
#include <memory>

namespace Firefly::SafeStorage {
// Supplied callbacks are trusted native functions; they must not retain JS/GC values.
using Factory = std::function<std::unique_ptr<Backend>()>;
void install_host(JS::Realm&, JS::Object&, std::function<bool()> ready, Factory);
}
