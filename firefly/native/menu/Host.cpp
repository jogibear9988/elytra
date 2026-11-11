/** Strict bounded metadata only; application callbacks remain in the main VM. */
#include "Host.h"
#include "../platform/GCIncludes.h"
#include "../platform/NativeCaptures.h"
#include "Roles.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly {
void install_menu_host(JS::Realm& realm, JS::Object& host, ApplicationMenu& menu)
{
    host.define_native_function(
        realm, "menuRoleAvailable"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return vm.throw_completion<JS::TypeError>("Menu role must be a string"_utf16);
            auto name = vm.argument(0).as_string().utf16_string_view();
            return JS::Value(MenuRoles::available()
                && (name == u"hide"sv || name == u"hideOthers"sv || name == u"unhide"sv
                    || name == u"front"sv || name == u"window"sv || name == u"help"sv
                    || name == u"services"sv));
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "menuRole"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return vm.throw_completion<JS::TypeError>("Menu role must be a string"_utf16);
            auto name = vm.argument(0).as_string().utf16_string_view();
            if (name.length_in_code_units() > 32
                || !MenuRoles::perform(
                    QString::fromUtf16(name.utf16_span().data(), name.length_in_code_units()))) {
                auto error = JS::Error::create(vm.realm(), u"Native menu role is unavailable"sv);
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, u"ERR_FIREFLY_UNSUPPORTED_API"sv),
                    JS::default_attributes);
                return JS::throw_completion(error);
            }
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(realm, "menuApply"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::native_reference(menu)),
            [](auto& menu_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& menu = *TRY(Firefly::live_native_capture(vm, menu_capture));
                if (!vm.argument(0).is_string())
                    return vm.throw_completion<JS::TypeError>(
                        "Menu metadata must be a string"_utf16);
                auto view = vm.argument(0).as_string().utf16_string_view();
                if (view.length_in_code_units() > 65536)
                    return vm.throw_completion<JS::RangeError>(
                        "Menu metadata exceeds budget"_utf16);
                auto bytes = view.to_utf8();
                QString failure;
                if (bytes.is_error()
                    || !menu.apply(QByteArray(bytes.value()
                                                  .bytes_as_string_view()
                                                  .characters_without_null_termination(),
                                       bytes.value().byte_count()),
                        failure))
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid or unsupported native application menu"_utf16);
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
}
}
