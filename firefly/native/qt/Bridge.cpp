/**
 * Connects Mite main-process APIs to native Qt windows and Ladybird views. Queue callbacks
 * onto the VM owner thread and disconnect them before runtime teardown.
 */

#include "Bridge.h"
#include "AppRegions.h"
#include "ChromeWindow.h"
#include "PreloadHooks.h"
#include "TransportHooks.h"
#include "core/EventLoop.h"
#include "modules/ModulePaths.h"
#include "serialization/BridgeData.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <LibCore/EventLoop.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibRequests/NetworkError.h>
#include <LibURL/Parser.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <UI/Qt/StringUtils.h>
#include <UI/Qt/WebContentView.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>

#include <unordered_map>
#include <unordered_set>

namespace Firefly {
namespace {
    JS::Value text(JS::VM& vm, StringView value)
    {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8_with_replacement_character(value));
    }
}

struct Bridge::Impl {
    struct Preload {
        String source;
        String filename;
        String dirname;
    };

    struct State {
        ChromeWindow* window;
        Ladybird::WebContentView* view;
        bool closing { false };
        bool painted { false };
        bool failed { false };
        bool pending { false };
        Optional<URL::URL> requested_url;
        std::unordered_set<u64> navigation_requests;
        Optional<Preload> preload;
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
    std::unique_ptr<QObject> context { std::make_unique<QObject>() };
    GC::Root<JS::FunctionObject> handler;
    GC::Root<JS::Object> api;
    std::unordered_map<int, State> windows;
    int next_id { 1 };
    bool stopping { false };
    size_t ipc_pending_count { 0 };

    Impl(JS::VM& vm, JS::Realm& realm, Mite::EventLoop& jobs, Core::EventLoop& gui,
        std::optional<int>& exit_code)
        : vm(vm)
        , realm(realm)
        , jobs(jobs)
        , gui(gui)
        , exit_code(exit_code)
    {
        set_preload_configurator([this](auto&, auto& page) { configure_preloads(page); });
        set_native_ipc_handler([this](auto& page, auto navigable, auto params) {
            receive_ipc(page, navigable, move(params));
        });
    }

    ~Impl()
    {
        stop();
    }

    void stop()
    {
        if (stopping)
            return;
        stopping = true;
        set_preload_configurator({});
        set_native_ipc_handler({});
        while (!windows.empty())
            delete windows.begin()->second.window;
        context.reset(); // Cancel queued Qt callbacks before the Mite VM goes away.
        handler = nullptr;
        api = nullptr;
    }

    void deliver(int id, ByteString const& type, ByteString const& data = {})
    {
        if (stopping || !handler)
            return;
        if (jobs.is_busy()) {
            QTimer::singleShot(
                1, context.get(), [this, id, type, data] { deliver(id, type, data); });
            return;
        }

        jobs.dispatch([&] {
            return JS::call(
                vm, *handler, JS::js_undefined(), JS::Value(id), text(vm, type), text(vm, data));
        });
        if (jobs.status())
            gui.quit(jobs.status());
    }

    void enqueue(int id, ByteString type, ByteString data = {})
    {
        if (stopping)
            return;
        QTimer::singleShot(0, context.get(),
            [this, id, type = move(type), data = move(data)] { deliver(id, type, data); });
    }

    // Navigation/crash invalidates both queued replies and native budget slots.
    void reset_ipc(int id)
    {
        auto found = windows.find(id);
        if (found == windows.end())
            return;
        auto& window = found->second;
        ipc_pending_count -= window.ipc_pending.size();
        window.ipc_pending.clear();
        window.ipc_document = {};
        window.ipc_realm = {};
        window.ipc_last = 0;
    }

    ByteString active_ipc_document(State const& window)
    {
        auto const& doc = window.view->page().traversable().active_document();
        return ByteString::formatted("{}:{}", doc.id().namespace_id, doc.id().local_id);
    }

