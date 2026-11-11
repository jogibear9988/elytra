/** Install trusted main-process bindings; preload/renderer code never receives this host object. */
#include "../../protocol/routes/Host.h"
#include "../../clipboard/ClipboardHost.h"
#include "../../crash/Host.h"
#include "../../ipc/application/Fragment.h"
#include "../../menu/Host.h"
#include "../../platform/DesktopServices.h"
#include "../../protocol/RegistrationHost.h"
#include "../../recent-documents/Host.h"
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
#include <LibJS/Runtime/TypedArray.h>
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
    install_permissions(*host);
    install_web_request(*host);
    install_frames(*host);
    install_background(*host);
    install_zoom(*host);
    install_renderer_ports(*host);
    install_constraints(*host);
    RecentDocuments::install_host(realm, *host, *context);
    host->define_native_function(realm, "spellCheckDisabled"_utf16,
        native_binding<&Impl::native_host_spell_check_disabled>(), 0, JS::default_attributes);
    host->define_native_function(
        realm, "firstMouseAvailable"_utf16,
        +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(first_mouse_available());
        },
        0, JS::default_attributes);
    host->define_native_function(
        realm, "titleBarAvailable"_utf16,
        +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(title_bar_available());
        },
        0, JS::default_attributes);
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
    host->define_native_function(realm, "enableSandbox"_utf16,
        native_binding<&Impl::native_host_enable_sandbox>(), 0, JS::default_attributes);
    install_menu_host(realm, *host, *application_menu);
    install_editing(*host);
    Protocols::install_registration_host(realm, *host);
    Protocols::install_file_route_host(
        realm, *host, NonnullRefPtr<Protocols::FileRoutes> { *protocols });
    host->define_native_function(realm, "setHandler"_utf16,
        native_binding<&Impl::native_host_set_handler>(), 1, JS::default_attributes);
    host->define_native_function(realm, "encodeIpc"_utf16,
        native_binding<&Impl::native_host_encode_ipc>(), 1, JS::default_attributes);
    host->define_native_function(realm, "decodeIpc"_utf16,
        native_binding<&Impl::native_host_decode_ipc>(), 1, JS::default_attributes);
    host->define_native_function(realm, "sendIpc"_utf16,
        native_binding<&Impl::native_host_send_ipc>(), 2, JS::default_attributes);
    host->define_native_function(realm, "replyIpc"_utf16,
        native_binding<&Impl::native_host_reply_ipc>(), 6, JS::default_attributes);
    host->define_native_function(realm, "create"_utf16, native_binding<&Impl::native_host_create>(),
        16, JS::default_attributes);
    host->define_native_function(realm, "destroy"_utf16,
        native_binding<&Impl::native_host_destroy>(), 1, JS::default_attributes);
    host->define_native_function(realm, "destroyNow"_utf16,
        native_binding<&Impl::native_host_destroy_now>(), 1, JS::default_attributes);
    host->define_native_function(
        realm, "exit"_utf16, native_binding<&Impl::native_host_exit>(), 1, JS::default_attributes);
    host->define_native_function(
        realm, "load"_utf16, native_binding<&Impl::native_host_load>(), 2, JS::default_attributes);
    auto method = [&](Utf16View name, void (*action)(ChromeWindow&)) {
        host->define_native_function(realm, Utf16String::from_utf16(name),
            native_binding<&Impl::native_host_window_action>(action), 1, JS::default_attributes);
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
    host->define_native_function(realm, "setWindowBounds"_utf16,
        native_binding<&Impl::native_host_set_window_bounds>(), 6, JS::default_attributes);
    host->define_native_function(realm, "centerWindow"_utf16,
        native_binding<&Impl::native_host_center_window>(), 1, JS::default_attributes);
    host->define_native_function(realm, "windowBounds"_utf16,
        native_binding<&Impl::native_host_window_bounds>(), 2, JS::default_attributes);
    host->define_native_function(realm, "visible"_utf16,
        native_binding<&Impl::native_host_visible>(), 1, JS::default_attributes);
    host->define_native_function(realm, "windowFocused"_utf16,
        native_binding<&Impl::native_host_window_focused>(), 1, JS::default_attributes);
    host->define_native_function(realm, "focusedWindow"_utf16,
        native_binding<&Impl::native_host_focused_window>(), 0, JS::default_attributes);
    host->define_native_function(realm, "minimized"_utf16,
        native_binding<&Impl::native_host_minimized>(), 1, JS::default_attributes);
    host->define_native_function(realm, "maximized"_utf16,
        native_binding<&Impl::native_host_maximized>(), 1, JS::default_attributes);
    host->define_native_function(
        realm, "pid"_utf16, native_binding<&Impl::native_host_pid>(), 1, JS::default_attributes);
    host->define_native_function(realm, "windowFullScreen"_utf16,
        native_binding<&Impl::native_host_fullscreen>(), 1, JS::default_attributes);
    host->define_native_function(realm, "nativeWindowHandle"_utf16,
        native_binding<&Impl::native_host_window_handle>(), 1, JS::default_attributes);
    host->define_native_function(realm, "windowDocumentEdited"_utf16,
        native_binding<&Impl::native_host_document_edited>(), 1, JS::default_attributes);
    host->define_native_function(realm, "setWindowDocumentEdited"_utf16,
        native_binding<&Impl::native_host_set_document_edited>(), 2, JS::default_attributes);
    host->define_native_function(realm, "representedFilename"_utf16,
        native_binding<&Impl::native_host_represented_filename>(), 1, JS::default_attributes);
    host->define_native_function(realm, "setRepresentedFilename"_utf16,
        native_binding<&Impl::native_host_set_represented_filename>(), 2, JS::default_attributes);
    host->define_native_function(realm, "title"_utf16, native_binding<&Impl::native_host_title>(),
        2, JS::default_attributes);
    host->define_native_function(realm, "titleText"_utf16,
        native_binding<&Impl::native_host_title_text>(), 1, JS::default_attributes);
    host->define_native_function(
        realm, "quit"_utf16, native_binding<&Impl::native_host_quit>(), 0, JS::default_attributes);
    install_views(*host);
    return host;
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_set_handler(JS::VM& vm)
{
    if (!vm.argument(0).is_function())
        return vm.throw_completion<JS::TypeError>("Handler must be callable"_utf16);
    handler = vm.argument(0).as_function();
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_encode_ipc(JS::VM& vm)
{
    auto graph = BridgeData::capture_ipc(vm.argument(0), realm, application_ipc_limits());
    if (graph.is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION",
            "IPC accepts bounded data and binary copies only"sv);
    auto encoded = graph.value().encode_wire(application_ipc_limits());
    if (encoded.is_error() || encoded.value().length() > ApplicationIpc::logical_wire_limit)
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "IPC data budget exceeded"sv);
    return wire_value(realm, encoded.value());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_decode_ipc(JS::VM& vm)
{
    auto wire = wire_bytes(vm.argument(0), realm, WireValuePolicy::ApplicationMessage);
    if (wire.is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC wire bytes"sv);
    auto graph = BridgeData::decode_wire(wire.value().view(), application_ipc_limits());
    if (graph.is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC wire graph"sv);
    return graph.value().materialize(realm, {}, BridgeData::Materialization::Mutable);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_send_ipc(JS::VM& vm)
{
    return send_ipc_event(vm);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_reply_ipc(JS::VM& vm)
{
    if (!vm.argument(0).is_int32() || !vm.argument(1).is_string() || !vm.argument(2).is_string()
        || !vm.argument(3).is_number() || !vm.argument(4).is_boolean())
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
    auto wire = wire_bytes(vm.argument(5), realm, WireValuePolicy::ApplicationMessage);
    if (wire.is_error()) {
        retire_ipc_channel(id);
        return JS::js_undefined();
    }

    send_ipc_reply(id, doc, caller, request, vm.argument(4).as_bool(), wire.value());
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_create(JS::VM& vm)
{
    if (!WebView::Application::is_initialized())
        return preload_error(vm, "ERR_FIREFLY_NOT_READY", "Native Hub is not initialized"sv);
    // Independently enforce native launch policy before preload I/O/window creation.
    if (WebView::Application::the().spell_checker())
        return preload_error(vm, "ERR_FIREFLY_UNSUPPORTED_API",
            "Firefly windows require disabled native spellchecking"sv);
    auto zoom_factor = TRY(read_zoom(vm, 14, true));
    auto autoplay_policy = TRY(read_autoplay(vm, 15));
    auto native_enabled
        = WebView::Application::browser_options().disable_sandbox == WebView::DisableSandbox::No;
    if (auto failure = sandbox.check_window(native_enabled); failure.has_value())
        return SandboxPolicy::fail(vm, *failure);
    auto limits = TRY(read_limits(vm));
    auto title_bar = vm.argument(13);
    if (!title_bar.is_undefined() && !title_bar.is_boolean())
        return preload_error(vm, "ERR_INVALID_ARG_TYPE", "Hidden title bar must be a boolean"sv);
    bool hidden_title_bar = title_bar.is_boolean() && title_bar.as_bool();
    if (hidden_title_bar && !title_bar_available())
        return preload_error(
            vm, "ERR_FIREFLY_UNSUPPORTED_API", "Hidden title bar requires the Cocoa backend"sv);
    auto first_mouse = vm.argument(12);
    if (!first_mouse.is_undefined()) {
        if (!first_mouse.is_boolean())
            return preload_error(
                vm, "ERR_INVALID_ARG_TYPE", "acceptFirstMouse must be a boolean"sv);
        if (!first_mouse_available())
            return preload_error(
                vm, "ERR_FIREFLY_UNSUPPORTED_API", "acceptFirstMouse requires the Cocoa backend"sv);
    }

    auto color = vm.argument(10);
    if (!color.is_undefined()
        && (!color.is_int32() || color.as_i32() < 0 || color.as_i32() > 0xffffff))
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid opaque background color"sv);
    auto background = color.is_undefined() ? 0xffffffu : static_cast<u32>(color.as_i32());
    auto title = TRY(vm.argument(2).to_utf16_string(vm));
    auto preload = TRY(read_preload(vm));
    auto id = create(TRY(vm.argument(0).to_i32(vm)), TRY(vm.argument(1).to_i32(vm)),
        qstring_from_utf16_string(title), vm.argument(3).to_boolean(), vm.argument(4).to_boolean(),
        move(preload), vm.argument(6).to_boolean(),
        vm.argument(7).is_undefined() ? Optional<int> {} : TRY(vm.argument(7).to_i32(vm)),
        vm.argument(8).is_undefined() ? Optional<int> {} : TRY(vm.argument(8).to_i32(vm)),
        background, limits,
        first_mouse.is_undefined() ? Optional<bool> {} : Optional<bool> { first_mouse.as_bool() },
        hidden_title_bar, zoom_factor, autoplay_policy);
    if (id.has<WindowCreationError>()) {
        if (id.get<WindowCreationError>() == WindowCreationError::TitleBarPolicy)
            return preload_error(vm, "ERR_FIREFLY_WINDOW_TITLE_BAR",
                "Native title bar policy could not be applied"sv);
        if (id.get<WindowCreationError>() == WindowCreationError::InputPolicy)
            return preload_error(
                vm, "ERR_FIREFLY_WINDOW_INPUT", "Native first mouse policy could not be applied"sv);
        return vm.throw_completion<JS::RangeError>(
            "Window geometry exceeds native coordinates"_utf16);
    }

    return JS::Value(id.get<int>());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_enable_sandbox(JS::VM& vm)
{
    std::optional<bool> selected;
    if (sandbox_launch_policy)
        selected = sandbox_launch_policy();
    else if (WebView::Application::is_initialized())
        selected = WebView::Application::browser_options().disable_sandbox
            == WebView::DisableSandbox::No;
    if (!selected.has_value())
        return preload_error(vm, "ERR_FIREFLY_NOT_READY", "Native launch policy is unavailable"sv);
    if (auto failure = sandbox.enable(*selected); failure.has_value())
        return SandboxPolicy::fail(vm, *failure);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_spell_check_disabled(JS::VM& vm)
{
    if (!WebView::Application::is_initialized())
        return preload_error(vm, "ERR_FIREFLY_NOT_READY", "Native Hub is not initialized"sv);
    return JS::Value(WebView::Application::the().spell_checker() == nullptr);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_destroy(JS::VM& vm)
{
    TRY(window_state(vm));
    destroy(vm.argument(0).as_i32());
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_destroy_now(JS::VM& vm)
{
    if (!vm.argument(0).is_int32())
        return vm.throw_completion<JS::TypeError>("Invalid window ID"_utf16);
    auto found = windows.find(vm.argument(0).as_i32());
    if (found != windows.end()) {
        if (found->second.kind != State::Kind::WindowMain)
            return vm.throw_completion<JS::TypeError>("ID does not identify a BrowserWindow"_utf16);
        destroy(vm.argument(0).as_i32(), true);
    }

    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_exit(JS::VM& vm)
{
    auto code = TRY(vm.argument(0).to_i32(vm));
    exit_code = code;
    stop();
    gui.quit(code);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_load(JS::VM& vm)
{
    auto* current = TRY(state(vm));
    auto input = TRY(vm.argument(1).to_utf16_string(vm));
    auto bytes = input.to_byte_string();
    auto url = URL::Parser::basic_parse(bytes);
    if (!url.has_value())
        return vm.throw_completion<JS::TypeError>("Invalid URL"_utf16);
    // A registered response handler owns this URL. Do not touch the
    // filesystem (even stat) before its authorization/response runs.
    if (url->scheme() == "file"sv && !protocols->intercepts_file()) {
        auto path = url->file_path();
        std::error_code error;
        if (path.contains('\0') || !std::filesystem::is_regular_file(path.characters(), error))
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
    if (current->native_view()->has_traversable())
        current->native_view()->traversable().for_each_hosting_page(
            [this](auto& page) { configure_preloads(page); });
    // A detached/hidden widget may not have delivered its resize event.
    // Upstream pushes its actual dimensions with the current device scale.
    current->view->push_viewport_size();
    current->view->load(*url);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_window_action(
    JS::VM& vm, void (*action)(ChromeWindow&))
{
    auto* current = TRY(window_state(vm));
    action(*current->window);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_set_window_bounds(JS::VM& vm)
{
    auto* current = TRY(window_state(vm));
    auto rect = QRect(TRY(vm.argument(1).to_i32(vm)), TRY(vm.argument(2).to_i32(vm)),
        TRY(vm.argument(3).to_i32(vm)), TRY(vm.argument(4).to_i32(vm)));
    if (!set_window_bounds(*current->window, rect, vm.argument(5).to_boolean()))
        return vm.throw_completion<JS::RangeError>(
            "Window geometry exceeds native coordinates"_utf16);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_center_window(JS::VM& vm)
{
    if (!center_window(*TRY(window_state(vm))->window))
        return vm.throw_completion<JS::Error>("Native window is unavailable"_utf16);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_window_bounds(JS::VM& vm)
{
    auto* current = TRY(window_state(vm));
    auto bounds = window_bounds(*current->window, vm.argument(1).to_boolean());
    auto bytes = QJsonDocument(QJsonObject { { "x", bounds.x() }, { "y", bounds.y() },
                                   { "width", bounds.width() }, { "height", bounds.height() } })
                     .toJson(QJsonDocument::Compact);
    return text(vm, StringView(bytes.data(), bytes.size()));
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_visible(JS::VM& vm)
{
    return JS::Value(TRY(window_state(vm))->window->isVisible());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_window_focused(JS::VM& vm)
{
    return JS::Value(TRY(window_state(vm))->window->isActiveWindow());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_focused_window(JS::VM&)
{
    // Qt activation reflects the current native focus, including focus inside
    // attached child views, rather than previously delivered JS focus events.
    for (auto const& [id, current] : windows) {
        if (current.kind == State::Kind::WindowMain && !current.closing && current.window
            && current.window->isActiveWindow())
            return JS::Value(id);
    }

    return JS::js_null();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_minimized(JS::VM& vm)
{
    return JS::Value(TRY(window_state(vm))->window->isMinimized());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_maximized(JS::VM& vm)
{
    return JS::Value(TRY(window_state(vm))->window->isMaximized());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_pid(JS::VM& vm)
{
    return JS::Value(static_cast<double>(TRY(state(vm))->native_view()->web_content_pid()));
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_fullscreen(JS::VM& vm)
{
    return JS::Value(TRY(window_state(vm))->window->isFullScreen());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_window_handle(JS::VM& vm)
{
    auto* current = TRY(window_state(vm));
#ifdef __APPLE__
    if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
        return preload_error(vm, "ERR_FIREFLY_UNSUPPORTED_API", "Native handle requires Cocoa"sv);
    // Qt's Cocoa WId is the actual NSView*, matching Electron's macOS contract.
    // Copy the native representation directly: never round a pointer through JS Number.
    auto handle = current->window->winId();
    static_assert(sizeof(handle) == sizeof(void*));
    GC::Root bytes { TRY(JS::Uint8Array::create(realm, sizeof(handle))) };
    bytes->viewed_array_buffer()->overwrite(0, &handle, sizeof(handle));
    return bytes.ptr();
#else
    (void)current;
    return preload_error(
        vm, "ERR_FIREFLY_UNSUPPORTED_API", "Native window handle mapping is unavailable"sv);
#endif
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_document_edited(JS::VM& vm)
{
    return JS::Value(TRY(window_state(vm))->window->isWindowModified());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_set_document_edited(JS::VM& vm)
{
    auto* current = TRY(window_state(vm));
    if (!vm.argument(1).is_boolean())
        return preload_error(vm, "ERR_INVALID_ARG_TYPE", "edited must be a boolean"sv);
    // Qt's Cocoa platform forwards this to NSWindow.documentEdited.
    current->window->setWindowModified(vm.argument(1).as_bool());
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_represented_filename(JS::VM& vm)
{
    return JS::PrimitiveString::create(
        vm, utf16_string_from_qstring(TRY(window_state(vm))->window->windowFilePath()));
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_set_represented_filename(JS::VM& vm)
{
    auto* current = TRY(window_state(vm));
    if (!vm.argument(1).is_string())
        return preload_error(vm, "ERR_INVALID_ARG_TYPE", "filename must be a string"sv);
    // The Cocoa backend updates the real representedFilename/document proxy.
    current->window->setWindowFilePath(
        qstring_from_utf16_string(TRY(vm.argument(1).to_utf16_string(vm))));
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_title(JS::VM& vm)
{
    auto* current = TRY(window_state(vm));
    current->window->setWindowTitle(
        qstring_from_utf16_string(TRY(vm.argument(1).to_utf16_string(vm))));
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_title_text(JS::VM& vm)
{
    return JS::PrimitiveString::create(
        vm, utf16_string_from_qstring(TRY(window_state(vm))->window->windowTitle()));
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_quit(JS::VM&)
{
    gui.quit(0);
    return JS::js_undefined();
}

}
