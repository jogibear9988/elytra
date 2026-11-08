/** Exact production dialog Host and LibJS factory; providers never display an OS dialog. */
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/dialog/Host.h"
#include "native/dialog/Provider.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <QApplication>
#include <QJsonArray>
#include <QPointer>
#include <QThread>
#include <QWidget>
#include <QtPlugin>
#include <chrono>
#include <cstdio>
#include <set>
#include <thread>
#include <vector>

Q_IMPORT_PLUGIN(QOffscreenIntegrationPlugin)

namespace {
using Firefly::Dialogs::Completion;
using Firefly::Dialogs::Controller;
using Firefly::Dialogs::Provider;
using Firefly::Dialogs::Result;

struct Request {
    int id;
    QString kind;
    QJsonObject options;
    QPointer<QWidget> parent;
    Completion callback;
};

std::vector<Request> requests;
std::set<int> pending_ids;
int created {}, stopped {}, destroyed {}, owned {}, canceled {}, delivered {}, mode {};
bool passed = true;
bool in_modal = false;

Result result_for(Request const& request, int choice)
{
    Result result;
    if (request.kind == QStringLiteral("message"))
        result.value = { { "response", 1 }, { "checkboxChecked", true } };
    else if (request.kind == QStringLiteral("open"))
        result.value = { { "canceled", false },
            { "filePaths", QJsonArray { QString::fromUtf8("/tmp/Grüße 日本語.txt") } } };
    else
        result.value
            = { { "canceled", false }, { "filePath", QString::fromUtf8("/tmp/Grüße 日本語.txt") } };
    if (choice == 1) {
        result.code = "ERR_CONTROLLED_DIALOG";
        result.message = "Native dialog failure: Grüße 日本語";
    }

    if (choice == 2)
        result.value = { { "response", "wrong" }, { "checkboxChecked", false } };
    if (choice == 3)
        result.value = { { "canceled", false }, { "filePath", QString(1024 * 1024, 'x') } };
    if (choice == 4) {
        result.code = std::string(1, static_cast<char>(0xff));
        result.message = "Malformed native error";
    }

    return result;
}

void complete(size_t index, int choice)
{
    auto request = requests.at(index);
    owned -= static_cast<int>(pending_ids.erase(request.id));
    auto result = result_for(request, choice);
    std::thread worker([callback = std::move(request.callback),
                           result = std::move(result)]() mutable { callback(std::move(result)); });
    worker.join();
}

class ControlledProvider final : public Provider {
public:
    ControlledProvider()
    {
        ++created;
    }

    ~ControlledProvider() override
    {
        stop();
        ++destroyed;
    }

    Result start(int id, QString const& kind, QJsonObject const& options, QWidget* parent,
        Completion callback) override
    {
        int choice = mode;
        mode = 0;
        if (choice == 1)
            return { {}, "ERR_CONTROLLED_DIALOG", "Native admission failure: Grüße 日本語" };
        requests.push_back({ id, kind, options, parent, std::move(callback) });
        pending_ids.insert(id);
        ++owned;
        if (choice == 2 || choice == 3) {
            owned -= static_cast<int>(pending_ids.erase(id));
            requests.back().callback(result_for(requests.back(), 0));
        }

        if (choice == 2) {
            return { {}, "ERR_CONTROLLED_DIALOG", "Failed after synchronous completion" };
        }

        return {};
    }

    Result message_sync(QJsonObject const& options, QWidget*) override
    {
        int choice = mode;
        mode = 0;
        if (choice == 1)
            return { {}, "ERR_CONTROLLED_DIALOG", "Native sync failure: Grüße 日本語" };
        if (choice == 2)
            return { { { "response", "wrong" } }, {}, {} };
        if (choice == 3) {
            in_modal = true;
            complete(requests.size() - 1, 0);
            QCoreApplication::processEvents();
            in_modal = false;
        }

        return { { { "response", options.value("buttons").toArray().size() > 1 ? 1 : 0 },
                     { "checkboxChecked", options.value("checkboxChecked").toBool() } },
            {}, {} };
    }

    void cancel(int id) override
    {
        ++canceled;
        owned -= static_cast<int>(pending_ids.erase(id));
    }

    void stop() override
    {
        if (m_stopped)
            return;
        m_stopped = true;
        ++stopped;
        owned -= static_cast<int>(pending_ids.size());
        pending_ids.clear();
    }

private:
    bool m_stopped {};
};
}

namespace Firefly::Dialogs {
std::unique_ptr<Provider> Provider::create()
{
    passed = false; // Every provider in this probe must use explicit controlled injection.
    return {};
}
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);
    bool expect_outstanding = argc > 2 && std::string(argv[2]) == "--leave-active";
    auto receiver = std::make_unique<QObject>();
    auto parent = std::make_unique<QWidget>(); // Hidden: never shown or assigned a native window.
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
        controller = Firefly::Dialogs::install_host(
            realm, *native, *receiver,
            [&](std::string payload) {
                if (in_modal || !owner || !loop || !handler
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
            [&](int id) -> QWidget* { return id == 7 ? parent.get() : nullptr; },
            [] { return std::make_unique<ControlledProvider>(); });
        native->define_native_function(
            realm, "setHandler"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                handler = vm.argument(0).as_function();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "complete"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                complete(vm.argument(0).as_i32(), vm.argument(1).as_i32());
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
                return JS::Value(static_cast<int>(requests.size()));
            },
            0, JS::default_attributes);
        native->define_native_function(
            realm, "created"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(created); }, 0,
            JS::default_attributes);
        native->define_native_function(
            realm, "canceled"_utf16,
            +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(canceled); }, 0,
            JS::default_attributes);
        native->define_native_function(
            realm, "destroyParent"_utf16,
            [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                parent.reset();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
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
        return modules.register_host_module("dialog-test", *native,
            { "dialogStart", "dialogMessageSync", "dialogCancel", "dialogShutdown", "setHandler",
                "complete", "control", "count", "created", "canceled", "destroyParent",
                "destroyReceiver", "collect" });
    };

    hooks.drive = [&](Mite::EventLoop& jobs) {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        do {
            if (jobs.pump_once())
                break;
            QCoreApplication::processEvents();
            if (std::chrono::steady_clock::now() >= deadline) {
                std::fprintf(stderr,
                    "FAIL dialog Host driver timed out: requests=%zu delivered=%d owned=%d "
                    "pending=%d\n",
                    requests.size(), delivered, owned, controller && controller->has_pending());
                passed = false;
                return 1;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (
            jobs.is_alive() || (!expect_outstanding && controller && controller->has_pending()));
        return jobs.status();
    };

    hooks.shutdown = [&] {
        if (expect_outstanding && owned != 1)
            passed = false;
        if (controller)
            controller->stop();
        receiver.reset();
        parent.reset();
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
    if (status || !passed || created != 1 || stopped != 1 || destroyed != 1 || owned
        || delivered != before) {
        std::fprintf(stderr,
            "FAIL dialog Host teardown: status=%d created=%d stopped=%d destroyed=%d owned=%d\n",
            status, created, stopped, destroyed, owned);
        return 1;
    }

    std::puts("PASS exact dialog Host worker/owner delivery and parent/controller/receiver/VM "
              "disposal; no OS dialogs shown");
    return 0;
}
