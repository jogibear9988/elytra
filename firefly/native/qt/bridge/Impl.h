/**
 * Private Qt bridge state shared by implementation units, never an application API.
 * One owner holds windows, GC roots and queued callbacks; stop() invalidates hooks,
 * destroys windows and cancels the QObject context before releasing VM roots.
 */
#pragma once
#include "../../desktop-capture/Host.h"
#include "../../dialog/Host.h"
#include "../../global-shortcut/Host.h"
#include "../../menu/ApplicationMenu.h"
#include "../../net/Host.h"
#include "../../notifications/Host.h"
#include "../../platform/GCIncludes.h"
#include "../../power-monitor/Host.h"
#include "../../power-save-blocker/Host.h"
#include "../../sandbox/policy/Policy.h"
#include "../../shell/Host.h"
#include "../../system-preferences/Host.h"
#include "../../tracing/Recorder.h"
#include "../Bridge.h"
#include "../ContentView.h"
#include "../hub/Frontend.h"
#include "TransportHooks.h"
#include "core/GCCaptures.h"
#include "native/file-picker/Picker.h"
#include "native/frames/Registry.h"
#include "native/permissions/Hooks.h"
#include "native/protocol/routes/Files.h"
#include "native/web-request/Broker.h"
#include "native/window/Constraints.h"
#include "native/window/FirstMouse.h"
#include "native/window/TitleBar.h"
#include "renderer-bridge/autoplay/Policy.h"
#include <AK/ByteString.h>
#include <AK/JsonValue.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <AK/Variant.h>
#include <LibCore/Timer.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <LibURL/URL.h>
#include <LibWebCommon/HTML/CrossProcessId.h>
#include <QObject>
#include <QString>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Ladybird {
class WebContentView;
}

namespace WebView {
class WebContentPage;
}

#include "native/ipc/application/Message.h"
#include "native/ipc/bytes/Channel.h"
#include "native/preload/ProcessConfig.h"

namespace Firefly {
class ChromeWindow;

struct Bridge::Impl : public Mite::NativeCaptureOwner<Bridge::Impl> {
    void visit_capture_edges(GC::Cell::Visitor& visitor) const
    {
        visitor.visit(handler.ptr());

        visitor.visit(api.ptr());

        visitor.visit(permission_check.ptr());
    }

    static JS::Completion retired_binding(JS::VM&);

    // A method pointer is native code; the only borrowed object travels through
    // the explicit revocable owner anchor, including during final VM collection.
    template <auto method> auto native_binding()
    {
        return GC::create_function(realm.heap(), GC::captures(Mite::native_reference(*this)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* current = owner.try_get();

                if (!current)
                    return retired_binding(vm);

                return (current->*method)(vm);
            });
    }

    template <auto method, typename Extra> auto native_binding(Extra extra)
    {
        return GC::create_function(realm.heap(),
            GC::captures(Mite::native_reference(*this), move(extra)),
            [](auto& owner, auto& argument, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* current = owner.try_get();

                if (!current)
                    return retired_binding(vm);

                return (current->*method)(vm, argument);
            });
    }

    struct Preload {
        String source;
        String filename;
        String dirname;
        PreloadProcess::Configuration process;
    };

    struct State {
        enum class Kind { WindowMain, ChildView };

        State(ChromeWindow* host, Ladybird::WebContentView* content, Optional<Preload> script,
            Kind content_kind = Kind::WindowMain)
            : window(host)
            , view(content)
            , kind(content_kind)
            , preload(move(script))
        {
        }

        WebView::ViewImplementation* native_view() const;

        bool owns_page(WebView::WebContentPage const&) const;

        // Canonical content identity never changes when its QWidget is reparented.
        // window is non-null only for WindowMain; child ownership is owner_id.
        ChromeWindow* window;
        Ladybird::WebContentView* view;
        Kind kind;
        QWidget* content_container {};
        WindowSizeConstraints* size_constraints {};
        bool accept_first_mouse { false };
        Optional<int> owner_id;
        std::vector<int> child_ids;
        std::array<int, 4> view_bounds { 0, 0, 0, 0 };
        bool desired_visible { true };
        Autoplay::Policy autoplay_policy { Autoplay::Policy::UpstreamDefault };
        u32 background_rgb { 0xffffff };
        u32 background_generation { 1 };
        bool closing { false };
        // Navigation and first-paint bookkeeping are distinct from IPC document ownership.
        bool painted { false };
        bool failed { false };
        bool pending { false };
        Optional<URL::URL> requested_url;
        std::unordered_set<u64> navigation_requests;
        Optional<Preload> preload;
        // Canonical sender keys and outstanding IDs are invalidated on navigation/crash.
        ByteString ipc_document {};
        ByteString ipc_realm {};
        u32 ipc_last { 0 };
        u32 ipc_control_last { 0 };

        struct RawRequest {
            bool event { false };
            bool marker { false };
            bool payload { false };
            bool history { false };
            ByteString wire {};
            ByteString url {};
            MonotonicTime deadline {};
        };

