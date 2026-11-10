/** Canonical web contents outlive presentation hosts; attachment only reparents the same widget. */
#include "../../sandbox/policy/Host.h"
#include "../ChromeWindow.h"
#include "Impl.h"
#include "Value.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibRequests/NetworkError.h>
#include <LibWebView/Application.h>
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <UI/Qt/WebContentView.h>
#include <algorithm>
#include <limits>

namespace Firefly {
WebView::ViewImplementation* Bridge::Impl::State::native_view() const
{
    auto native = HubFrontend::canonical_view(*view);
    return native.has_value() ? &*native : nullptr;
}

bool Bridge::Impl::State::owns_page(WebView::WebContentPage const& page) const
{
    return !closing && HubFrontend::owns_page(*view, page);
}

namespace {
    void move_content_widget(Ladybird::WebContentView& view, QWidget* parent)
    {
        if (view.parentWidget() == parent)
            return;
        // Upstream moves the dropdown to the new top-level owner and recreates
        // RHI surfaces where required. Without finish_window_move(), closing the
        // old host can delete a still-live view's dropdown. Canonical Hub identity
        // and preload/IPC state are unaffected by this presentation-only move.
        view.prepare_for_window_move();
        view.setParent(parent);
        view.finish_window_move();
        // RHI completion shows the native widget. Restore public visibility only
        // after the caller has committed ownership and installed its input policy.
        view.hide();
    }

