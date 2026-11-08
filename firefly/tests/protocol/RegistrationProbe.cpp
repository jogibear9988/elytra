/** Run public ESM policy against the actual native binding/URL table, without Qt or LibWeb. */
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/protocol/OriginConfig.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/RegistrationHost.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibURL/Parser.h>

int main(int argc, char** argv)
{
    Mite::HostHooks hooks;
    hooks.initialize = [](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root native { JS::Object::create(realm, nullptr) };
        Firefly::Protocols::install_registration_host(realm, *native);
        native->define_native_function(
            realm, "configuration"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto config = Firefly::Protocols::configuration_for_helper();
                if (!config.has_value())
                    return JS::js_null();
                return JS::PrimitiveString::create(vm, Utf16String::from_utf8(*config));
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "seal"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                (void)Firefly::Protocols::configuration_for_helper_launch();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "origin"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto input = TRY(vm.argument(0).to_utf16_string(vm)).to_utf8();
                auto url = URL::Parser::basic_parse(input);
                if (!url.has_value())
                    return vm.throw_completion<JS::TypeError>("Invalid probe URL"_utf16);
                return JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(url->origin().serialize()));
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "secure"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto input = TRY(vm.argument(0).to_utf16_string(vm)).to_utf8();
                auto url = URL::Parser::basic_parse(input);
                if (!url.has_value())
                    return vm.throw_completion<JS::TypeError>("Invalid probe URL"_utf16);
                return JS::Value(Firefly::Protocols::is_secure_origin(url->origin()));
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "cors"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto name = TRY(vm.argument(0).to_utf16_string(vm)).to_utf8();
                return JS::Value(Firefly::Protocols::is_cors_scheme(name));
            },
            1, JS::default_attributes);
        return modules.register_host_module("registration-test", *native,
            { "registerSchemes", "configuration", "seal", "origin", "secure", "cors" });
    };

    return Mite::run(argc, argv, hooks);
}
