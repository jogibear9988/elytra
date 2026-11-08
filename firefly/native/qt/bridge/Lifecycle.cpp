/** Own hook installation, callback dispatch and teardown before the VM is destroyed. */
#include "../ChromeWindow.h"
#include "../PreloadHooks.h"
#include "Impl.h"
#include "TransportHooks.h"
#include "Value.h"
#include "core/EventLoop.h"
#include <AK/JsonObject.h>
#include <LibCore/EventLoop.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <QTimer>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
using BridgeDetail::text;

Bridge::Impl::Impl(JS::VM& vm, JS::Realm& realm, Mite::EventLoop& jobs, Core::EventLoop& gui,
    std::optional<int>& exit_code, bool electron_compat)
    : vm(vm)
    , realm(realm)
    , jobs(jobs)
    , gui(gui)
    , exit_code(exit_code)
    , electron_compat(electron_compat)
{
    application_menu = std::make_unique<ApplicationMenu>(*context,
        [this](int generation, int command, bool checked, QWidget* focused, int modifiers) {
            if (stopping)
                return;
            int window_id = 0;
            for (auto const& [id, state] : windows)
                if (state.kind == State::Kind::WindowMain && !state.closing && focused
                    && (focused == state.window || state.window->isAncestorOf(focused))) {
                    window_id = id;
                    break;
                }

            JsonObject metadata;
            metadata.set("generation"sv, generation);
            metadata.set("command"sv, command);
            metadata.set("checked"sv, checked);
            metadata.set("modifiers"sv, modifiers);
            enqueue(window_id, "menu-click", metadata.serialized().to_byte_string());
        });
    ipc_timer = Core::Timer::create_repeating(
        AK::Duration::from_seconds(1), [this] { expire_ipc_events(); });
    ipc_timer->start();
    initialize_protocols();
    set_preload_configurator([this](auto&, auto& page) { configure_preloads(page); });
    set_native_ipc_handler([this](auto& page, auto navigable, auto params, NativeIpcKind kind) {
        receive_ipc(page, navigable, move(params), kind);
    });
}

Bridge::Impl::~Impl()
{
    stop();
}

void Bridge::Impl::stop()
{
    if (stopping)
        return;
    stopping = true;
    set_preload_configurator({});
    set_native_ipc_handler({});
    ipc_timer->stop();
    sandbox.stop();
    tracing.reset();
    stop_protocols();
    if (desktop_capture)
        desktop_capture->stop();
    if (application_menu)
        application_menu->stop();
    while (!windows.empty())
        close_contents(windows.begin()->first, true);
    context.reset(); // Cancel queued Qt callbacks before the Mite VM goes away.
    handler = nullptr;
    api = nullptr;
}

void Bridge::Impl::deliver(int id, ByteString const& type, ByteString const& data)
{
    if (stopping || !handler)
        return;
    if (jobs.is_busy()) {
        QTimer::singleShot(1, context.get(), [this, id, type, data] { deliver(id, type, data); });
        return;
    }

    Tracing::Recorder::Scope scope(
        tracing, type.starts_with("ipc-"sv) ? "firefly.ipc" : "firefly.main", type.characters());
    jobs.dispatch([&] {
        return JS::call(
            vm, *handler, JS::js_undefined(), JS::Value(id), text(vm, type), text(vm, data));
    });
    if (jobs.status())
        gui.quit(jobs.status());
}

void Bridge::Impl::enqueue(int id, ByteString type, ByteString data)
{
    if (stopping)
        return;
    QTimer::singleShot(0, context.get(),
        [this, id, type = move(type), data = move(data)] { deliver(id, type, data); });
}
}
