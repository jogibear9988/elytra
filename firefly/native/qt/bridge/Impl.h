/**
 * Private Qt bridge state shared by implementation units, never an application API.
 * One owner holds windows, GC roots and queued callbacks; stop() invalidates hooks,
 * destroys windows and cancels the QObject context before releasing VM roots.
 */
#pragma once
#include "../Bridge.h"
#include "native/protocol/routes/Files.h"
#include <AK/ByteString.h>
#include <AK/JsonValue.h>
#include <AK/Optional.h>
#include <AK/String.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/Object.h>
#include <LibURL/URL.h>
#include <LibWebCommon/HTML/CrossProcessId.h>
#include <QObject>
#include <QString>
#include <unordered_map>
#include <unordered_set>

namespace Ladybird {
class WebContentView;
}

namespace WebView {
class WebContentPage;
}

namespace Firefly {
class ChromeWindow;

struct Bridge::Impl {
    struct Preload {
        String source;
        String filename;
        String dirname;
    };

    struct State {
        // Qt owns the view through its parent window; destroyed removes this registry entry.
        ChromeWindow* window;
        Ladybird::WebContentView* view;
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
        std::unordered_set<u32> ipc_pending {};
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
    size_t ipc_pending_count { 0 };
    RefPtr<Protocols::FileRoutes> protocols;
    RefPtr<Core::Timer> protocol_timer;

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

    void dispatch_ipc(int, ByteString document, ByteString realm_id, ByteString data);

    void send_ipc_reply(State&, ByteString const& document, ByteString const& realm_id, u32 request,
        bool success, ByteString const& wire);

    void receive_ipc(WebView::WebContentPage&, Web::HTML::CrossProcessId, JsonValue);

    // Window lookup/teardown and native view callbacks share this same owner.
    JS::ThrowCompletionOr<State*> state(JS::VM&);

    void destroy(int);

    Optional<int> create(int width, int height, QString const& title, bool show, bool frame,
        Optional<Preload>, bool use_content_size, Optional<int> x, Optional<int> y);

    // Preload file/source budgets are checked before configuring the native session.
    JS::Completion preload_error(JS::VM&, char const* code, StringView message);

    JS::ThrowCompletionOr<Optional<Preload>> read_preload(JS::VM&);

    void configure_preloads(WebView::WebContentPage&);

    GC::Ref<JS::Object> install();
};
}