    bool ipc_current(int id, ByteString const& document, ByteString const& realm_id)
    {
        auto found = windows.find(id);
        return found != windows.end() && !found->second.closing
            && found->second.view->page().routed_connection()
            && found->second.ipc_document == document && found->second.ipc_realm == realm_id
            && active_ipc_document(found->second) == document;
    }

    void dispatch_ipc(int id, ByteString document, ByteString realm_id, ByteString data)
    {
        if (stopping || !ipc_current(id, document, realm_id))
            return;
        if (jobs.is_busy()) {
            QTimer::singleShot(1, context.get(),
                [this, id, document = move(document), realm_id = move(realm_id),
                    data = move(data)] { dispatch_ipc(id, document, realm_id, data); });
            return;
        }

        deliver(id, "ipc-invoke", data);
    }

    // This sends native data only: it is also safe while the Mite VM is busy.
    void send_ipc_reply(State& window, ByteString const& document, ByteString const& realm_id,
        u32 request, bool success, ByteString const& wire)
    {
        JsonObject data;
        data.set("document"sv, MUST(String::from_utf8(document.view())));
        data.set("realm"sv, MUST(String::from_utf8(realm_id.view())));
        data.set("request"sv, request);
        data.set("success"sv, success);
        data.set("wire"sv, MUST(String::from_utf8(wire.view())));
        auto& page = window.view->page();
        page.async_run_webdriver_bidi_command(
            0, page.traversable().id(), "firefly.ipc.reply.v1"sv, move(data));
    }

    void receive_ipc(
        WebView::WebContentPage& page, Web::HTML::CrossProcessId navigable, JsonValue params)
    {
        if (stopping || !params.is_object() || params.as_object().size() != 4)
            return;
        auto const& data = params.as_object();
        auto document = data.get_string("document"sv), realm_id = data.get_string("realm"sv),
             wire = data.get_string("wire"sv);
        auto request = data.get_u32("request"sv);
        if (!document.has_value() || document->bytes().size() > 64 || !realm_id.has_value()
            || realm_id->is_empty() || realm_id->bytes().size() > 1024 || !wire.has_value()
            || wire->bytes().size() > 128 * 1024 || !request.has_value() || *request == 0)
            return;
        auto canonical = page.hosted_navigable(navigable);
        if (!canonical.has_value() || !canonical->is_top_level_traversable()
            || !page.hosts_active_document_of(*canonical))
            return;
        auto const& actual = canonical->active_document();
        auto actual_key
            = ByteString::formatted("{}:{}", actual.id().namespace_id, actual.id().local_id);
        if (document->to_byte_string() != actual_key)
            return;
        for (auto& [id, window] : windows) {
            if (window.closing || !window.preload.has_value() || &page.view() != window.view)
                continue;
            if (active_ipc_document(window) != actual_key)
                return;
            auto caller = realm_id->to_byte_string();
            if (window.ipc_document != actual_key) {
                ipc_pending_count -= window.ipc_pending.size();
                window.ipc_pending.clear();
                window.ipc_document = actual_key;
                window.ipc_realm = caller;
                window.ipc_last = 0;
            }

            if (window.ipc_realm != caller || *request <= window.ipc_last)
                return;
            window.ipc_last = *request;
            if (window.ipc_pending.size() >= 16 || ipc_pending_count >= 64) {
                // Refuse an authenticated request promptly without reserving another
                // main-process slot. Never run an application handler over budget.
                auto wire = MUST(BridgeData::from_string(u"IPC pending request budget exceeded"sv)
                        .encode_wire({}));
                send_ipc_reply(window, actual_key, caller, *request, false, wire);
                return;
            }

            window.ipc_pending.insert(*request);
            ++ipc_pending_count;
            params.as_object().set("url"sv, canonical->active_document_url().serialize());
            auto encoded = params.serialized().to_byte_string();
            QTimer::singleShot(0, context.get(),
                [this, id, document = actual_key, realm_id = caller, data = move(encoded)] {
                    dispatch_ipc(id, document, realm_id, data);
                });
            return;
        }
    }

