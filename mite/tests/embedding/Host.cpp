#include "Runtime.h"
#include "EventLoop.h"
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
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader&, Mite::EventLoop& loop) -> JS::ThrowCompletionOr<void> {
        initialized = true;
        owner_vm = &vm;
        if (auto* path = getenv("MITE_EMBED_FAIL_INIT_WORK_PATH")) {
            TRY(loop.queue_work([path = std::string(path)] { std::ifstream blocked(path); },
                GC::create_function(vm.heap(), []() -> JS::ThrowCompletionOr<JS::Value> { return JS::js_undefined(); })));
            return vm.throw_completion<JS::Error>("Embedded host failed with pending work"_utf16);
        }
        if (getenv("MITE_EMBED_FAIL_INIT"))
            return vm.throw_completion<JS::Error>("Embedded host initialization failed"_utf16);
        auto host = JS::Object::create(realm, nullptr);
        host->define_native_function(realm, "onPulse"_utf16, [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_function())
                return vm.throw_completion<JS::TypeError>("Host pulse requires a callback"_utf16);
            pulse = vm.argument(0).as_function();
            return JS::js_undefined();
        }, 1, JS::default_attributes);
        realm.global_object().define_direct_property("embeddedHost"_utf16, host, JS::default_attributes);
        return {};
    };
    hooks.drive = [&](Mite::EventLoop& loop) {
        // Model an external GUI window keeping the process alive after libuv is
        // idle. Deliver a rooted callback through the runtime's dispatch boundary.
        auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(20);
        bool delivered = false;
        do {
            if (loop.pump_once()) break;
            if (!delivered && std::chrono::steady_clock::now() >= until) {
                delivered = true;
                if (pulse) {
                    loop.dispatch([&]() { return JS::call(*owner_vm, *pulse, JS::js_undefined()); });
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
    if (initialized && owner_vm) return 99; // Host cleanup must precede VM destruction.
    return status;
}
