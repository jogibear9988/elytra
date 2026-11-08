/** Internal realm boundary primitives; never expose source errors, frames or thenable hooks. */
#pragma once
#include <AK/Function.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/PromiseCapability.h>

namespace Firefly::Boundary {
GC::Ref<JS::TypeError> error(JS::Realm&, Utf16View);

void settle(JS::PromiseCapability&, JS::Realm&, JS::Value, bool reject);

void reject(JS::PromiseCapability&, JS::Realm&);

bool resolve_safe(JS::Value);

bool caller_is(JS::VM&, JS::Realm&);

JS::ThrowCompletionOr<JS::Value> in_realm(JS::Realm&, Function<JS::ThrowCompletionOr<JS::Value>()>);
}