    JS::ThrowCompletionOr<State*> state(JS::VM& vm)
    {
        if (!vm.argument(0).is_int32())
            return vm.throw_completion<JS::TypeError>("Invalid window ID"_utf16);
        auto found = windows.find(vm.argument(0).as_i32());
        if (found == windows.end() || found->second.closing)
            return vm.throw_completion<JS::Error>("BrowserWindow has been destroyed"_utf16);
        return &found->second;
    }

    void destroy(int id)
    {
        auto found = windows.find(id);
        if (found == windows.end() || found->second.closing)
            return;
        found->second.closing = true;
        found->second.window->hide();
        found->second.window->deleteLater();
    }

    JS::Completion preload_error(JS::VM& vm, char const* code, StringView message)
    {
        auto error
            = JS::Error::create(realm, Utf16String::from_utf8_with_replacement_character(message));
        error->define_direct_property(
            "code"_utf16, text(vm, StringView(code, strlen(code))), JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<Optional<Preload>> read_preload(JS::VM& vm)
    {
        auto value = vm.argument(5);
        if (value.is_undefined())
            return Optional<Preload> {};
        if (!value.is_string())
            return preload_error(vm, "ERR_INVALID_ARG_TYPE", "preload must be a string"sv);
        auto encoded = value.as_string().utf16_string_view().to_byte_string();
        if (encoded.is_error())
            return preload_error(
                vm, "ERR_INVALID_ARG_VALUE", "Preload filename must encode as UTF-8"sv);
        auto bytes = encoded.release_value();
        if (bytes.contains('\0') || bytes.length() > 4096)
            return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid preload filename"sv);
        auto path
            = Mite::ModulePaths::path_from_utf8(std::string(bytes.characters(), bytes.length()));
        if (!path.is_absolute())
            return preload_error(vm, "ERR_INVALID_ARG_VALUE", "preload must be absolute"sv);
        if (path.extension() == ".mjs")
            return preload_error(
                vm, "ERR_FIREFLY_UNSUPPORTED_API", "ES module preloads are not implemented"sv);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
            return preload_error(
                vm, "ERR_FIREFLY_PRELOAD_READ_FAILED", "Preload is not a readable regular file"sv);
        std::ifstream input(path, std::ios::binary);
        std::string contents(65537, '\0');
        input.read(contents.data(), contents.size());
        auto count = static_cast<size_t>(input.gcount());
        if (input.bad() || (!input.eof() && input.fail()))
            return preload_error(vm, "ERR_FIREFLY_PRELOAD_READ_FAILED", "Cannot read preload"sv);
        if (count > 65536)
            return preload_error(vm, "ERR_FIREFLY_PRELOAD_TOO_LARGE", "Preload exceeds 64 KiB"sv);
        contents.resize(count);
        auto source = String::from_utf8(StringView(contents.data(), contents.size()));
        if (source.is_error())
            return preload_error(
                vm, "ERR_FIREFLY_PRELOAD_READ_FAILED", "Preload must be valid UTF-8"sv);
        size_t total = count, configured = 1;
        for (auto const& [id, state] : windows)
            if (state.preload.has_value()) {
                total += state.preload->source.bytes().size();
                ++configured;
            }

        if (total > 1024 * 1024 || configured > 16)
            return preload_error(
                vm, "ERR_FIREFLY_PRELOAD_TOO_LARGE", "Preload window/source budget exceeded"sv);
        auto filename = Mite::ModulePaths::path_string(path);
        auto dirname = Mite::ModulePaths::path_string(path.parent_path());
        return Optional<Preload> { Preload { source.release_value(),
            MUST(String::from_utf8(StringView(filename.data(), filename.size()))),
            MUST(String::from_utf8(StringView(dirname.data(), dirname.size()))) } };
    }

    void configure_preloads(WebView::WebContentPage& page)
    {
        if (stopping)
            return;
        Web::WebDriver::BiDiSessionState state;
        state.is_active = true;
        for (auto const& [id, window] : windows) {
            if (window.closing || !window.preload.has_value())
                continue;
            auto const& preload = *window.preload;
            JsonObject metadata;
            metadata.set("filename"sv, preload.filename);
            metadata.set("dirname"sv, preload.dirname);
            state.preload_scripts.append({ "firefly.preload.v1"_string, preload.source,
                move(metadata), Vector<String> { window.view->handle() }, {}, {} });
        }

        page.client().async_set_webdriver_bidi_session_state(move(state));
    }

    int create(int width, int height, QString const& title, bool show, bool frame,
        Optional<Preload> preload)
    {
        auto id = next_id++;
        auto* window = new ChromeWindow(frame);
        window->setWindowTitle(title);
        window->resize(width, height);
        auto* layout = new QVBoxLayout(window);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* view = new Ladybird::WebContentView(window);
        layout->addWidget(view);
        window->observe(view);
        windows.emplace(
            id, State { window, view, false, false, false, false, {}, {}, move(preload) });
        QObject::connect(window, &QObject::destroyed, context.get(), [this, id] {
            if (auto found = windows.find(id); found != windows.end())
                ipc_pending_count -= found->second.ipc_pending.size();
            windows.erase(id);
            enqueue(id, "closed");
        });
        window->request_close = [this, id] {
            enqueue(id, "close-request");
            return true;
        };

        window->resized = [this, id] { enqueue(id, "resize"); };
        window->state_changed = [this, id](auto before, auto after) {
            if (before.testFlag(Qt::WindowMinimized) != after.testFlag(Qt::WindowMinimized))
                enqueue(id, after.testFlag(Qt::WindowMinimized) ? "minimize" : "restore");
            if (before.testFlag(Qt::WindowMaximized) != after.testFlag(Qt::WindowMaximized))
                enqueue(id, after.testFlag(Qt::WindowMaximized) ? "maximize" : "unmaximize");
        };

        if (!frame) {
            auto* timer = new QTimer(window);
            timer->setInterval(250);
            auto active = std::make_shared<bool>(false), pending = std::make_shared<bool>(false);
            auto epoch = std::make_shared<unsigned>(0), requested = std::make_shared<unsigned>(0);
            view->on_received_js_console_result
                = [window, active, pending, epoch, requested](WebView::DebuggerValue result) {
                      if (!*pending)
                          return;
                      *pending = false;
                      window->set_regions({});
                      if (!*active || *requested != *epoch
                          || result.type != WebView::DebuggerValueType::String
                          || result.text.length_in_code_units() > 65536)
                          return;
                      auto json = JsonValue::from_string(result.text.to_byte_string());
                      std::vector<ChromeWindow::Region> regions;
                      if (!json.is_error() && json.value().is_array()
                          && json.value().as_array().size() <= 256) {
                          for (auto const& value : json.value().as_array().values()) {
                              if (!value.is_array() || value.as_array().size() != 5) {
                                  regions.clear();
                                  break;
                              }

                              auto const& a = value.as_array();
                              bool valid = a.at(4).is_bool();
                              for (int i = 0; i < 4; ++i)
                                  valid = valid && a.at(i).is_integer<int>()
                                      && a.at(i).as_integer<int>() >= 0
                                      && a.at(i).as_integer<int>() <= 32768;
                              if (!valid) {
                                  regions.clear();
                                  break;
                              }

                              regions.push_back(
                                  { QRect(a.at(0).as_integer<int>(), a.at(1).as_integer<int>(),
                                        a.at(2).as_integer<int>(), a.at(3).as_integer<int>())
                                          .intersected(window->rect()),
                                      a.at(4).as_bool() });
                          }
                      }

                      window->set_regions(std::move(regions));
                  };

            QObject::connect(timer, &QTimer::timeout, window, [view, active, pending, epoch, requested] {
                if (!*active || *pending)
                    return;
                *pending = true;
                *requested = *epoch;
                view->js_console_input(
                    "typeof __fireflyRegionSnapshot === 'function' ? __fireflyRegionSnapshot() : '[]'"_string);
            });
            view->on_load_start = [this, id, window, active, epoch] {
                *active = false;
                ++*epoch;
                window->set_regions({});
                enqueue(id, "load-start");
            };
            // Install only browser-side CSS/layout code, with no native bindings.
            view->on_load_finish = [view, active](auto const&) {
                view->run_javascript(MUST(String::from_utf8(StringView(
                    FIREFLY_APP_REGIONS_SOURCE, sizeof(FIREFLY_APP_REGIONS_SOURCE) - 1))));
                *active = true;
            };

            timer->start();
        }

        auto region_start = move(view->on_load_start);
        auto region_finish = move(view->on_load_finish);
        view->on_close = [this, id] { enqueue(id, "close-request"); };
        view->on_load_start = [this, id, region_start = move(region_start)] {
            reset_ipc(id);
            if (region_start)
                region_start();
            else
                enqueue(id, "load-start");
        };

        view->on_load_finish = [this, id, region_finish = move(region_finish)](auto const& url) {
            if (region_finish)
                region_finish(url);
            auto found = windows.find(id);
            if (found == windows.end() || found->second.closing || found->second.failed
                || !found->second.requested_url.has_value())
                return;
            auto& current = found->second;
            if (current.pending && !current.requested_url->equals(url, URL::ExcludeFragment::Yes))
                return;
            current.pending = false;
            enqueue(id, "loaded", url.serialize().to_byte_string());
        };

        view->on_url_change
            = [this, id](auto const& url) { enqueue(id, "url", url.serialize().to_byte_string()); };
        view->on_title_change
            = [this, id](auto const& title) { enqueue(id, "title", title.to_byte_string()); };
        view->on_web_content_crashed = [this, id](auto) {
            reset_ipc(id);
            enqueue(id, "crashed");
        };

        view->on_network_request_started = [this, id](u64 request, auto const& url, auto const&,
                                               auto const&, auto, auto, auto, bool navigation,
                                               auto) {
            auto found = windows.find(id);
            if (found != windows.end() && navigation && found->second.requested_url.has_value()
                && url.equals(*found->second.requested_url, URL::ExcludeFragment::Yes))
                found->second.navigation_requests.insert(request);
        };

        view->on_network_request_finished
            = [this, id](u64 request, auto, auto const&, auto const& error) {
                  auto found = windows.find(id);
                  if (found == windows.end() || !found->second.navigation_requests.erase(request))
                      return;
                  if (error.has_value()) {
                      found->second.failed = true;
                      found->second.pending = false;
                      enqueue(id, "failed", Requests::network_error_to_string(*error));
                  }
              };

        QObject::connect(
            view, &Ladybird::WebContentView::ready_to_paint, context.get(), [this, id] {
                auto found = windows.find(id);
                if (found != windows.end() && !found->second.painted) {
                    found->second.painted = true;
                    enqueue(id, "paint");
                }
            });
        if (show)
            window->show();
        return id;
    }

    GC::Ref<JS::Object> install()
    {
        auto host = JS::Object::create(realm, nullptr);
        api = host;
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
                auto graph = BridgeData::capture(vm.argument(0), realm, {});
                if (graph.is_error())
                    return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION",
                        "IPC accepts bounded plain data only"sv);
                auto encoded = graph.value().encode_wire({});
                if (encoded.is_error() || encoded.value().length() > 128 * 1024)
                    return preload_error(
                        vm, "ERR_FIREFLY_IPC_SERIALIZATION", "IPC data budget exceeded"sv);
                return text(vm, encoded.value());
            },
            1, JS::default_attributes);
        host->define_native_function(
            realm, "decodeIpc"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_string())
                    return preload_error(
                        vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC wire data"sv);
                auto wire = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
                if (wire.length() > 128 * 1024)
                    return preload_error(
                        vm, "ERR_FIREFLY_IPC_SERIALIZATION", "IPC data budget exceeded"sv);
                auto graph = BridgeData::decode_wire(wire, {});
                if (graph.is_error())
                    return preload_error(
                        vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC wire graph"sv);
                return graph.value().materialize(realm, {}, BridgeData::Materialization::Mutable);
            },
            1, JS::default_attributes);
        host->define_native_function(
            realm, "replyIpc"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_int32() || !vm.argument(1).is_string()
                    || !vm.argument(2).is_string() || !vm.argument(3).is_number()
                    || !vm.argument(4).is_boolean() || !vm.argument(5).is_string())
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
                auto wire = MUST(vm.argument(5).as_string().utf16_string_view().to_byte_string());
                if (wire.length() > 128 * 1024)
                    return JS::js_undefined();
                send_ipc_reply(window, doc, caller, request, vm.argument(4).as_bool(), wire);
                return JS::js_undefined();
            },
            6, JS::default_attributes);
        host->define_native_function(
            realm, "create"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto title = TRY(vm.argument(2).to_utf16_string(vm));
                auto preload = TRY(read_preload(vm));
                return JS::Value(create(TRY(vm.argument(0).to_i32(vm)),
                    TRY(vm.argument(1).to_i32(vm)), qstring_from_utf16_string(title),
                    vm.argument(3).to_boolean(), vm.argument(4).to_boolean(), move(preload)));
            },
            6, JS::default_attributes);
        host->define_native_function(
            realm, "destroy"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                TRY(state(vm));
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
                if (found != windows.end())
                    delete found->second.window;
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

                current->requested_url = *url;
                current->navigation_requests.clear();
                current->failed = false;
                current->pending = true;
                if (current->view->has_traversable())
                    current->view->traversable().for_each_hosting_page(
                        [this](auto& page) { configure_preloads(page); });
                current->view->load(*url);
                return JS::js_undefined();
            },
            2, JS::default_attributes);
        auto method = [&](Utf16View name, auto action) {
            host->define_native_function(
                realm, Utf16String::from_utf16(name),
                [this, action](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto* current = TRY(state(vm));
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
            realm, "resize"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* current = TRY(state(vm));
                current->window->resize(
                    TRY(vm.argument(1).to_i32(vm)), TRY(vm.argument(2).to_i32(vm)));
                return JS::js_undefined();
            },
            3, JS::default_attributes);
        host->define_native_function(
            realm, "size"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* current = TRY(state(vm));
                JS::Value values[] { JS::Value(current->window->width()),
                    JS::Value(current->window->height()) };
                return JS::Array::create_from(realm, values);
            },
            1, JS::default_attributes);
        host->define_native_function(
            realm, "visible"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(TRY(state(vm))->window->isVisible());
            },
            1, JS::default_attributes);
        host->define_native_function(
            realm, "minimized"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(TRY(state(vm))->window->isMinimized());
            },
            1, JS::default_attributes);
        host->define_native_function(
            realm, "maximized"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(TRY(state(vm))->window->isMaximized());
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
                auto* current = TRY(state(vm));
                current->window->setWindowTitle(
                    qstring_from_utf16_string(TRY(vm.argument(1).to_utf16_string(vm))));
                return JS::js_undefined();
            },
            2, JS::default_attributes);
        host->define_native_function(
            realm, "titleText"_utf16,
            [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::PrimitiveString::create(
                    vm, utf16_string_from_qstring(TRY(state(vm))->window->windowTitle()));
            },
            1, JS::default_attributes);
        host->define_native_function(
            realm, "quit"_utf16,
            [this](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                gui.quit(0);
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return host;
    }
};

Bridge::Bridge(JS::VM& vm, JS::Realm& realm, Mite::EventLoop& jobs, Core::EventLoop& gui,
    std::optional<int>& exit_code)
    : m_impl(std::make_unique<Impl>(vm, realm, jobs, gui, exit_code))
{
}

Bridge::~Bridge() = default;

GC::Ref<JS::Object> Bridge::exports()
{
    return m_impl->install();
}

void Bridge::ready()
{
    m_impl->enqueue(0, "ready");
}
}
