#include "native/platform/GCIncludes.h"
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
#include <cstdio>

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
    unsigned pending = 0, emissions = 0, deliveries = 0, triggers = 0, native_signals = 0;
    int last_generation = 0, last_command = 0;
    auto diagnostic = [&] {
        fprintf(stderr,
            "Native menu generation=%d command=%d triggers=%u native_signals=%u emissions=%u "
            "deliveries=%u pending=%u stopped=%d\n",
            last_generation, last_command, triggers, native_signals, emissions, deliveries, pending,
            stopped);
    };

    std::function<void(QByteArray)> enqueue;
    enqueue = [&](QByteArray wire) {
        ++pending;
        QTimer::singleShot(1, &app, [&, wire] {
            --pending;
            if (stopped || !handler)
                return;
            if (jobs->is_busy()) {
                enqueue(wire);
                return;
            }

            ++deliveries;
            jobs->dispatch([&] {
                return JS::call(*vm, *handler, JS::js_undefined(),
                    JS::PrimitiveString::create(
                        *vm, Utf16String::from_utf8(StringView(wire.data(), wire.size()))));
            });
            if (jobs->status()) {
                diagnostic();
                app.exit(jobs->status());
            }
        });
    };

    Firefly::ApplicationMenu menu(
        app, [&](int generation, int command, bool checked, QWidget*, int modifiers) {
            ++emissions;
            last_generation = generation;
            last_command = command;
            enqueue(
                QJsonDocument(QJsonObject { { "generation", generation }, { "command", command },
                                  { "checked", checked }, { "modifiers", modifiers } })
                    .toJson(QJsonDocument::Compact));
        });
    menu.attach(window);

    // Borrow native fixture state through a revocable anchor; only handler owns JS cells.
    struct ProbeCaptures : Mite::NativeCaptureOwner<ProbeCaptures> {
        ProbeCaptures(GC::Root<JS::FunctionObject>& handler, Firefly::ApplicationMenu& menu,
            QApplication& app, std::unique_ptr<QWidget>& second, unsigned& triggers,
            unsigned& native_signals, unsigned& emissions, unsigned& deliveries, unsigned& pending,
            int& last_generation, int& last_command)
            : handler(handler)
            , menu(menu)
            , app(app)
            , second(second)
            , triggers(triggers)
            , native_signals(native_signals)
            , emissions(emissions)
            , deliveries(deliveries)
            , pending(pending)
            , last_generation(last_generation)
            , last_command(last_command)
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
        Firefly::ApplicationMenu& menu;
        QApplication& app;
        std::unique_ptr<QWidget>& second;
        unsigned& triggers;
        unsigned& native_signals;
        unsigned& emissions;
        unsigned& deliveries;
        unsigned& pending;
        int& last_generation;
        int& last_command;
    } captured { handler, menu, app, second, triggers, native_signals, emissions, deliveries,
        pending, last_generation, last_command };

    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& value, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop& loop) -> JS::ThrowCompletionOr<void> {
        vm = &value;
        jobs = &loop;
        GC::Root native { JS::Object::create(realm, nullptr) };
        Firefly::install_menu_host(realm, *native, menu);
        native->define_native_function(realm, "setHandler"_utf16,
            GC::create_function(realm.heap(), GC::captures(Mite::native_reference(captured)),
                [](auto& captured, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& handler = captured.get().handler;

                    if (!vm.argument(0).is_function())
                        return vm.throw_completion<JS::TypeError>("Expected handler"_utf16);
                    handler = vm.argument(0).as_function();
                    return JS::js_undefined();
                }),
            1, JS::default_attributes);
        native->define_native_function(realm, "trigger"_utf16,
            GC::create_function(realm.heap(), GC::captures(Mite::native_reference(captured)),
                [](auto& captured, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& menu = captured.get().menu;
                    auto& app = captured.get().app;
                    auto& triggers = captured.get().triggers;
                    auto& native_signals = captured.get().native_signals;

                    if (!vm.argument(0).is_int32() || !menu.bar())
                        return JS::Value(false);
                    auto* action = find(*menu.bar(), vm.argument(0).as_i32());
                    if (!action)
                        return JS::Value(false);
                    ++triggers;
                    auto connection = QObject::connect(
                        action, &QAction::triggered, &app, [&](bool) { ++native_signals; });
                    action->trigger();
                    QObject::disconnect(connection);
                    return JS::Value(true);
                }),
            1, JS::default_attributes);
        native->define_native_function(realm, "state"_utf16,
            GC::create_function(realm.heap(), GC::captures(Mite::native_reference(captured)),
                [](auto& captured, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& menu = captured.get().menu;
                    auto& last_generation = captured.get().last_generation;
                    auto& last_command = captured.get().last_command;
                    auto& triggers = captured.get().triggers;
                    auto& native_signals = captured.get().native_signals;
                    auto& emissions = captured.get().emissions;
                    auto& deliveries = captured.get().deliveries;
                    auto& pending = captured.get().pending;

                    auto* bar = menu.bar();
                    auto* action = bar && vm.argument(0).is_int32()
                        ? find(*bar, vm.argument(0).as_i32())
                        : nullptr;
                    QJsonObject state { { "platform", QGuiApplication::platformName() },
                        { "bar", bar != nullptr }, { "native", bar && bar->isNativeMenuBar() },
                        { "checked", action && action->isChecked() },
                        { "enabled", action && action->isEnabled() },
                        { "visible", action && action->isVisible() },
                        { "generation", last_generation }, { "command", last_command },
                        { "triggers", static_cast<int>(triggers) },
                        { "native_signals", static_cast<int>(native_signals) },
                        { "emissions", static_cast<int>(emissions) },
                        { "deliveries", static_cast<int>(deliveries) },
                        { "pending", static_cast<int>(pending) } };
                    if (action)
                        state.insert("label", action->text());
                    auto wire = QJsonDocument(state).toJson(QJsonDocument::Compact);
                    return JS::PrimitiveString::create(
                        vm, Utf16String::from_utf8(StringView(wire.data(), wire.size())));
                }),
            1, JS::default_attributes);
        native->define_native_function(realm, "attachWindow"_utf16,
            GC::create_function(realm.heap(), GC::captures(Mite::native_reference(captured)),
                [](auto& captured, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& second = captured.get().second;
                    auto& menu = captured.get().menu;

                    second = std::make_unique<QWidget>();
                    new QVBoxLayout(second.get());
                    menu.attach(*second);
                    return JS::js_undefined();
                }),
            0, JS::default_attributes);
        return modules.register_host_module("menu-test", *native,
            { "menuApply", "setHandler", "trigger", "state", "attachWindow" });
    };

    hooks.drive = [&](Mite::EventLoop& loop) {
        QTimer poll;
        poll.setInterval(1);
        QObject::connect(&poll, &QTimer::timeout, [&] {
            loop.pump_once();
            if (loop.status() || (!loop.is_alive() && pending == 0))
                app.exit(loop.status());
        });
        loop.pump_once();
        if (loop.status() || (!loop.is_alive() && pending == 0))
            return loop.status();
        poll.start();
        QTimer::singleShot(10000, &app, [&] {
            diagnostic();
            app.exit(124);
        });
        return app.exec();
    };

    hooks.shutdown = [&] {
        stopped = true;
        menu.stop();
        handler = nullptr;
    };

    auto result = Mite::run(argc, argv, hooks);
    if (result)
        diagnostic();
    return result;
}
