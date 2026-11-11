/** Live-owner admission for main-process native service bindings. */
#pragma once
#include "GCIncludes.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/VM.h>

namespace Firefly {
template <typename T>
JS::ThrowCompletionOr<T*> live_native_capture(JS::VM& vm, Mite::NativeReference<T> const& capture)
{
    if (auto* owner = capture.try_get())
        return owner;
    auto error = JS::Error::create(vm.realm(), "Native service has stopped"_utf16);

    error->define_direct_property("code"_utf16,
        JS::PrimitiveString::create(vm, "ERR_FIREFLY_APP_EXITED"_utf16), JS::default_attributes);

    return JS::throw_completion(error);
}
}
