/** Bind privileged policy once; caller supplies the actual current native launch state. */
#pragma once
#include "Policy.h"
#include <LibJS/Runtime/Completion.h>
#include <functional>

namespace JS {
class Realm;
class Object;
class VM;
}

namespace Firefly::SandboxPolicy {
JS::Completion fail(JS::VM&, Failure const&);

void install_host(JS::Realm&, JS::Object&, Policy&, std::function<bool()>);
}
