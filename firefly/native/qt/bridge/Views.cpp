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
namespace {
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

Optional<int> Bridge::Impl::create_view(Optional<Preload> preload)
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
    view->hide();
    view->setGeometry(0, 0, 0, 0);
    windows.emplace(id, State { nullptr, view, move(preload), State::Kind::ChildView });
    setup_content_callbacks(id);
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
    child.view->setParent(nullptr);
    child.view->setGeometry(native_bounds(child.view_bounds));
    child.view->push_viewport_size();
}

void Bridge::Impl::attach_view(int host_id, int child_id, size_t index)
{
    auto& host = windows.at(host_id);
    auto& child = windows.at(child_id);
    if (child.owner_id.has_value())
        detach_view(*child.owner_id, child_id);
    child.owner_id = host_id;
    child.view->setParent(host.content_container);
    child.view->setGeometry(native_bounds(child.view_bounds));
    child.view->push_viewport_size();
    auto& ids = host.child_ids;
    ids.insert(ids.begin() + std::min(index, ids.size()), child_id);
    child.view->setVisible(child.desired_visible);
    restack_views(host_id);
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
}

void Bridge::Impl::install_views(JS::Object& host)
{
    host.define_native_function(
        realm, "createView"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto native_enabled = WebView::Application::browser_options().disable_sandbox
                == WebView::DisableSandbox::No;
            if (auto failure = sandbox.check_window(native_enabled); failure.has_value())
                return SandboxPolicy::fail(vm, *failure);
            auto id = create_view(TRY(read_preload(vm, 0, 1)));
            if (!id.has_value())
                return vm.throw_completion<JS::RangeError>("Native view budget exceeded"_utf16);
            return JS::Value(*id);
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "attachView"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
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
            attach_view(vm.argument(0).as_i32(), vm.argument(1).as_i32(), index);
            return JS::js_undefined();
        },
        3, JS::default_attributes);
    host.define_native_function(
        realm, "detachView"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            TRY(window_state(vm));
            TRY(view_state(vm, 1));
            detach_view(vm.argument(0).as_i32(), vm.argument(1).as_i32());
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "setViewBounds"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* current = TRY(view_state(vm));
            for (size_t index = 1; index <= 4; ++index)
                if (!vm.argument(index).is_int32())
                    return vm.throw_completion<JS::TypeError>(
                        "View bounds require signed integer coordinates"_utf16);
            auto x = vm.argument(1).as_i32(), y = vm.argument(2).as_i32();
            auto width = vm.argument(3).as_i32(), height = vm.argument(4).as_i32();
            auto right = int64_t(x) + width, bottom = int64_t(y) + height;
            if (width < 0 || height < 0 || width > 16384 || height > 16384
                || right < std::numeric_limits<int>::min()
                || right > std::numeric_limits<int>::max()
                || bottom < std::numeric_limits<int>::min()
                || bottom > std::numeric_limits<int>::max())
                return vm.throw_completion<JS::RangeError>(
                    "View bounds exceed native coordinates"_utf16);
            current->view_bounds = { x, y, width, height };
            current->view->setGeometry(native_bounds(current->view_bounds));
            // Qt defers hidden-widget resize events; use upstream's explicit
            // viewport push so the real page receives geometry and device scale.
            current->view->push_viewport_size();
            return JS::js_undefined();
        },
        5, JS::default_attributes);
    host.define_native_function(
        realm, "viewBounds"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto bounds = TRY(view_state(vm))->view_bounds;
            auto bytes = QJsonDocument(QJsonArray { bounds[0], bounds[1], bounds[2], bounds[3] })
                             .toJson(QJsonDocument::Compact);
            return BridgeDetail::text(vm, StringView(bytes.data(), bytes.size()));
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "setViewVisible"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* current = TRY(view_state(vm));
            if (!vm.argument(1).is_boolean())
                return vm.throw_completion<JS::TypeError>("View visibility must be boolean"_utf16);
            current->desired_visible = vm.argument(1).as_bool();
            current->view->setVisible(current->desired_visible && current->owner_id.has_value());
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "viewVisible"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(TRY(view_state(vm))->desired_visible);
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "closeContents"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            TRY(state(vm));
            close_contents(vm.argument(0).as_i32());
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "focusContents"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            TRY(state(vm))->view->setFocus(Qt::OtherFocusReason);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "ownerWindow"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
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
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "childViewIds"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto* host = TRY(window_state(vm));
            QJsonArray ids;
            // Qt updates QObject child order on QWidget raise/lower. Inspect the
            // actual composition tree instead of returning our ownership metadata.
            for (auto* object : host->content_container->children())
                for (auto const& [id, current] : windows)
                    if (current.kind == State::Kind::ChildView && !current.closing
                        && current.view == object
                        && current.view->parentWidget() == host->content_container) {
                        ids.append(id);
                        break;
                    }

            auto bytes = QJsonDocument(ids).toJson(QJsonDocument::Compact);
            return BridgeDetail::text(vm, StringView(bytes.data(), bytes.size()));
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "focusedContents"_utf16,
        [this](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            auto* focused = QApplication::focusWidget();
            for (auto const& [id, current] : windows)
                if (!current.closing && focused
                    && (focused == current.view || current.view->isAncestorOf(focused)))
                    return JS::Value(id);
            return JS::js_null();
        },
        0, JS::default_attributes);
}
}
