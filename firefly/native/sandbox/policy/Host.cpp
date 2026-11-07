/** No renderer capability is exposed by the app.enableSandbox binding. */
#include "Host.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cstring>

namespace Firefly::SandboxPolicy {
JS::Completion fail(JS::VM& vm, Failure const& failure)
{
    auto error = JS::Error::create(*vm.current_realm(),
        Utf16String::from_utf8(StringView(failure.message, std::strlen(failure.message))));
    error->define_direct_property("code"_utf16,
        JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(failure.code, std::strlen(failure.code)))),
        JS::default_attributes);
    return JS::throw_completion(error);
}

void install_host(
    JS::Realm& realm, JS::Object& host, Policy& policy, std::function<bool()> native_enabled)
{
    host.define_native_function(
        realm, "enableSandbox"_utf16,
        [&policy, native_enabled = std::move(native_enabled)](
            JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (auto failure = policy.enable(native_enabled()); failure.has_value())
                return fail(vm, *failure);
            return JS::js_undefined();
        },
        0, JS::default_attributes);
}
}
