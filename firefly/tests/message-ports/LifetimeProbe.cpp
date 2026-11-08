/** Run the actual port core with Mite jobs, GC and native BridgeData, without a GUI. */
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/ipc/application/Message.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include "serialization/WireValue.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>

int main(int argc, char** argv)
{
    Mite::HostHooks hooks;
    hooks.initialize = [](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root native { JS::Object::create(realm, nullptr) };
        native->define_native_function(
            realm, "encodeIpc"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto graph = Firefly::BridgeData::capture(
                    vm.argument(0), vm.realm(), Firefly::public_binary_limits());
                if (graph.is_error())
                    return vm.throw_completion<JS::Error>("Unsupported port data graph"_utf16);
                auto wire = graph.value().encode_wire(Firefly::public_binary_limits());
                if (wire.is_error() || wire.value().length() > Firefly::ApplicationIpc::wire_limit)
                    return vm.throw_completion<JS::Error>(
                        "Port data graph exceeds wire budget"_utf16);
                return Firefly::wire_value(vm.realm(), wire.value());
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "decodeIpc"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto wire = Firefly::wire_bytes(vm.argument(0), vm.realm());
                if (wire.is_error())
                    return vm.throw_completion<JS::Error>("Invalid port wire bytes"_utf16);
                auto graph = Firefly::BridgeData::decode_wire(
                    wire.value().view(), Firefly::public_binary_limits());
                if (graph.is_error())
                    return vm.throw_completion<JS::Error>("Invalid port wire graph"_utf16);
                return graph.value().materialize(
                    vm.realm(), {}, Firefly::BridgeData::Materialization::Mutable);
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "collectGarbage"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module(
            "port-lifetime-native", *native, { "encodeIpc", "decodeIpc", "collectGarbage" });
    };

    return Mite::run(argc, argv, hooks);
}
