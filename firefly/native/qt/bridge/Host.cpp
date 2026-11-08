/** Install trusted main-process bindings; preload/renderer code never receives this host object. */
#include "../../protocol/routes/Host.h"
#include "../../clipboard/ClipboardHost.h"
#include "../../crash/Host.h"
#include "../../menu/Host.h"
#include "../../platform/DesktopServices.h"
#include "../../protocol/RegistrationHost.h"
#include "../../sandbox/policy/Host.h"
#include "../../tracing/Host.h"
#include "../../window/Geometry.h"
#include "../ChromeWindow.h"
#include "Impl.h"
#include "Value.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include "serialization/WireValue.h"
#include <LibCore/EventLoop.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibURL/Parser.h>
#include <LibWebView/Application.h>
#include <LibWebView/WebContentPage.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <UI/Qt/StringUtils.h>
#include <UI/Qt/WebContentView.h>
#include <filesystem>

namespace Firefly {
using BridgeDetail::text;

GC::Ref<JS::Object> Bridge::Impl::install()
{
    auto host = JS::Object::create(realm, nullptr);
    api = host;
    install_desktop_services(
        realm, *host, *context, [this](char const* event) { enqueue(0, event); });
    desktop_capture
        = DesktopCapture::install_host(realm, *host, *context, [this](std::string data) {
              enqueue(0, "desktop-capture-result", ByteString(data.data(), data.size()));
          });
    notifications = Notifications::install_host(realm, *host, *context, [this](std::string data) {
        enqueue(0, "notification-result", ByteString(data.data(), data.size()));
    });
    power_monitor = PowerMonitor::install_host(realm, *host, *context, [this](std::string data) {
        enqueue(0, "power-monitor-event", ByteString(data.data(), data.size()));
    });
    global_shortcuts
        = GlobalShortcut::install_host(realm, *host, *context, [this](std::string data) {
              enqueue(0, "global-shortcut-event", ByteString(data.data(), data.size()));
          });
    dialogs = Dialogs::install_host(
        realm, *host, *context,
        [this](std::string data) {
            enqueue(0, "dialog-result", ByteString(data.data(), data.size()));
        },
        [this](int id) -> QWidget* {
            auto found = windows.find(id);
            if (found == windows.end() || found->second.kind != State::Kind::WindowMain
                || found->second.closing)
                return nullptr;
            return found->second.window;
        });
    power_save_blocker = PowerSaveBlocker::install_host(realm, *host);
    shell_service = Shell::install_host(realm, *host, *context, [this](std::string data) {
        enqueue(0, "shell-result", ByteString(data.data(), data.size()));
    });
    system_preferences
        = SystemPreferences::install_host(realm, *host, *context, [this](std::string data) {
              enqueue(0, "preferences-notification", ByteString(data.data(), data.size()));
          });
    install_clipboard_host(realm, *host);
    network_state = Network::install_host(realm, *host);
    CrashReporting::install_host(realm, *host);
    Tracing::install_host(realm, *host, tracing);
    SandboxPolicy::install_host(realm, *host, sandbox, [] {
        return WebView::Application::browser_options().disable_sandbox
            == WebView::DisableSandbox::No;
    });
    install_menu_host(realm, *host, *application_menu);
    Protocols::install_registration_host(realm, *host);
    Protocols::install_file_route_host(
        realm, *host, NonnullRefPtr<Protocols::FileRoutes> { *protocols });
    host->define_native_function(
        realm, "setHandler"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_function())
                return vm.throw_completion<JS::TypeError>("Handler must be callable"_utf16);
            handler = vm.argument(0).as_function();
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "encodeIpc"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto graph = BridgeData::capture(vm.argument(0), realm, public_binary_limits());
            if (graph.is_error())
                return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION",
                    "IPC accepts bounded data and binary copies only"sv);
            auto encoded = graph.value().encode_wire(public_binary_limits());
            if (encoded.is_error() || encoded.value().length() > ApplicationIpc::wire_limit)
                return preload_error(
                    vm, "ERR_FIREFLY_IPC_SERIALIZATION", "IPC data budget exceeded"sv);
            return wire_value(realm, encoded.value());
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "decodeIpc"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto wire = wire_bytes(vm.argument(0), realm);
            if (wire.is_error())
                return preload_error(
                    vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC wire bytes"sv);
            auto graph = BridgeData::decode_wire(wire.value().view(), public_binary_limits());
            if (graph.is_error())
                return preload_error(
                    vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC wire graph"sv);
            return graph.value().materialize(realm, {}, BridgeData::Materialization::Mutable);
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "sendIpc"_utf16, [this](JS::VM& vm) { return send_ipc_event(vm); }, 2,
        JS::default_attributes);
    host->define_native_function(
        realm, "replyIpc"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_int32() || !vm.argument(1).is_string()
                || !vm.argument(2).is_string() || !vm.argument(3).is_number()
                || !vm.argument(4).is_boolean())
                return JS::js_undefined();
            auto id = vm.argument(0).as_i32();
            auto request = TRY(vm.argument(3).to_u32(vm));
            auto doc = MUST(vm.argument(1).as_string().utf16_string_view().to_byte_string());
            auto caller = MUST(vm.argument(2).as_string().utf16_string_view().to_byte_string());
            if (!ipc_current(id, doc, caller))
                return JS::js_undefined();
            auto& window = windows.at(id);
            if (!window.ipc_pending.erase(request))
                return JS::js_undefined();
            --ipc_pending_count;
            auto wire = wire_bytes(vm.argument(5), realm);
            if (wire.is_error()) {
                retire_ipc_channel(id);
                return JS::js_undefined();
            }

            send_ipc_reply(id, doc, caller, request, vm.argument(4).as_bool(), wire.value());
            return JS::js_undefined();
        },
        6, JS::default_attributes);
    host->define_native_function(
        realm, "create"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            // Independently enforce native launch policy before preload I/O/window creation.
            auto native_enabled = WebView::Application::browser_options().disable_sandbox
                == WebView::DisableSandbox::No;
            if (auto failure = sandbox.check_window(native_enabled); failure.has_value())
                return SandboxPolicy::fail(vm, *failure);
            auto title = TRY(vm.argument(2).to_utf16_string(vm));
            auto preload = TRY(read_preload(vm));
            auto id = create(TRY(vm.argument(0).to_i32(vm)), TRY(vm.argument(1).to_i32(vm)),
                qstring_from_utf16_string(title), vm.argument(3).to_boolean(),
                vm.argument(4).to_boolean(), move(preload), vm.argument(6).to_boolean(),
                vm.argument(7).is_undefined() ? Optional<int> {} : TRY(vm.argument(7).to_i32(vm)),
                vm.argument(8).is_undefined() ? Optional<int> {} : TRY(vm.argument(8).to_i32(vm)));
            if (!id.has_value())
                return vm.throw_completion<JS::RangeError>(
                    "Window geometry exceeds native coordinates"_utf16);
            return JS::Value(*id);
        },
        10, JS::default_attributes);
    host->define_native_function(
        realm, "destroy"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            TRY(window_state(vm));
            destroy(vm.argument(0).as_i32());
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "destroyNow"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_int32())
                return vm.throw_completion<JS::TypeError>("Invalid window ID"_utf16);
            auto found = windows.find(vm.argument(0).as_i32());
            if (found != windows.end()) {
                if (found->second.kind != State::Kind::WindowMain)
                    return vm.throw_completion<JS::TypeError>(
                        "ID does not identify a BrowserWindow"_utf16);
                destroy(vm.argument(0).as_i32(), true);
            }

            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "exit"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto code = TRY(vm.argument(0).to_i32(vm));
            exit_code = code;
            stop();
            gui.quit(code);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "load"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* current = TRY(state(vm));
            auto input = TRY(vm.argument(1).to_utf16_string(vm));
            auto bytes = input.to_byte_string();
            auto url = URL::Parser::basic_parse(bytes);
            if (!url.has_value())
                return vm.throw_completion<JS::TypeError>("Invalid URL"_utf16);
            if (url->scheme() == "file"sv) {
                auto path = url->file_path();
                std::error_code error;
                if (path.contains('\0')
                    || !std::filesystem::is_regular_file(path.characters(), error))
                    return vm.throw_completion<JS::Error>(
                        "File does not exist or is not a regular file"_utf16);
            }

            if (!url->scheme().is_one_of("file"sv, "http"sv, "https"sv)
                && !protocols->has_route(url->scheme()))
                return preload_error(
                    vm, "ERR_FIREFLY_UNSUPPORTED_API", "URL has no registered file route"sv);
            current->file_picker->cancel();
            current->requested_url = *url;
            current->navigation_requests.clear();
            current->failed = false;
            current->pending = true;
            if (current->view->has_traversable())
                current->view->traversable().for_each_hosting_page(
                    [this](auto& page) { configure_preloads(page); });
            // A detached/hidden widget may not have delivered its resize event.
            // Upstream pushes its actual dimensions with the current device scale.
            current->view->push_viewport_size();
            current->view->load(*url);
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    auto method = [&](Utf16View name, auto action) {
        host->define_native_function(
            realm, Utf16String::from_utf16(name),
            [this, action](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* current = TRY(window_state(vm));
                action(*current->window);
                return JS::js_undefined();
            },
            1, JS::default_attributes);
    };

    method("show"_utf16, [](auto& window) { window.show(); });
    method("minimize"_utf16, [](auto& window) { window.showMinimized(); });
    method("maximize"_utf16, [](auto& window) { window.showMaximized(); });
    method("unmaximize"_utf16, [](auto& window) { window.showNormal(); });
    method("restore"_utf16, [](auto& window) {
        window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
        window.show();
    });
    method("hide"_utf16, [](auto& window) { window.hide(); });
    method("focus"_utf16, [](auto& window) {
        window.raise();
        window.activateWindow();
        window.setFocus();
    });
    host->define_native_function(
        realm, "setWindowBounds"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* current = TRY(window_state(vm));
            auto rect = QRect(TRY(vm.argument(1).to_i32(vm)), TRY(vm.argument(2).to_i32(vm)),
                TRY(vm.argument(3).to_i32(vm)), TRY(vm.argument(4).to_i32(vm)));
            if (!set_window_bounds(*current->window, rect, vm.argument(5).to_boolean()))
                return vm.throw_completion<JS::RangeError>(
                    "Window geometry exceeds native coordinates"_utf16);
            return JS::js_undefined();
        },
        6, JS::default_attributes);
    host->define_native_function(
        realm, "centerWindow"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!center_window(*TRY(window_state(vm))->window))
                return vm.throw_completion<JS::Error>("Native window is unavailable"_utf16);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "windowBounds"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* current = TRY(window_state(vm));
            auto bounds = window_bounds(*current->window, vm.argument(1).to_boolean());
            auto bytes
                = QJsonDocument(QJsonObject { { "x", bounds.x() }, { "y", bounds.y() },
                                    { "width", bounds.width() }, { "height", bounds.height() } })
                      .toJson(QJsonDocument::Compact);
            return text(vm, StringView(bytes.data(), bytes.size()));
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "visible"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(TRY(window_state(vm))->window->isVisible());
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "minimized"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(TRY(window_state(vm))->window->isMinimized());
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "maximized"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(TRY(window_state(vm))->window->isMaximized());
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "pid"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(static_cast<double>(TRY(state(vm))->view->web_content_pid()));
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "title"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* current = TRY(window_state(vm));
            current->window->setWindowTitle(
                qstring_from_utf16_string(TRY(vm.argument(1).to_utf16_string(vm))));
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "titleText"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::PrimitiveString::create(
                vm, utf16_string_from_qstring(TRY(window_state(vm))->window->windowTitle()));
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "quit"_utf16,
        [this](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            gui.quit(0);
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    install_views(*host);
    return host;
}
}
