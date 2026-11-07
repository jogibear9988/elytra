/** Strict bounded metadata only; application callbacks remain in the main VM. */
#include "Host.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly {
void install_menu_host(JS::Realm& realm, JS::Object& host, ApplicationMenu& menu)
{
    host.define_native_function(
        realm, "menuApply"_utf16,
        [&menu](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return vm.throw_completion<JS::TypeError>("Menu metadata must be a string"_utf16);
            auto view = vm.argument(0).as_string().utf16_string_view();
            if (view.length_in_code_units() > 65536)
                return vm.throw_completion<JS::RangeError>("Menu metadata exceeds budget"_utf16);
            auto bytes = view.to_utf8();
            QString failure;
            if (bytes.is_error()
                || !menu.apply(
                    QByteArray(
                        bytes.value().bytes_as_string_view().characters_without_null_termination(),
                        bytes.value().byte_count()),
                    failure))
                return vm.throw_completion<JS::TypeError>(
                    "Invalid or unsupported native application menu"_utf16);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
}
}
