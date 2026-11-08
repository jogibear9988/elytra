/** Real private route Host/LibJS validation, without grants, paths or a renderer. */
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/routes/Host.h"
#include <AK/Array.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>

int main(int argc, char** argv)
{
    Array schemes { Firefly::Protocols::OriginScheme { "app"_string, true, true } };
    if (Firefly::Protocols::install_origin_schemes(schemes).is_error())
        return 1;
    JsonArray policies;
    auto routes = adopt_ref(*new Firefly::Protocols::FileRoutes([](auto) { return false; },
        [](auto, auto, auto) { VERIFY_NOT_REACHED(); },
        [](auto, auto const&, auto const&) { VERIFY_NOT_REACHED(); },
        [&](u64 generation, bool active) {
            JsonObject policy;
            policy.set("generation"sv, String::number(generation));
            policy.set("active"sv, active);
            MUST(policies.append(move(policy)));
        }));
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root native { JS::Object::create(realm, nullptr) };
        Firefly::Protocols::install_file_route_host(realm, *native, routes);
        native->define_native_function(
            realm, "state"_utf16,
            [&, routes](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                JsonObject state;
                state.set("active"sv, routes->intercepts_file());
                state.set("generation"sv, String::number(routes->file_policy_generation()));
                state.set("policies"sv, policies);
                auto text = JsonValue(move(state)).serialized();
                return JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(text.bytes_as_string_view()));
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "collect"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module("protocol-route-test", *native,
            { "protocolRegisterFile", "protocolRegisterResponse", "protocolUnregisterFile",
                "protocolFileReply", "protocolResponseHead", "protocolResponseChunk", "state",
                "collect" });
    };

    hooks.shutdown = [&] { routes->stop(); };
    return Mite::run(argc, argv, hooks);
}
