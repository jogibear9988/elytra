/** Actual Qt QAction -> queued LibJS callback; no production test hook or browser window. */
#include "core/EventLoop.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/menu/Host.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QAction>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMenu>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QAction* find(QWidget& widget, int command)
{
    for (auto* action : widget.actions()) {
        if (action->data().toInt() == command)
            return action;
        if (action->menu())
            if (auto* result = find(*action->menu(), command))
                return result;
    }

    return nullptr;
}
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QWidget window;
    new QVBoxLayout(&window);
    std::unique_ptr<QWidget> second;
    JS::VM* vm = nullptr;
    Mite::EventLoop* jobs = nullptr;
    GC::Root<JS::FunctionObject> handler;
    bool stopped = false;
    std::function<void(QByteArray)> enqueue;
    enqueue = [&](QByteArray wire) {
        QTimer::singleShot(1, &app, [&, wire] {
            if (stopped || !handler)
                return;
            if (jobs->is_busy()) {
                enqueue(wire);
                return;
            }

            jobs->dispatch([&] {
                return JS::call(*vm, *handler, JS::js_undefined(),
                    JS::PrimitiveString::create(
                        *vm, Utf16String::from_utf8(StringView(wire.data(), wire.size()))));
            });
            if (jobs->status())
                app.exit(jobs->status());
        });
    };

    Firefly::ApplicationMenu menu(
        app, [&](int generation, int command, bool checked, QWidget*, int modifiers) {
            enqueue(
                QJsonDocument(QJsonObject { { "generation", generation }, { "command", command },
                                  { "checked", checked }, { "modifiers", modifiers } })
                    .toJson(QJsonDocument::Compact));
        });
    menu.attach(window);
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& value, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& loop) -> JS::ThrowCompletionOr<void> {
        vm = &value;
        jobs = &loop;
        GC::Root native { JS::Object::create(realm, nullptr) };
        Firefly::install_menu_host(realm, *native, menu);
        native->define_native_function(
            realm, "setHandler"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_function())
                    return vm.throw_completion<JS::TypeError>("Expected handler"_utf16);
                handler = vm.argument(0).as_function();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "trigger"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_int32() || !menu.bar())
                    return JS::Value(false);
                auto* action = find(*menu.bar(), vm.argument(0).as_i32());
                if (!action)
                    return JS::Value(false);
                action->trigger();
                return JS::Value(true);
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "state"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* bar = menu.bar();
                auto* action = bar && vm.argument(0).is_int32()
                    ? find(*bar, vm.argument(0).as_i32())
                    : nullptr;
                QJsonObject state { { "bar", bar != nullptr },
                    { "native", bar && bar->isNativeMenuBar() },
                    { "checked", action && action->isChecked() },
                    { "enabled", action && action->isEnabled() },
                    { "visible", action && action->isVisible() } };
                if (action)
                    state.insert("label", action->text());
                auto wire = QJsonDocument(state).toJson(QJsonDocument::Compact);
                return JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(StringView(wire.data(), wire.size())));
            },
            1, JS::default_attributes);
        native->define_native_function(
            realm, "attachWindow"_utf16,
            [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                second = std::make_unique<QWidget>();
                new QVBoxLayout(second.get());
                menu.attach(*second);
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module("menu-test", *native,
            { "menuApply", "setHandler", "trigger", "state", "attachWindow" });
    };

    hooks.drive = [&](Mite::EventLoop& loop) {
        QTimer poll;
        poll.setInterval(1);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            loop.pump_once();
            if (loop.status() || !loop.is_alive())
                app.exit(loop.status());
        });
        loop.pump_once();
        if (loop.status() || !loop.is_alive())
            return loop.status();
        poll.start();
        QTimer::singleShot(10000, &app, [&] { app.exit(124); });
        return app.exec();
    };

    hooks.shutdown = [&] {
        stopped = true;
        menu.stop();
        handler = nullptr;
    };

    return Mite::run(argc, argv, hooks);
}
