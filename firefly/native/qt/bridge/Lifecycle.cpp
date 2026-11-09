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
    ipc_timer = Core::Timer::create_repeating(AK::Duration::from_seconds(1), [this] {
        expire_ipc_events();
        expire_permissions();
        if (web_request)
            web_request->sweep();
    });
    ipc_timer->start();
    initialize_protocols();
    initialize_permissions();
    initialize_web_request();
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
    if (web_request)
        web_request->stop();
    web_request = nullptr;
    Permissions::install_hooks({});
    reset_permissions(0);
    permission_check = nullptr;
    frames.clear();
    set_preload_configurator({});
    set_native_ipc_handler({});
    ipc_timer->stop();
    sandbox.stop();
    tracing.reset();
    stop_protocols();
    if (desktop_capture)
        desktop_capture->stop();
    if (notifications)
        notifications->stop();
    if (dialogs)
        dialogs->stop();
    if (global_shortcuts)
        global_shortcuts->stop();
    if (power_monitor)
        power_monitor->stop();
    if (power_save_blocker)
        power_save_blocker->stop();
    if (shell_service)
        shell_service->stop();
    if (system_preferences)
        system_preferences->stop();
    if (network_state)
        network_state->stop();
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
    if (type == "web-request"sv) {
        auto parsed = JsonValue::from_string(data);
        if (parsed.is_error() || !parsed.value().is_object())
            return;
        auto token = parsed.value().as_object().get_u32("token"sv);
        auto epoch = parsed.value().as_object().get_u32("epoch"sv);
        if (!token.has_value() || !epoch.has_value() || !web_request
            || !web_request->current(*token, *epoch))
            return;
    }

    if (type == "permission-request"sv && !permission_event_current(data))
        return;
    if (jobs.is_busy() || (dialogs && dialogs->is_synchronous())) {
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
