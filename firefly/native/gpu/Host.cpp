/** Synchronous policy snapshots hold no JS callback roots and never query a guessed GPU backend. */
#include "Host.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly::GPU {
void install_host(JS::Realm& realm, JS::Object& host, State& state)
{
    host.define_native_function(
        realm, "gpuFeatureStatus"_utf16,
        [&state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto snapshot = state.snapshot();
            return JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(snapshot.data(), snapshot.size())));
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "gpuDisableHardwareAcceleration"_utf16,
        [&state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!state.request_cpu()) {
                auto error
                    = JS::Error::create(vm.realm(), "Hardware acceleration policy is sealed"_utf16);
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, "ERR_FIREFLY_GPU_TOO_LATE"_utf16),
                    JS::default_attributes);
                return JS::throw_completion(error);
            }

            return JS::js_undefined();
        },
        0, JS::default_attributes);
}
}
