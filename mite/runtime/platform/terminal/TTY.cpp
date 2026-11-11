/** Query the actual borrowed descriptor without opening/closing it or changing terminal modes. */
#include "platform/terminal/TTY.h"
#include "platform/UV.h"
#include "platform/process/InheritedPipe.h"
#include <LibJS/Runtime/VM.h>
#include <cmath>

namespace Mite {
GC::Ref<JS::Object> create_tty_host(JS::Realm& realm)
{
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(
        realm, "isatty"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            if (!value.is_number())
                return JS::Value(false);
            auto number = value.as_double();
            if (!std::isfinite(number) || number < 0 || number > 0x7fffffff
                || std::trunc(number) != number)
                return JS::Value(false);
            auto descriptor = static_cast<int>(number);
#ifdef _WIN32
            // UCRT's ordinary invalid-parameter handler can terminate an embedder
            // for forged descriptors. Scope the probe to this thread/call only.
            ScopedDescriptorProbe guard;
            auto native = _get_osfhandle(descriptor);
            if (native == -1 || native == -2)
                return JS::Value(false);
            DWORD flags {};
            if (!GetHandleInformation(reinterpret_cast<HANDLE>(native), &flags))
                return JS::Value(false);
#endif
            return JS::Value(uv_guess_handle(descriptor) == UV_TTY);
        },
        1, JS::default_attributes);
    return host;
}
}
