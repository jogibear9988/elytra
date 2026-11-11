/** Install the trusted inherited-channel facade in a real LibJS utility process. */
#include "Bootstrap.h"
#include "../net/Host.h"
#include "Host.h"
#include "UtilityNetSource.h"
#include "UtilityParentSource.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "platform/UV.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include "serialization/WireValue.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace Firefly::Utility {
int run(int argc, char** argv, bool electron_compat)
{
    // Check before Mite/libuv creates its own wakeup descriptors. Otherwise an
    // absent fd 3 could be reused internally and mistaken for the parent pipe.
    if (argc < 3 || uv_guess_handle(3) != UV_NAMED_PIPE) {
        fputs("Missing inherited utility channel or entry\n", stderr);
        return 1;
    }
    // Strip the executable-owned mode switch; modulePath remains argv[1].
    std::vector<char*> arguments { argv[0] };
    for (int index = 2; index < argc; ++index)
        arguments.push_back(argv[index]);
    arguments.push_back(nullptr);
    auto diagnose = [](char const* phase) {
        if (std::getenv("FIREFLY_IPC_BYTES_DIAGNOSTIC"))
            fprintf(stderr, "FIREFLY_UTILITY_PHASE pid=%d phase=%s\n",
                static_cast<int>(uv_os_getpid()), phase);
    };

    Mite::HostHooks hooks;
    hooks.allow_process_signals = true;
    std::shared_ptr<NetworkState> network_state;
    GC::Root<JS::Object> network_service;
    JS::VM* owner_vm {};
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& jobs) -> JS::ThrowCompletionOr<void> {
        diagnose("initialize-begin");
        owner_vm = &vm;
        GC::Root native { TRY(create_parent_host(vm, realm, jobs, 3)) };
        network_state = Network::install_host(realm, *native);
        GC::Root transport { TRY(modules.web_transport()) };
        auto net_factory = TRY(modules.run_eval(FIREFLY_UTILITY_NET_SOURCE));
        GC::Root net_factory_root { net_factory.as_object() };
        auto service
            = TRY(JS::call(vm, net_factory, JS::js_undefined(), native.ptr(), transport.ptr()));
        VERIFY(service.is_object());
        network_service = service.as_object();
        GC::Root exports { JS::Object::create(realm, nullptr) };
        exports->define_direct_property(
            "net"_utf16, TRY(network_service->get("net"_utf16)), JS::default_attributes);
        TRY(modules.register_host_module("firefly", *exports, { "net" }));
        if (electron_compat)
            TRY(modules.register_host_module("electron", *exports, { "net" }));
        GC::Root codec { JS::Object::create(realm, nullptr) };
        codec->define_native_function(
            realm, "encodeIpc"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto graph
                    = BridgeData::capture(vm.argument(0), vm.realm(), public_binary_limits());
                if (graph.is_error())
                    return vm.throw_completion<JS::TypeError>("Unsupported utility message"_utf16);
                auto wire = graph.value().encode_wire(public_binary_limits());
                if (wire.is_error() || wire.value().length() > 128 * 1024)
                    return vm.throw_completion<JS::RangeError>(
                        "Utility message exceeds budget"_utf16);
                return wire_value(vm.realm(), wire.value());
            },
            1, JS::default_attributes);
        codec->define_native_function(
            realm, "decodeIpc"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto wire = wire_bytes(vm.argument(0), vm.realm());
                if (wire.is_error())
                    return vm.throw_completion<JS::TypeError>("Invalid utility wire bytes"_utf16);
                auto graph = BridgeData::decode_wire(wire.value().view(), public_binary_limits());
                if (graph.is_error())
                    return vm.throw_completion<JS::TypeError>("Invalid utility wire graph"_utf16);
                return graph.value().materialize(
                    vm.realm(), {}, BridgeData::Materialization::Mutable);
            },
            1, JS::default_attributes);
        auto factory = TRY(modules.run_eval(FIREFLY_UTILITY_PARENT_SOURCE));
        GC::Root factory_root { factory.as_object() };
        GC::Root events { TRY(modules.builtin_exports("events")) };
        auto parent = TRY(JS::call(vm, factory, JS::js_undefined(), JS::Value(native.ptr()),
            TRY(events->get("EventEmitter"_utf16)), JS::Value(codec.ptr())));
        auto process = TRY(realm.global_object().get("process"_utf16));
        process.as_object().define_direct_property(
            "parentPort"_utf16, parent, JS::default_attributes);
        process.as_object().define_direct_property(
            "type"_utf16, JS::PrimitiveString::create(vm, "utility"_utf16), JS::default_attributes);
        process.as_object().define_direct_property(
            "execArgv"_utf16, TRY(JS::Array::create(realm, 0)), JS::default_attributes);
        diagnose("parent-installed");
        return {};
    };

    hooks.shutdown = [&] {
        diagnose("shutdown");
        // Revoke HTTP bodies while the realm is alive, then retire passive OS state.
        if (network_service && owner_vm) {
            auto stop = network_service->get("stop"_utf16);
            if (!stop.is_error()) {
                auto completion = JS::call(*owner_vm, stop.value(), network_service.ptr());
                if (completion.is_error())
                    fputs("Utility network shutdown failed\n", stderr);
            }
        }

        network_service = nullptr;
        if (network_state)
            network_state->stop();
        network_state.reset();
        owner_vm = nullptr;
    };

    return Mite::run(static_cast<int>(arguments.size()) - 1, arguments.data(), hooks);
}
}
