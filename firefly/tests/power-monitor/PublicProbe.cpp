/** Actual public factory -> exact native Host -> OS Provider, without a desktop application. */
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/power-monitor/Host.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QCoreApplication>
#include <QTimer>
#include <memory>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    auto receiver = std::make_unique<QObject>();
    std::shared_ptr<Firefly::PowerMonitor::Controller> controller;
    GC::Root<JS::FunctionObject> handler;
    JS::VM* owner = nullptr;
    Mite::EventLoop* loop = nullptr;
    std::function<void(std::string)> deliver;
    deliver = [&](std::string value) {
        if (!owner || !loop || !handler || !receiver)
            return;
        if (loop->is_busy()) {
            QTimer::singleShot(0, receiver.get(),
                [&, value = std::move(value)]() mutable { deliver(std::move(value)); });
            return;
        }

        GC::Root callback { handler };
        loop->dispatch([&] {
            return JS::call(*owner, *callback, JS::js_undefined(),
                JS::PrimitiveString::create(
                    *owner, Utf16String::from_utf8(StringView(value.data(), value.size()))));
        });
    };

    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& jobs) -> JS::ThrowCompletionOr<void> {
        owner = &vm;
        loop = &jobs;
        GC::Root native { JS::Object::create(realm, nullptr) };
        controller = Firefly::PowerMonitor::install_host(realm, *native, *receiver, deliver);
        native->define_native_function(
            realm, "setHandler"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Power probe handler must be callable"_utf16);
                handler = vm.argument(0).as_function();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "collect"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module("power-test", *native,
            { "powerMonitorCapabilities", "powerMonitorQuery", "powerMonitorStart",
                "powerMonitorStop", "setHandler", "collect" });
    };

    hooks.drive = [&](Mite::EventLoop& jobs) {
        do {
            if (jobs.pump_once())
                break;
            QCoreApplication::processEvents();
        } while (jobs.is_alive());
        return jobs.status();
    };

    hooks.shutdown = [&] {
        if (controller)
            controller->stop();
        receiver.reset(); // Cancel Qt deliveries before clearing roots and the VM.
        handler = nullptr;
        owner = nullptr;
        loop = nullptr;
    };

    return Mite::run(argc, argv, hooks);
}