        std::unordered_map<u32, RawRequest> ipc_packets {};
        std::unordered_map<u32, bool> ipc_refused {};
        RefPtr<Bytes::Channel> ipc_byte_channel {};
        u64 ipc_byte_epoch { 0 };
        bool ipc_byte_ready { false };
        Vector<ByteString> ipc_byte_queue {};
        Vector<RefPtr<Bytes::Channel>> ipc_admission_probes {};
        Optional<MonotonicTime> ipc_byte_deadline {};
        std::unordered_set<u32> ipc_pending {};
        bool ipc_outgoing_failed { false };
        u32 ipc_outgoing_next { 0 };
        std::unordered_map<u32, MonotonicTime> ipc_outgoing {};
        FilePicker* file_picker { nullptr };
    };

    JS::VM& vm;
    JS::Realm& realm;
    Mite::EventLoop& jobs;
    Core::EventLoop& gui;
    std::optional<int>& exit_code;
    bool electron_compat;
    std::function<std::optional<bool>()> sandbox_launch_policy;
    std::unique_ptr<QObject> context { std::make_unique<QObject>() };
    GC::Root<JS::FunctionObject> handler;
    GC::Root<JS::Object> api;
    std::unordered_map<int, State> windows;
    int next_id { 1 };
    bool stopping { false };
    u64 ipc_byte_next_epoch { 0 };
    size_t ipc_pending_count { 0 };
    size_t ipc_outgoing_count { 0 };
    RefPtr<Core::Timer> ipc_timer;
    std::unique_ptr<ApplicationMenu> application_menu;
    Tracing::Recorder tracing;
    SandboxPolicy::Policy sandbox;
    RefPtr<Protocols::FileRoutes> protocols;
    RefPtr<Core::Timer> protocol_timer;
    std::shared_ptr<DesktopCapture::Controller> desktop_capture;
    std::shared_ptr<Notifications::Controller> notifications;
    std::shared_ptr<PowerMonitor::Controller> power_monitor;
    std::shared_ptr<GlobalShortcut::Controller> global_shortcuts;
    std::shared_ptr<Dialogs::Controller> dialogs;
    std::shared_ptr<PowerSaveBlocker::Controller> power_save_blocker;
    std::shared_ptr<Shell::Controller> shell_service;
    std::shared_ptr<SystemPreferences::Controller> system_preferences;
    std::shared_ptr<NetworkState> network_state;
    RefPtr<WebRequest::Broker> web_request;
    void initialize_web_request();

    void install_web_request(JS::Object&);

    Optional<JsonObject> authenticate_web_request(
        WebView::WebContentClient&, WebRequest::Packet const&);

    Frames::Registry frames;
    void install_frames(JS::Object&);

    struct PermissionPending {
        int contents;
        u64 epoch;
        RefPtr<WebView::WebContentPage> page;
        Permissions::DocumentId document;
        Permissions::Name name;
        Permissions::Decision decision;
        MonotonicTime deadline;
    };

    struct PermissionGrant {
        int contents;
        Permissions::DocumentId document;
        Permissions::Name name;
    };

    u64 permission_epoch { 0 };
    int permission_next { 0 };
    bool permission_requests { false };
    GC::Root<JS::FunctionObject> permission_check;
    std::unordered_map<int, PermissionPending> permission_pending;
    std::vector<PermissionGrant> permission_grants;
    void initialize_permissions();

    void install_permissions(JS::Object&);

    void broadcast_permissions();

    void configure_permission_page(WebView::WebContentPage&);

    void reset_permissions(int);

    void expire_permissions();

    bool permission_event_current(ByteString const&);

    int permission_contents(WebView::WebContentPage&, Permissions::DocumentId);

    bool check_permission(
        WebView::WebContentPage&, Permissions::DocumentId, u64, Permissions::Name);

    void request_permission(WebView::WebContentPage&, Permissions::DocumentId, Permissions::Name,
        Permissions::Decision);

    bool complete_permission(int, u64, bool);

    Impl(JS::VM&, JS::Realm&, Mite::EventLoop&, Core::EventLoop&, std::optional<int>&,
        bool electron_compat, std::function<std::optional<bool>()> sandbox_launch_policy);

    ~Impl();

    // VM-owner-thread delivery; busy VM frames defer through the owned Qt context.
    void stop();

    void initialize_protocols();

    void stop_protocols();

    void deliver_protocol(u64 token, ByteString type, ByteString metadata);

    void deliver(int, ByteString const&, ByteString const& = {});

    void enqueue(int, ByteString, ByteString = {});

    // Authenticated document/realm/request admission and pending-budget ownership.
    void reset_ipc(int);

    ByteString active_ipc_document(State const&);

    bool ipc_current(int, ByteString const& document, ByteString const& realm_id);

    void send_ipc_reply(int, ByteString const& document, ByteString const& realm_id, u32 request,
        bool success, ByteString const& wire);

    void receive_ipc(WebView::WebContentPage&, Web::HTML::CrossProcessId, JsonValue, NativeIpcKind);

