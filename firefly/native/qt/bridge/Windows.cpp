/** Create native windows and bind view/navigation/chrome callbacks to their bridge owner. */
#include "../../window/Geometry.h"
#include "../ChromeWindow.h"
#include "AppRegions.h"
#include "Impl.h"
#include <AK/JsonArray.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/VM.h>
#include <LibRequests/NetworkError.h>
#include <LibWebView/WebContentPage.h>
#include <QTimer>
#include <QVBoxLayout>
#include <QWindow>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
JS::ThrowCompletionOr<Bridge::Impl::State*> Bridge::Impl::state(JS::VM& vm)
{
    if (!vm.argument(0).is_int32())
        return vm.throw_completion<JS::TypeError>("Invalid window ID"_utf16);
    auto found = windows.find(vm.argument(0).as_i32());
    if (found == windows.end() || found->second.closing)
        return vm.throw_completion<JS::Error>("BrowserWindow has been destroyed"_utf16);
    return &found->second;
}

void Bridge::Impl::destroy(int id)
{
    Tracing::Recorder::Scope scope(tracing, "firefly.window", "destroy");
    auto found = windows.find(id);
    if (found == windows.end() || found->second.closing)
        return;
    found->second.closing = true;
    found->second.file_picker->cancel();
    found->second.window->hide();
    found->second.window->deleteLater();
}

Optional<int> Bridge::Impl::create(int width, int height, QString const& title, bool show,
    bool frame, Optional<Preload> preload, bool use_content_size, Optional<int> x, Optional<int> y)
{
    Tracing::Recorder::Scope scope(tracing, "firefly.window", "create");
    auto id = next_id++;
    auto* window = new ChromeWindow(frame);
    window->setWindowTitle(title);
    window->resize(width, height);
    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    application_menu->attach(*window);
    auto* view = new Ladybird::WebContentView(window);
    layout->addWidget(view);
    window->observe(view);
    auto bounds = window_bounds(*window, false);
    auto size_bounds = use_content_size ? window_bounds(*window, true) : bounds;
    size_bounds.setSize(QSize(width, height));
    if (!set_window_bounds(*window, size_bounds, use_content_size, false)) {
        delete window;
        return {};
    }

    if (x.has_value() || y.has_value()) {
        bounds = window_bounds(*window, false);
        bounds.moveTopLeft(QPoint(x.value_or(bounds.x()), y.value_or(bounds.y())));
        if (!set_window_bounds(*window, bounds, false)) {
            delete window;
            return {};
        }
    }

    windows.emplace(id, State { window, view, false, false, false, false, {}, {}, move(preload) });
    auto* file_picker
        = new FilePicker(*window, [view](auto files) { view->file_picker_closed(move(files)); });
    windows.at(id).file_picker = file_picker;
    view->on_request_file_picker = [file_picker](auto const& accepted, auto multiple) {
        file_picker->request(accepted, multiple);
    };

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
    // QWindow position signals also cover hidden windows, whose QWidget move
    // events can be deferred until show(). The context owns both connections.
    QObject::connect(window->windowHandle(), &QWindow::xChanged, context.get(),
        [this, id] { enqueue(id, "move"); });
    QObject::connect(window->windowHandle(), &QWindow::yChanged, context.get(),
        [this, id] { enqueue(id, "move"); });
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
        view->on_received_js_console_result = [window, active, pending, epoch, requested](
                                                  WebView::DebuggerValue result) {
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
                        valid = valid && a.at(i).is_integer<int>() && a.at(i).as_integer<int>() >= 0
                            && a.at(i).as_integer<int>() <= 32768;
                    if (!valid) {
                        regions.clear();
                        break;
                    }

                    regions.push_back({ QRect(a.at(0).as_integer<int>(), a.at(1).as_integer<int>(),
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
            view->run_javascript(MUST(String::from_utf8(
                StringView(FIREFLY_APP_REGIONS_SOURCE, sizeof(FIREFLY_APP_REGIONS_SOURCE) - 1))));
            *active = true;
        };

        timer->start();
    }

    auto region_start = move(view->on_load_start);
    auto region_finish = move(view->on_load_finish);
    view->on_close = [this, id] { enqueue(id, "close-request"); };
    view->on_load_start = [this, id, file_picker, region_start = move(region_start)] {
        file_picker->cancel();
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
    view->on_web_content_crashed = [this, id, file_picker](auto) {
        file_picker->cancel();
        reset_ipc(id);
        enqueue(id, "crashed");
    };

    view->on_network_request_started = [this, id](u64 request, auto const& url, auto const&,
                                           auto const&, auto, auto, auto, bool navigation, auto) {
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

    QObject::connect(view, &Ladybird::WebContentView::ready_to_paint, context.get(), [this, id] {
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
}
