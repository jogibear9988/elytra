/** Native checks are independent of the public JS facade; no renderer receives these hooks. */
#pragma once
#include "Backend.h"
#include <LibJS/Runtime/Realm.h>
#include <functional>
#include <memory>

namespace Firefly::SafeStorage {
using Factory = std::function<std::unique_ptr<Backend>()>;
void install_host(JS::Realm&, JS::Object&, std::function<bool()> ready, Factory);
}
