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
#include "../../power-monitor/Host.h"
#include "../../power-save-blocker/Host.h"
#include "../../sandbox/policy/Policy.h"
#include "../../shell/Host.h"
#include "../../system-preferences/Host.h"
#include "../../tracing/Recorder.h"
#include "../Bridge.h"
#include "TransportHooks.h"
#include "native/file-picker/Picker.h"
#include "native/protocol/routes/Files.h"
#include <AK/ByteString.h>
#include <AK/JsonValue.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <AK/Time.h>
#include <LibCore/Timer.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/Object.h>
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

struct Bridge::Impl {
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

        // Canonical content identity never changes when its QWidget is reparented.
        // window is non-null only for WindowMain; child ownership is owner_id.
        ChromeWindow* window;
        Ladybird::WebContentView* view;
        Kind kind;
        QWidget* content_container {};
        Optional<int> owner_id;
        std::vector<int> child_ids;
        std::array<int, 4> view_bounds { 0, 0, 0, 0 };
        bool desired_visible { true };
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

    Impl(JS::VM&, JS::Realm&, Mite::EventLoop&, Core::EventLoop&, std::optional<int>&,
        bool electron_compat);

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

    Optional<int> create_view(Optional<Preload>);

    void setup_content_callbacks(int);

    void attach_view(int host_id, int child_id, size_t index);

    void detach_view(int host_id, int child_id);

    void restack_views(int host_id);

    void install_views(JS::Object&);

    Optional<int> create(int width, int height, QString const& title, bool show, bool frame,
        Optional<Preload>, bool use_content_size, Optional<int> x, Optional<int> y);

    // Preload file/source budgets are checked before configuring the native session.
    JS::Completion preload_error(JS::VM&, char const* code, StringView message);

    JS::ThrowCompletionOr<Optional<Preload>> read_preload(
        JS::VM&, size_t preload_argument = 5, size_t arguments_argument = 9);

    JS::ThrowCompletionOr<JsonArray> read_process_arguments(JS::VM&, size_t argument = 9);

    JS::ThrowCompletionOr<PreloadProcess::Configuration> read_process_config(JS::VM&, JsonArray);

    void configure_preloads(WebView::WebContentPage&);

    GC::Ref<JS::Object> install();
};
}
