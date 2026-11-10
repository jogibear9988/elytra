#include "native/platform/GCIncludes.h"
/** Real factory/LibJS/exact Host; controlled preference store and notification peers only. */
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/system-preferences/Host.h"
#include "native/system-preferences/Provider.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QCoreApplication>
#include <QThread>
#include <cstdio>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
using Firefly::SystemPreferences::Event;
using Firefly::SystemPreferences::EventCallback;
using Firefly::SystemPreferences::Provider;
using Firefly::SystemPreferences::Result;
using Firefly::SystemPreferences::ValueResult;

struct Subscription {
    int id;
    std::string kind;
    std::optional<std::string> name;
    EventCallback callback;
};

std::vector<Subscription> subscriptions;
std::unordered_map<std::string, std::string> values;
int created {}, stopped {}, destroyed {}, delivered {}, mode {}, unsubscribed {};
bool passed = true;

Event event_for(Subscription const& subscription, int choice)
{
    Event event { subscription.id, subscription.name.value_or("ControlledName"),
        "{\"nested\":{\"enabled\":true},\"unicode\":\"Grüße 日本語\"}", "ControlledSender", {},
        {} };
    if (choice == 1)
        event.user_info_json = "{";
    if (choice == 2)
        event.name = std::string("\xff", 1);
    if (choice == 3)
        event.user_info_json = std::string(256 * 1024, 'x');
    if (choice == 4) {
        event.code = "ERR_CONTROLLED_PREFERENCES";
        event.message = "Native failure: Grüße 日本語";
    }

    if (choice == 5)
        event.user_info_json = "[]";
    if (choice == 6)
        event.user_info_json = std::string("{\"value\":\"") + static_cast<char>(0xff) + "\"}";
    if (choice == 7)
        event.user_info_json = "{\"value\":\"\\u0000\"}";
    if (choice == 8)
        event.user_info_json = "{\"value\":[[[[[[[[[true]]]]]]]]]}";
    if (choice == 9) {
        event.user_info_json = "{\"value\":[0";
        for (int index = 0; index < 1024; ++index)
            event.user_info_json += ",0";
        event.user_info_json += "]}";
    }

    return event;
}

void emit_event(size_t index, int choice)
{
    auto callback = subscriptions.at(index).callback;
    auto event = event_for(subscriptions.at(index), choice);
    std::thread worker([callback = std::move(callback), event = std::move(event)]() mutable {
        callback(std::move(event));
    });
    worker.join();
}

ValueResult value_result(std::string json)
{
    int choice = mode;
    mode = 0;
    if (choice == 1)
        json = "{";
    if (choice == 2)
        json = "{\"value\":123}";
    if (choice == 3)
        json = std::string("\xff", 1);
    if (choice == 4)
        json = std::string(256 * 1024, 'x');
    if (choice == 6)
        json = std::string("{\"value\":\"") + static_cast<char>(0xff) + "\"}";
    if (choice == 5)
        return { "ERR_CONTROLLED_PREFERENCES", "Native failure: Grüße 日本語", {} };
    return { {}, {}, std::move(json) };
}

class ControlledProvider final : public Provider {
public:
    ControlledProvider()
    {
        ++created;
    }

    ~ControlledProvider() override
    {
        ++destroyed;
    }

    ValueResult get_user_default(std::string const& name, std::string const& type) override
    {
        auto found = values.find(name);
        auto value = found == values.end() ? (type == "boolean" ? "false" : "\"\"") : found->second;
        return value_result("{\"value\":" + value + "}");
    }

    Result set_user_default(
        std::string const& name, std::string const&, std::string const& json) override
    {
        values[name] = json;
        return {};
    }

    ValueResult media_access_status(std::string const&) override
    {
        return value_result("{\"value\":\"denied\"}");
    }

    Result subscribe(int id, std::string const& kind, std::optional<std::string> const& name,
        EventCallback callback) override
    {
        subscriptions.push_back({ id, kind, name, std::move(callback) });
        if (name == "sync" || name == "sync-fail")
            subscriptions.back().callback(event_for(subscriptions.back(), 0));
        if (name == "sync-fail")
            return { "ERR_CONTROLLED_PREFERENCES", "Native failure: Grüße 日本語" };
        return {};
    }

    Result unsubscribe(int, std::string const&) override
    {
        ++unsubscribed;
        return {};
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

namespace Firefly::SystemPreferences {
std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<ControlledProvider>();
}
}

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    auto receiver = std::make_unique<QObject>();
    std::shared_ptr<Firefly::SystemPreferences::Controller> controller;
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
        controller = Firefly::SystemPreferences::install_host(
            realm, *native, *receiver, [&](std::string payload) {
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
            realm, "emit"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                emit_event(vm.argument(0).as_i32(), vm.argument(1).as_i32());
                return JS::js_undefined();
            },
            2, JS::default_attributes);
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
                return JS::Value(static_cast<int>(subscriptions.size()));
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "created"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(created); }, 0,
            JS::default_attributes);
        native->define_native_function(
            realm, "unsubscribed"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(unsubscribed); }, 0,
            JS::default_attributes);
        native->define_native_function(
            realm, "collect"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module("preferences-test", *native,
            { "preferencesGetDefault", "preferencesSetDefault", "preferencesMediaStatus",
                "preferencesSubscribe", "preferencesUnsubscribe", "preferencesShutdown",
                "setHandler", "emit", "control", "count", "created", "unsubscribed", "collect" });
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
    for (size_t index = 0; index < subscriptions.size(); ++index)
        emit_event(index, 0);
    QCoreApplication::processEvents();
    if (status || !passed || created != 1 || stopped != 1 || destroyed != 1
        || delivered != before) {
        std::fprintf(stderr,
            "FAIL preferences host teardown: status=%d created=%d stopped=%d destroyed=%d\n",
            status, created, stopped, destroyed);
        return 1;
    }

    std::puts("PASS exact preferences Host worker/owner delivery and controller/receiver/VM "
              "disposal; no OS preferences/permissions changed");
    return 0;
}