    QRect native_bounds(std::array<int, 4> const& bounds)
    {
        auto x = bounds[0], y = bounds[1];
        // QRect stores inclusive lower/right edges. An empty dimension at
        // INT_MIN would underflow its checked x+width-1 representation. Keep
        // the public logical coordinate; this invisible physical edge shifts 1px.
        if (!bounds[2] && x == std::numeric_limits<int>::min())
            ++x;
        if (!bounds[3] && y == std::numeric_limits<int>::min())
            ++y;
        return QRect(x, y, bounds[2], bounds[3]);
    }
}

JS::ThrowCompletionOr<Bridge::Impl::State*> Bridge::Impl::view_state(JS::VM& vm, size_t argument)
{
    auto value = vm.argument(argument);
    if (!value.is_int32())
        return vm.throw_completion<JS::TypeError>("Invalid view ID"_utf16);
    auto found = windows.find(value.as_i32());
    if (found == windows.end() || found->second.closing)
        return vm.throw_completion<JS::Error>("WebContentsView has been destroyed"_utf16);
    if (found->second.kind != State::Kind::ChildView)
        return vm.throw_completion<JS::TypeError>("ID does not identify a WebContentsView"_utf16);
    return &found->second;
}

Optional<int> Bridge::Impl::create_view(Optional<Preload> preload, double zoom_factor)
{
    if (stopping)
        return {};
    size_t count = 0;
    for (auto const& [id, current] : windows)
        count += current.kind == State::Kind::ChildView;
    if (windows.size() >= 256 || count >= 64)
        return {};
    auto id = next_id++;
    auto* view = new Ladybird::WebContentView(nullptr);
    initialize_content_zoom(*view, zoom_factor);
    initialize_content_background(*view);
    view->hide();
    view->setGeometry(0, 0, 0, 0);
    windows.emplace(id, State { nullptr, view, move(preload), State::Kind::ChildView });
    setup_content_callbacks(id);
    broadcast_background();
    return id;
}

void Bridge::Impl::restack_views(int host_id)
{
    auto found = windows.find(host_id);
    if (found == windows.end() || found->second.kind != State::Kind::WindowMain)
        return;
    found->second.view->lower();
    for (auto id : found->second.child_ids)
        if (auto child = windows.find(id); child != windows.end() && !child->second.closing)
            child->second.view->raise();
}

void Bridge::Impl::detach_view(int host_id, int child_id)
{
    auto found = windows.find(child_id);
    if (found == windows.end() || found->second.kind != State::Kind::ChildView
        || found->second.owner_id != host_id)
        return;
    auto& child = found->second;
    if (child.file_picker)
        child.file_picker->cancel();
    child.view->hide();
    if (auto host = windows.find(host_id); host != windows.end()) {
        auto& ids = host->second.child_ids;
        ids.erase(std::remove(ids.begin(), ids.end(), child_id), ids.end());
    }

    child.owner_id.clear();
    clear_first_mouse(*child.view);
    move_content_widget(*child.view, nullptr);
    child.view->setGeometry(native_bounds(child.view_bounds));
    child.view->push_viewport_size();
}

bool Bridge::Impl::attach_view(int host_id, int child_id, size_t index)
{
    auto& host = windows.at(host_id);
    auto& child = windows.at(child_id);
    auto previous_owner = child.owner_id;
    auto* previous_parent = child.view->parentWidget();
    if (child.file_picker)
        child.file_picker->cancel();
    child.view->hide();
    clear_first_mouse(*child.view);
    move_content_widget(*child.view, host.content_container);
    // An attached native child must inherit its current host's first-click
    // policy. Commit ownership only after the real platform view accepted it.
    if (first_mouse_available()
        && !install_first_mouse(*host.window, *child.view, host.accept_first_mouse)) {
        clear_first_mouse(*child.view);
        move_content_widget(*child.view, previous_parent);
        if (previous_owner.has_value()) {
            auto& previous = windows.at(*previous_owner);
            if (!install_first_mouse(*previous.window, *child.view, previous.accept_first_mouse)) {
                // Never leave a surviving child with JS ownership that no
                // longer matches its native input policy after failed rollback.
                close_contents(child_id, true);
                return false;
            }

            child.view->setVisible(child.desired_visible);
            restack_views(*previous_owner);
        }

        child.view->setGeometry(native_bounds(child.view_bounds));
        child.view->push_viewport_size();
        return false;
    }

    if (previous_owner.has_value()) {
        auto& previous = windows.at(*previous_owner).child_ids;
        previous.erase(std::remove(previous.begin(), previous.end(), child_id), previous.end());
    }

    child.owner_id = host_id;
    child.view->setGeometry(native_bounds(child.view_bounds));
    child.view->push_viewport_size();
    auto& ids = host.child_ids;
    ids.insert(ids.begin() + std::min(index, ids.size()), child_id);
    child.view->setVisible(child.desired_visible);
    restack_views(host_id);
    return true;
}

void Bridge::Impl::close_contents(int id, bool immediate)
{
    auto found = windows.find(id);
    if (found == windows.end())
        return;
    auto& current = found->second;
    if (current.kind == State::Kind::WindowMain) {
        destroy(id, immediate);
        return;
    }

    if (current.closing && !immediate)
        return;
    if (current.owner_id.has_value())
        detach_view(*current.owner_id, id);
    if (!current.closing) {
        current.closing = true;
        if (current.file_picker)
            current.file_picker->cancel();
        reset_ipc(id);
    }

    current.view->hide();
    if (immediate)
        delete current.view;
    else
        current.view->deleteLater();
}

void Bridge::Impl::setup_content_callbacks(int id)
{
    auto& current = windows.at(id);
    auto* view = current.view;
    auto weak_view = QPointer<Ladybird::WebContentView>(view);
    auto* file_picker = new FilePicker(*view, [weak_view](auto files) {
        if (weak_view)
            weak_view->file_picker_closed(move(files));
    });
    current.file_picker = file_picker;
    view->on_request_file_picker = [this, id, weak_view](auto const& accepted, auto multiple) {
        auto found = windows.find(id);
        if (found == windows.end() || found->second.closing || !weak_view)
            return;
        auto& current = found->second;
        if (current.kind == State::Kind::ChildView && !current.owner_id.has_value()) {
            weak_view->file_picker_closed({});
            return;
        }

        current.file_picker->request(accepted, multiple);
    };

    QObject::connect(view, &QObject::destroyed, context.get(), [this, id] {
        auto found = windows.find(id);
        if (found == windows.end() || found->second.kind != State::Kind::ChildView)
            return;
        auto& current = found->second;
        if (current.owner_id.has_value()) {
            auto host = windows.find(*current.owner_id);
            if (host != windows.end()) {
                auto& ids = host->second.child_ids;
                ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
            }
        }

        if (!current.closing)
            reset_ipc(id);
        windows.erase(id);
        broadcast_background();
        enqueue(id, "contents-destroyed");
    });
    auto region_start = move(view->on_load_start);
    auto region_finish = move(view->on_load_finish);
    view->on_close = [this, id] {
        auto found = windows.find(id);
        if (found != windows.end() && !found->second.closing)
            enqueue(id,
                found->second.kind == State::Kind::WindowMain ? "close-request"
                                                              : "contents-close-request");
    };

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

    auto* native = current.native_view();
    VERIFY(native);
    auto lifetime = QPointer<QObject>(context.get());
    auto network_start = move(native->on_network_request_started);
    native->on_network_request_started
        = [this, id, lifetime, weak_view, original = move(network_start)](u64 request,
              auto const& url, auto const& method, auto const& headers, auto body, auto detail,
              auto type, bool navigation, auto priority) {
              if (original)
                  original(request, url, method, headers, move(body), move(detail), move(type),
                      navigation, priority);
              // The Hub view can outlive its presentation widget and bridge while
              // its destroy-view message is still queued. Never borrow the UI owner then.
              if (!lifetime || !weak_view)
                  return;
              auto found = windows.find(id);
              if (found != windows.end() && !found->second.closing && navigation
                  && found->second.requested_url.has_value()
                  && url.equals(*found->second.requested_url, URL::ExcludeFragment::Yes))
                  found->second.navigation_requests.insert(request);
          };

    auto network_finish = move(native->on_network_request_finished);
    native->on_network_request_finished
        = [this, id, lifetime, weak_view, original = move(network_finish)](
              u64 request, auto bytes, auto const& timing, auto const& error) {
              if (original)
                  original(request, bytes, timing, error);
              if (!lifetime || !weak_view)
                  return;
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
}

void Bridge::Impl::install_views(JS::Object& host)
{
    host.define_native_function(realm, "createView"_utf16,
        native_binding<&Impl::native_views_create_view>(), 3, JS::default_attributes);
    host.define_native_function(realm, "attachView"_utf16,
        native_binding<&Impl::native_views_attach_view>(), 3, JS::default_attributes);
    host.define_native_function(realm, "detachView"_utf16,
        native_binding<&Impl::native_views_detach_view>(), 2, JS::default_attributes);
    host.define_native_function(realm, "setViewBounds"_utf16,
        native_binding<&Impl::native_views_set_view_bounds>(), 5, JS::default_attributes);
    host.define_native_function(realm, "viewBounds"_utf16,
        native_binding<&Impl::native_views_view_bounds>(), 1, JS::default_attributes);
    host.define_native_function(realm, "setViewVisible"_utf16,
        native_binding<&Impl::native_views_set_view_visible>(), 2, JS::default_attributes);
    host.define_native_function(realm, "viewVisible"_utf16,
        native_binding<&Impl::native_views_view_visible>(), 1, JS::default_attributes);
    host.define_native_function(realm, "closeContents"_utf16,
        native_binding<&Impl::native_views_close_contents>(), 1, JS::default_attributes);
    host.define_native_function(realm, "focusContents"_utf16,
        native_binding<&Impl::native_views_focus_contents>(), 1, JS::default_attributes);
    host.define_native_function(realm, "ownerWindow"_utf16,
        native_binding<&Impl::native_views_owner_window>(), 1, JS::default_attributes);
    host.define_native_function(realm, "childViewIds"_utf16,
        native_binding<&Impl::native_views_child_view_ids>(), 1, JS::default_attributes);
    host.define_native_function(realm, "focusedContents"_utf16,
        native_binding<&Impl::native_views_focused_contents>(), 0, JS::default_attributes);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_create_view(JS::VM& vm)
{
    if (!WebView::Application::is_initialized())
        return preload_error(vm, "ERR_FIREFLY_NOT_READY", "Native Hub is not initialized"sv);
    // All current Firefly views run without a native spelling provider;
    // recheck that invariant even if main-process validation was bypassed.
    if (WebView::Application::the().spell_checker())
        return preload_error(vm, "ERR_FIREFLY_UNSUPPORTED_API",
            "Firefly views require disabled native spellchecking"sv);
    auto native_enabled
        = WebView::Application::browser_options().disable_sandbox == WebView::DisableSandbox::No;
    if (auto failure = sandbox.check_window(native_enabled); failure.has_value())
        return SandboxPolicy::fail(vm, *failure);
    auto zoom_factor = TRY(read_zoom(vm, 2, true));
    auto id = create_view(TRY(read_preload(vm, 0, 1)), zoom_factor);
    if (!id.has_value())
        return vm.throw_completion<JS::RangeError>("Native view budget exceeded"_utf16);
    return JS::Value(*id);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_attach_view(JS::VM& vm)
{
    auto* host = TRY(window_state(vm));
    auto* child = TRY(view_state(vm, 1));
    size_t index = host->child_ids.size();
    if (!vm.argument(2).is_undefined()) {
        if (!vm.argument(2).is_int32() || vm.argument(2).as_i32() < 0)
            return vm.throw_completion<JS::RangeError>(
                "View index must be a nonnegative integer"_utf16);
        index = static_cast<size_t>(vm.argument(2).as_i32());
    }

    (void)child;
    if (!attach_view(vm.argument(0).as_i32(), vm.argument(1).as_i32(), index))
        return preload_error(vm, "ERR_FIREFLY_WINDOW_INPUT",
            "Native child first mouse policy could not be applied"sv);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_detach_view(JS::VM& vm)
{
    TRY(window_state(vm));
    TRY(view_state(vm, 1));
    detach_view(vm.argument(0).as_i32(), vm.argument(1).as_i32());
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_set_view_bounds(JS::VM& vm)
{
    auto* current = TRY(view_state(vm));
    for (size_t index = 1; index <= 4; ++index)
        if (!vm.argument(index).is_int32())
            return vm.throw_completion<JS::TypeError>(
                "View bounds require signed integer coordinates"_utf16);
    auto x = vm.argument(1).as_i32(), y = vm.argument(2).as_i32();
    auto width = vm.argument(3).as_i32(), height = vm.argument(4).as_i32();
    auto right = int64_t(x) + width, bottom = int64_t(y) + height;
    if (width < 0 || height < 0 || width > 16384 || height > 16384
        || right < std::numeric_limits<int>::min() || right > std::numeric_limits<int>::max()
        || bottom < std::numeric_limits<int>::min() || bottom > std::numeric_limits<int>::max())
        return vm.throw_completion<JS::RangeError>("View bounds exceed native coordinates"_utf16);
    current->view_bounds = { x, y, width, height };
    current->view->setGeometry(native_bounds(current->view_bounds));
    // Qt defers hidden-widget resize events; use upstream's explicit
    // viewport push so the real page receives geometry and device scale.
    current->view->push_viewport_size();
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_view_bounds(JS::VM& vm)
{
    auto bounds = TRY(view_state(vm))->view_bounds;
    auto bytes = QJsonDocument(QJsonArray { bounds[0], bounds[1], bounds[2], bounds[3] })
                     .toJson(QJsonDocument::Compact);
    return BridgeDetail::text(vm, StringView(bytes.data(), bytes.size()));
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_set_view_visible(JS::VM& vm)
{
    auto* current = TRY(view_state(vm));
    if (!vm.argument(1).is_boolean())
        return vm.throw_completion<JS::TypeError>("View visibility must be boolean"_utf16);
    current->desired_visible = vm.argument(1).as_bool();
    current->view->setVisible(current->desired_visible && current->owner_id.has_value());
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_view_visible(JS::VM& vm)
{
    return JS::Value(TRY(view_state(vm))->desired_visible);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_close_contents(JS::VM& vm)
{
    TRY(state(vm));
    close_contents(vm.argument(0).as_i32());
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_focus_contents(JS::VM& vm)
{
    TRY(state(vm))->view->setFocus(Qt::OtherFocusReason);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_owner_window(JS::VM& vm)
{
    if (!vm.argument(0).is_int32())
        return vm.throw_completion<JS::TypeError>("Invalid web contents ID"_utf16);
    auto id = vm.argument(0).as_i32();
    auto found = windows.find(id);
    if (found == windows.end() || found->second.closing)
        return JS::js_null();
    auto const& current = found->second;
    if (current.kind == State::Kind::WindowMain) {
        if (current.window && current.content_container
            && current.view->parentWidget() == current.content_container)
            return JS::Value(id);
        return JS::js_null();
    }

    if (!current.owner_id.has_value())
        return JS::js_null();
    auto owner = windows.find(*current.owner_id);
    if (owner == windows.end() || owner->second.closing
        || owner->second.kind != State::Kind::WindowMain || !owner->second.window
        || !owner->second.content_container
        || current.view->parentWidget() != owner->second.content_container)
        return JS::js_null();
    return JS::Value(owner->first);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_child_view_ids(JS::VM& vm)
{
    auto* host = TRY(window_state(vm));
    QJsonArray ids;
    // Qt updates QObject child order on QWidget raise/lower. Inspect the
    // actual composition tree instead of returning our ownership metadata.
    for (auto* object : host->content_container->children())
        for (auto const& [id, current] : windows)
            if (current.kind == State::Kind::ChildView && !current.closing && current.view == object
                && current.view->parentWidget() == host->content_container) {
                ids.append(id);
                break;
            }

    auto bytes = QJsonDocument(ids).toJson(QJsonDocument::Compact);
    return BridgeDetail::text(vm, StringView(bytes.data(), bytes.size()));
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_views_focused_contents(JS::VM&)
{
    auto* focused = QApplication::focusWidget();
    for (auto const& [id, current] : windows)
        if (!current.closing && focused
            && (focused == current.view || current.view->isAncestorOf(focused)))
            return JS::Value(id);
    return JS::js_null();
}

}
