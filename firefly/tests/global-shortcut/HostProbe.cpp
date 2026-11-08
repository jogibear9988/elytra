/** Exact production Host and LibJS factory; only native OS registrations are controlled. */
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/global-shortcut/Host.h"
#include "native/global-shortcut/Provider.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include <set>
#include <thread>
#include <vector>

namespace {
using Firefly::GlobalShortcut::Chord;
using Firefly::GlobalShortcut::Controller;
using Firefly::GlobalShortcut::EventCallback;
using Firefly::GlobalShortcut::install_host;
using Firefly::GlobalShortcut::Provider;
using Firefly::GlobalShortcut::Result;

struct Registration {
    int token;
    EventCallback callback;
};

std::vector<Registration> registrations;
int created {}, stopped {}, destroyed {}, owned {}, mode {}, delivered {};
bool passed = true;

void emit_event(size_t index)
{
    auto registration = registrations.at(index);
    std::thread worker(
        [registration = std::move(registration)] { registration.callback(registration.token); });
    worker.join();
}

class ControlledProvider final : public Provider {
public:
    explicit ControlledProvider(EventCallback callback)
        : m_callback(std::move(callback))
    {
        ++created;
    }

    ~ControlledProvider() override
    {
        stop();
        ++destroyed;
    }

    Result register_shortcut(int token, Chord const&) override
    {
        int choice = mode;
        mode = choice == 4 ? 1 : 0;
        if (choice == 1)
            return { false, {}, {} };
        if (choice == 2)
            return { false, "ERR_CONTROLLED_SHORTCUT", "Native failure: Grüße 日本語" };
        m_owned.insert(token);
        ++owned;
        registrations.push_back({ token, m_callback });
        if (choice == 3)
            m_callback(token);
        return { true, {}, {} };
    }

    Result unregister_shortcut(int token) override
    {
        if (mode == 5) {
            mode = 0;
            return { false, "ERR_CONTROLLED_RELEASE", "Native release failed" };
        }

        owned -= static_cast<int>(m_owned.erase(token));
        return { true, {}, {} };
    }

    void stop() override
    {
        if (m_stopped)
            return;
        m_stopped = true;
        ++stopped;
        owned -= static_cast<int>(m_owned.size());
        m_owned.clear();
    }

private:
    EventCallback m_callback;
    std::set<int> m_owned;
    bool m_stopped {};
};
}

namespace Firefly::GlobalShortcut {
std::unique_ptr<Provider> Provider::create(EventCallback)
{
    passed = false; // The controlled injection must own every registration here.
    return {};
}
}

int main(int argc, char** argv)
{
    bool expect_outstanding = argc > 2 && std::string(argv[2]) == "--leave-active";
    QCoreApplication application(argc, argv);
    auto receiver = std::make_unique<QObject>();
    std::shared_ptr<Controller> controller;
    GC::Root<JS::FunctionObject> handler;
    JS::VM* owner = nullptr;
    Mite::EventLoop* loop = nullptr;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& jobs) -> JS::ThrowCompletionOr<void> {
        owner = &vm;
        loop = &jobs;
        GC::Root native { JS::Object::create(realm, nullptr) };
        controller = install_host(
            realm, *native, *receiver,
            [&](std::string payload) {
                if (!owner || !loop || !handler
                    || QThread::currentThread() != application.thread()) {
                    passed = false;
                    return;
                }

                ++delivered;
                GC::Root callback { handler };
                loop->dispatch([&] {
                    return JS::call(*owner, *callback, JS::js_undefined(),
                        JS::PrimitiveString::create(*owner,
                            Utf16String::from_utf8(StringView(payload.data(), payload.size()))));
                });
            },
            [](EventCallback callback) {
                return std::make_unique<ControlledProvider>(std::move(callback));
            });
        native->define_native_function(
            realm, "setHandler"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                handler = vm.argument(0).as_function();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "emit"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                emit_event(vm.argument(0).as_i32());
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "control"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                mode = vm.argument(0).as_i32();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "count"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(static_cast<int>(registrations.size()));
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "owned"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(owned); }, 0,
            JS::default_attributes);
        native->define_native_function(
            realm, "created"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(created); }, 0,
            JS::default_attributes);
        native->define_native_function(
            realm, "destroyReceiver"_utf16,
            [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                receiver.reset();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "collect"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module("shortcut-test", *native,
            { "globalShortcutRegister", "globalShortcutUnregister", "globalShortcutShutdown",
                "setHandler", "emit", "control", "count", "owned", "created", "collect",
                "destroyReceiver" });
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
        if (expect_outstanding && owned != 1)
            passed = false;
        if (controller)
            controller->stop();
        receiver.reset();
        handler = nullptr;
        owner = nullptr;
        loop = nullptr;
    };

    int status = Mite::run(argc, argv, hooks);
    controller.reset();
    int before = delivered;
    for (size_t index = 0; index < registrations.size(); ++index)
        emit_event(index);
    QCoreApplication::processEvents();
    if (status || !passed || created != 1 || stopped != 1 || destroyed != 1 || owned
        || delivered != before) {
        std::fprintf(stderr,
            "FAIL shortcut Host teardown: status=%d created=%d stopped=%d destroyed=%d owned=%d\n",
            status, created, stopped, destroyed, owned);
        return 1;
    }

    std::puts("PASS exact globalShortcut Host worker/owner delivery and controller/receiver/VM "
              "disposal; no OS hotkeys registered");
    return 0;
}