    void bind_ipc_channel(int, WebView::WebContentPage&, Web::HTML::CrossProcessId,
        Web::HTML::CrossProcessId document);

    void probe_ipc_handoff(int, WebView::WebContentPage&, Web::HTML::CrossProcessId,
        Web::HTML::CrossProcessId document, u64 epoch, ByteString realm_id, u8 scenario);

    void retire_ipc_channel(int);

    bool queue_ipc_packet(int, ApplicationIpc::Kind, u32, bool, ByteString const&);

    void flush_ipc_packets(int, u64 epoch);

    void receive_ipc_packet(int, u64 epoch, ReadonlyBytes);

    bool admit_ipc_half(int, u32 request, bool marker, bool event);

    void ready_ipc_packet(int, u32 request);

    void dispatch_ipc_packet(int, ByteString document, ByteString realm_id, u32 request, bool event,
        ByteString wire, ByteString url);

    JS::ThrowCompletionOr<JS::Value> send_ipc_event(JS::VM&);

    void expire_ipc_events();

    // Window lookup/teardown and native view callbacks share this same owner.
    JS::ThrowCompletionOr<State*> state(JS::VM&);

    JS::ThrowCompletionOr<State*> window_state(JS::VM&);

    JS::ThrowCompletionOr<State*> view_state(JS::VM&, size_t argument = 0);

    void destroy(int, bool immediate = false);

    void close_contents(int, bool immediate = false);

    Optional<int> create_view(Optional<Preload>, double zoom_factor = 1.0,
        Autoplay::Policy autoplay_policy = Autoplay::Policy::UpstreamDefault);

    void setup_content_callbacks(int);

    bool attach_view(int host_id, int child_id, size_t index);

    void detach_view(int host_id, int child_id);

    void restack_views(int host_id);

    void install_views(JS::Object&);

    void install_background(JS::Object&);

    void install_zoom(JS::Object&);

    JS::ThrowCompletionOr<Autoplay::Policy> read_autoplay(JS::VM&, size_t index);

    static JS::ThrowCompletionOr<double> read_zoom(JS::VM&, size_t index, bool optional = false);

    void install_constraints(JS::Object&);

    JS::ThrowCompletionOr<WindowLimits> read_limits(JS::VM&, size_t argument = 11);

    void broadcast_background();

    enum class WindowCreationError { Geometry, InputPolicy, TitleBarPolicy };
    Variant<int, WindowCreationError> create(int width, int height, QString const& title, bool show,
        bool frame, Optional<Preload>, bool use_content_size, Optional<int> x, Optional<int> y,
        u32 background_rgb = 0xffffff, WindowLimits const& limits = {},
        Optional<bool> first_mouse = {}, bool hidden_title_bar = false, double zoom_factor = 1.0,
        Autoplay::Policy autoplay_policy = Autoplay::Policy::UpstreamDefault);

    // Preload file/source budgets are checked before configuring the native session.
    JS::Completion preload_error(JS::VM&, char const* code, StringView message);

    JS::ThrowCompletionOr<Optional<Preload>> read_preload(
        JS::VM&, size_t preload_argument = 5, size_t arguments_argument = 9);

    JS::ThrowCompletionOr<JsonArray> read_process_arguments(JS::VM&, size_t argument = 9);

    JS::ThrowCompletionOr<PreloadProcess::Configuration> read_process_config(JS::VM&, JsonArray);

    void configure_preloads(WebView::WebContentPage&);

    // Typed native bodies remain ordinary member methods; GC closures hold only declared captures.
    JS::ThrowCompletionOr<JS::Value> native_background_set_contents_background(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_background_contents_background(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_background_lookup_color(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_constraints_get_constraint(JS::VM&, bool maximum);

    JS::ThrowCompletionOr<JS::Value> native_constraints_set_constraint(JS::VM&, bool maximum);

    JS::ThrowCompletionOr<JS::Value> native_frames_snapshot(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_set_handler(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_encode_ipc(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_decode_ipc(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_send_ipc(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_reply_ipc(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_create(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_destroy(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_destroy_now(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_exit(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_load(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_window_action(
        JS::VM&, void (*action)(ChromeWindow&));

    JS::ThrowCompletionOr<JS::Value> native_host_set_window_bounds(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_center_window(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_window_bounds(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_visible(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_minimized(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_maximized(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_pid(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_document_edited(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_set_document_edited(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_represented_filename(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_set_represented_filename(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_title(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_title_text(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_quit(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_permissions_configure(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_permissions_complete(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_create_view(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_attach_view(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_detach_view(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_set_view_bounds(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_view_bounds(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_set_view_visible(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_view_visible(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_close_contents(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_focus_contents(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_owner_window(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_child_view_ids(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_views_focused_contents(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_webrequest_configure(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_webrequest_complete(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_zoom_set_contents_zoom(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_zoom_contents_zoom(JS::VM&);

    GC::Ref<JS::Object> install();

    JS::ThrowCompletionOr<JS::Value> native_host_enable_sandbox(JS::VM&);

    JS::ThrowCompletionOr<JS::Value> native_host_spell_check_disabled(JS::VM&);
};
}
