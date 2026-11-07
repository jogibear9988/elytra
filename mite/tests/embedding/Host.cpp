#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>

int main(int argc, char** argv)
{
    GC::Root<JS::FunctionObject> pulse;
    JS::VM* owner_vm = nullptr;
    bool initialized = false;
    size_t pump_count = 0;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& loop) -> JS::ThrowCompletionOr<void> {
        initialized = true;
        owner_vm = &vm;
        if (auto* path = getenv("MITE_EMBED_FAIL_INIT_WORK_PATH")) {
            TRY(loop.queue_work([path = std::string(path)] { std::ifstream blocked(path); },
                GC::create_function(vm.heap(),
                    []() -> JS::ThrowCompletionOr<JS::Value> { return JS::js_undefined(); })));
            return vm.throw_completion<JS::Error>("Embedded host failed with pending work"_utf16);
        }

        if (getenv("MITE_EMBED_FAIL_INIT"))
            return vm.throw_completion<JS::Error>("Embedded host initialization failed"_utf16);
        auto host = JS::Object::create(realm, nullptr);
        host->define_native_function(
            realm, "onPulse"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Host pulse requires a callback"_utf16);
                pulse = vm.argument(0).as_function();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        host->define_native_function(
            realm, "reenterPump"_utf16,
            [&loop](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!loop.is_busy())
                    return vm.throw_completion<JS::Error>(
                        "External callback must hold a host turn"_utf16);
                return JS::Value(loop.pump_once());
            },
            0, JS::default_attributes);
        host->define_native_function(
            realm, "pumpCount"_utf16,
            [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(static_cast<double>(pump_count));
            },
            0, JS::default_attributes);
        host->define_native_function(
            realm, "activeTimerCount"_utf16,
            [&loop](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                unsigned count = 0;
                uv_walk(
                    &loop.native_loop(),
                    [](uv_handle_t* handle, void* data) {
                        if (handle->type == UV_TIMER && !uv_is_closing(handle)
                            && uv_is_active(handle))
                            ++*static_cast<unsigned*>(data);
                    },
                    &count);
                return JS::Value(count);
            },
            0, JS::default_attributes);
        auto state = JS::Object::create(realm, nullptr);
        state->define_direct_property("count"_utf16, JS::Value(0), JS::default_attributes);
        host->define_direct_property("state"_utf16, state, JS::default_attributes);
        host->define_direct_property("class"_utf16,
            JS::PrimitiveString::create(vm, "reserved-export"_utf16), JS::default_attributes);
        TRY(modules.register_host_module("embedded-host", host, { "onPulse", "state", "class" }));
        TRY(modules.register_host_module("host-default-only", host, {}));
        TRY(modules.register_host_filesystem("original-fs"));
        auto rooted = JS::Object::create(realm, nullptr);
        rooted->define_direct_property("label"_utf16,
            JS::PrimitiveString::create(vm, "registry-root"_utf16), JS::default_attributes);
        TRY(modules.register_host_module("host-rooted", rooted, { "label" }));
        auto check_rejection = [&](JS::ThrowCompletionOr<void> result,
                                   Utf16View code) -> JS::ThrowCompletionOr<void> {
            if (!result.is_error())
                return vm.throw_completion<JS::Error>(
                    "Invalid host registration was accepted"_utf16);
            auto exception = result.error_value();
            if (!exception.is_object())
                return vm.throw_completion<JS::Error>("Invalid host registration error"_utf16);
            auto actual = exception.as_object().get_without_side_effects("code"_utf16);
            if (!actual.is_string() || actual.as_string().utf16_string_view() != code)
                return vm.throw_completion<JS::Error>(
                    "Incorrect host registration error code"_utf16);
            return {};
        };

        auto reject = [&](std::string const& name, std::vector<std::string> names,
                          Utf16View code) -> JS::ThrowCompletionOr<void> {
            return check_rejection(
                modules.register_host_module(name, host, std::move(names)), code);
        };

        for (auto const* name :
            { "", "fs", "node:fs", "embedded/host", "mite-host:other", "bad name", "1host" })
            TRY(reject(name, {}, "ERR_INVALID_ARG_VALUE"_utf16));
        TRY(reject("embedded-host", {}, "ERR_MITE_HOST_MODULE_EXISTS"_utf16));
        for (auto const* name : { "", "fs", "node:fs", "bad/name" })
            TRY(check_rejection(
                modules.register_host_filesystem(name), "ERR_INVALID_ARG_VALUE"_utf16));
        TRY(check_rejection(
            modules.register_host_filesystem("original-fs"), "ERR_MITE_HOST_MODULE_EXISTS"_utf16));
        for (auto const* exported :
            { "", "default", "bad-name", "1value", "x';throw Error('injected')//" })
            TRY(reject("registration-retry", { exported }, "ERR_INVALID_ARG_VALUE"_utf16));
        TRY(reject("registration-retry", { "value", "value" }, "ERR_INVALID_ARG_VALUE"_utf16));
        TRY(modules.register_host_module("registration-retry", host, {}));
        realm.global_object().define_native_function(
            realm, "collectHostGarbage"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        realm.global_object().define_direct_property(
            "embeddedHost"_utf16, host, JS::default_attributes);
        return {};
    };

    hooks.drive = [&](Mite::EventLoop& loop) {
        // Model an external GUI window keeping the process alive after libuv is
        // idle. Deliver a rooted callback through the runtime's dispatch boundary.
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
        bool delivered = false;
        do {
            ++pump_count;
            if (loop.pump_once())
                break;
            if (!delivered && std::chrono::steady_clock::now() >= until) {
                delivered = true;
                if (pulse) {
                    loop.dispatch(
                        [&]() { return JS::call(*owner_vm, *pulse, JS::js_undefined()); });
                    pulse = nullptr;
                }
            }

            if (!delivered || loop.is_alive())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (!delivered || loop.is_alive());
        return loop.status();
    };

    hooks.shutdown = [&] {
        pulse = nullptr;
        owner_vm = nullptr;
        fputs("HOST_SHUTDOWN\n", stderr);
        fflush(stderr);
    };

    auto status = Mite::run(argc, argv, hooks);
    if (initialized && owner_vm)
        return 99; // Host cleanup must precede VM destruction.
    return status;
}
