#include "native/platform/GCIncludes.h"
/** Exact production Host plus real Mite factory; native peer never calls OS shell services. */
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/shell/Host.h"
#include "native/shell/Provider.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
using Firefly::Shell::Completion;
using Firefly::Shell::Result;

struct Request {
    std::string kind, value;
    bool activate;
    Completion complete;
};

std::vector<Request> requests;
int created {}, stopped {}, destroyed {}, reveals {}, delivered {};
bool passed = true;

void complete(size_t index, int mode)
{
    auto callback = requests.at(index).complete;
    std::thread worker([callback = std::move(callback), mode] {
        Result result;
        if (mode == 1)
            result = { "ERR_CONTROLLED_SHELL", "Native failure: Grüße 日本語" };
        if (mode == 2)
            result.message = std::string(256 * 1024, 'x');
        if (mode == 3)
            result.message = std::string("\xff", 1);
        callback(std::move(result));
    });
    worker.join();
}

class ControlledProvider final : public Firefly::Shell::Provider {
public:
    ControlledProvider()
    {
        ++created;
    }

    ~ControlledProvider() override
    {
        ++destroyed;
    }

    Result reveal(std::string const& value) override
    {
        ++reveals;
        if (value.ends_with("invalid-reveal-utf8"))
            return { "ERR_CONTROLLED_SHELL", std::string("\xff", 1) };
        if (value.ends_with("oversize-reveal"))
            return { "ERR_CONTROLLED_SHELL", std::string(65537, 'x') };
        if (value.ends_with("invalid-reveal-code"))
            return { "not a code", "Invalid native status" };
        return {};
    }

    void open_external(std::string value, bool activate, Completion callback) override
    {
        requests.push_back({ "openExternal", value, activate, std::move(callback) });
        if (value == "https://example.invalid/synchronous")
            requests.back().complete({});
    }

    void open_path(std::string value, Completion callback) override
    {
        requests.push_back({ "openPath", value, true, std::move(callback) });
    }

    void trash(std::string value, Completion callback) override
    {
        requests.push_back({ "trashItem", value, true, std::move(callback) });
    }

    void stop() override
    {
        if (!m_stopped) {
            m_stopped = true;
            ++stopped;
        }
    }

private:
    bool m_stopped {};
};
}

namespace Firefly::Shell {
std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<ControlledProvider>();
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    auto receiver = std::make_unique<QObject>();
    std::shared_ptr<Firefly::Shell::Controller> controller;
    GC::Root<JS::FunctionObject> handler;

    // Preserve live outer bindings and trace the real mutable callback root.
    // The revocation anchor retires before any referenced stack owner disappears.
    struct ProbeCaptures : Mite::NativeCaptureOwner<ProbeCaptures> {
        ProbeCaptures(GC::Root<JS::FunctionObject>& handler)
            : handler(handler)
        {
        }

        ~ProbeCaptures()
        {
            revoke_native_captures();
        }

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            visitor.visit(handler.ptr());
        }

        GC::Root<JS::FunctionObject>& handler;
    } captured { handler };

    JS::VM* owner = nullptr;
    Mite::EventLoop* loop = nullptr;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& jobs) -> JS::ThrowCompletionOr<void> {
        owner = &vm;
        loop = &jobs;
        GC::Root native { JS::Object::create(realm, nullptr) };
        controller
            = Firefly::Shell::install_host(realm, *native, *receiver, [&](std::string payload) {
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
              });
        native->define_native_function(realm, "setHandler"_utf16,
            GC::create_function(realm.heap(), GC::captures(Mite::native_reference(captured)),
                [](auto& captured, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& handler = captured.get().handler;
                    handler = vm.argument(0).as_function();
                    return JS::js_undefined();
                }),
            1, JS::default_attributes);
        native->define_native_function(
            realm, "complete"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                complete(vm.argument(0).as_i32(), vm.argument(1).as_i32());
                return JS::js_undefined();
            },
            2, JS::default_attributes);
        native->define_native_function(
            realm, "count"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(static_cast<int>(requests.size()));
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "created"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(created); }, 0,
            JS::default_attributes);
        native->define_native_function(
            realm, "value"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto const& value = requests.at(vm.argument(0).as_i32()).value;
                return JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(StringView(value.data(), value.size())));
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "activate"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(requests.at(vm.argument(0).as_i32()).activate);
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "collect"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module("shell-test", *native,
            { "shellShowItemInFolder", "shellRequest", "shellCancel", "shellShutdown", "setHandler",
                "complete", "count", "created", "value", "activate", "collect" });
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
        receiver.reset();
        handler = nullptr;
        owner = nullptr;
        loop = nullptr;
    };

    int status = Mite::run(argc, argv, hooks);
    controller.reset();
    int before = delivered;
    for (size_t index = 0; index < requests.size(); ++index)
        complete(index, 0);
    QCoreApplication::processEvents();
    if (status || !passed || created != 1 || stopped != 1 || destroyed != 1
        || delivered != before) {
        std::fprintf(stderr,
            "FAIL shell host teardown: status=%d created=%d stopped=%d destroyed=%d\n", status,
            created, stopped, destroyed);
        return 1;
    }

    std::puts("PASS exact shell Host worker/owner delivery and controller/receiver/VM teardown; no "
              "OS shell operations");
    return 0;
}
