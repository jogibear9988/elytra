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
        return vm.throw_completion<JS::TypeError>("Invalid web contents ID"_utf16);
    auto found = windows.find(vm.argument(0).as_i32());
    if (found == windows.end() || found->second.closing)
        return vm.throw_completion<JS::Error>("WebContents has been destroyed"_utf16);
    return &found->second;
}

JS::ThrowCompletionOr<Bridge::Impl::State*> Bridge::Impl::window_state(JS::VM& vm)
{
    auto* current = TRY(state(vm));
    if (current->kind != State::Kind::WindowMain || !current->window)
        return vm.throw_completion<JS::TypeError>("ID does not identify a BrowserWindow"_utf16);
    return current;
}

void Bridge::Impl::destroy(int id, bool immediate)
{
    Tracing::Recorder::Scope scope(tracing, "firefly.window", "destroy");
    auto found = windows.find(id);
    if (found == windows.end() || found->second.kind != State::Kind::WindowMain
        || (found->second.closing && !immediate))
        return;
    auto& current = found->second;
    current.closing = true;
    while (!current.child_ids.empty())
        detach_view(id, current.child_ids.back());
    if (current.file_picker)
        current.file_picker->cancel();
    current.window->hide();
    if (immediate)
        delete current.window;
    else
        current.window->deleteLater();
}

Variant<int, Bridge::Impl::WindowCreationError> Bridge::Impl::create(int width, int height,
    QString const& title, bool show, bool frame, Optional<Preload> preload, bool use_content_size,
    Optional<int> x, Optional<int> y, u32 background_rgb, WindowLimits const& limits,
    Optional<bool> first_mouse, bool hidden_title_bar, double zoom_factor)
{
    Tracing::Recorder::Scope scope(tracing, "firefly.window", "create");
    if (stopping || windows.size() >= 256 || !WindowSizeConstraints::valid(limits))
        return WindowCreationError::Geometry;
    auto id = next_id++;
    auto* window = new ChromeWindow(frame, hidden_title_bar);
    window->setWindowTitle(title);
    window->resize(width, height);
    if (hidden_title_bar && !configure_title_bar(*window, true)) {
        delete window;
        return WindowCreationError::TitleBarPolicy;
    }

    auto* constraints = new WindowSizeConstraints(*window);
    if (!constraints->initialize(limits, use_content_size)) {
        delete window;
        return WindowCreationError::Geometry;
    }

    auto* layout = new QVBoxLayout(window);
    layout->setContentsMargins(0, 0, 0, 0);
    application_menu->attach(*window);
    auto* content = new QWidget(window);
    layout->addWidget(content);
    auto* content_layout = new QVBoxLayout(content);
    content_layout->setContentsMargins(0, 0, 0, 0);
    auto* view = new Ladybird::WebContentView(content);
    initialize_content_zoom(*view, zoom_factor);
    initialize_content_background(*view, background_rgb);
    content_layout->addWidget(view);
    window->observe(view);
    if ((first_mouse_available() || first_mouse.has_value())
        && !install_first_mouse(*window, *view, first_mouse.value_or(false))) {
        delete window;
        return WindowCreationError::InputPolicy;
    }

    auto bounds = window_bounds(*window, false);
    auto size_bounds = use_content_size ? window_bounds(*window, true) : bounds;
    size_bounds.setSize(QSize(width, height));
    if (!set_window_bounds(*window, size_bounds, use_content_size, false)) {
        delete window;
        return WindowCreationError::Geometry;
    }

    if (x.has_value() || y.has_value()) {
        bounds = window_bounds(*window, false);
        bounds.moveTopLeft(QPoint(x.value_or(bounds.x()), y.value_or(bounds.y())));
        if (!set_window_bounds(*window, bounds, false)) {
            delete window;
            return WindowCreationError::Geometry;
        }
    }

    windows.emplace(id, State { window, view, move(preload) });
    windows.at(id).content_container = content;
    windows.at(id).size_constraints = constraints;
    windows.at(id).accept_first_mouse = first_mouse.value_or(false);
    windows.at(id).background_rgb = background_rgb;

    QObject::connect(window, &QObject::destroyed, context.get(), [this, id] {
        reset_ipc(id);
        windows.erase(id);
        broadcast_background();
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

    if (window->is_custom_chrome()) {
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

    setup_content_callbacks(id);
    broadcast_background();
    if (show)
        window->show();
    return id;
}
}
