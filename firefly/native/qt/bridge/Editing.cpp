/** Native plaintext editing targets the actual owned view and its focused document. */
#include "Impl.h"
#include <LibCore/Promise.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibWebCommon/Clipboard/SystemClipboard.h>
#include <LibWebView/Application.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <QApplication>
#include <QClipboard>
#include <QPointer>
#include <UI/Qt/WebContentView.h>
#include <cmath>

namespace Firefly {
namespace {
    bool active_lineage(
        WebView::CanonicalNavigable const& node, WebView::CanonicalTraversable const& root)
    {
        auto const* current = &node;
        for (size_t depth = 0; depth < 128; ++depth) {
            if (!current->active_document().host() || !current->active_document().host()->is_open())
                return false;
            if (current == &root)
                return true;
            auto const* parent = current->parent();
            if (!parent || !current->container_document()
                || current->container_document()->id() != parent->active_document().id())
                return false;
            current = parent;
        }

        return false;
    }
}

void Bridge::Impl::install_editing(JS::Object& host)
{
    host.define_native_function(realm, "editContents"_utf16,
        native_binding<&Impl::native_host_edit_contents>(), 2, JS::default_attributes);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_edit_contents(JS::VM& vm)
{
    auto value = vm.argument(0);
    if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 0
        || value.as_double() > 2147483647 || std::trunc(value.as_double()) != value.as_double()
        || !vm.argument(1).is_string())
        return preload_error(
            vm, "ERR_INVALID_ARG_TYPE", "Editing requires a contents ID and command"sv);
    auto command = vm.argument(1).as_string().utf16_string_view();
    bool copy = command == u"copy"sv, cut = command == u"cut"sv;
    bool paste = command == u"paste"sv || command == u"pasteAndMatchStyle"sv;
    if (!copy && !cut && !paste && command != u"undo"sv && command != u"redo"sv
        && command != u"selectAll"sv)
        return preload_error(
            vm, "ERR_FIREFLY_UNSUPPORTED_API", "This native editing command is not implemented"sv);
    if (stopping || !context)
        return preload_error(vm, "ERR_FIREFLY_APP_EXITED", "Application has exited"sv);
    int id = static_cast<int>(value.as_double());
    if (!id) {
        auto* focused = QApplication::focusWidget();
        for (auto const& [candidate, state] : windows)
            if (!state.closing && state.view && focused
                && (focused == state.view || state.view->isAncestorOf(focused))) {
                id = candidate;
                break;
            }
        // A menu can be invoked while no renderer owns the OS first responder.
        // Never redirect its edit command to an arbitrary application window.
        if (!id)
            return JS::js_undefined();
    }

    auto found = windows.find(id);
    if (found == windows.end() || found->second.closing || !found->second.view)
        return preload_error(
            vm, "ERR_FIREFLY_OBJECT_DESTROYED", "Editing contents are destroyed"sv);
    if ((copy || cut || paste) && QGuiApplication::platformName() != QStringLiteral("cocoa")
        && QGuiApplication::platformName() != QStringLiteral("xcb"))
        return preload_error(vm, "ERR_FIREFLY_UNSUPPORTED_API",
            "Native clipboard editing requires the Cocoa or X11 desktop backend"sv);
    auto* widget = found->second.view;
    auto canonical = HubFrontend::canonical_view(*widget);
    if (!canonical.has_value() || !canonical->has_traversable())
        return preload_error(
            vm, "ERR_FIREFLY_NOT_READY", "Editing contents have no live canonical document"sv);
    auto& root = canonical->traversable();
    auto focused_id = root.focused_navigable_id().value_or(root.id());
    auto focused = root.find(focused_id);
    if (!focused.has_value() || !active_lineage(*focused, root))
        return preload_error(
            vm, "ERR_FIREFLY_NOT_READY", "Focused editing document is unavailable"sv);
    if (command == u"undo"sv)
        canonical->undo();
    else if (command == u"redo"sv)
        canonical->redo();
    else if (command == u"selectAll"sv)
        canonical->select_all();
    else if (paste) {
        auto* clipboard = QGuiApplication::clipboard();
        if (!clipboard)
            return preload_error(
                vm, "ERR_FIREFLY_UNSUPPORTED_API", "System clipboard is unavailable"sv);
        auto top_document = root.active_document().id();
        auto document = focused->active_document().id();
        auto epoch = found->second.ipc_byte_epoch;
        QPointer<QObject> lifetime(context.get());
        QPointer<Ladybird::WebContentView> view(widget);
        auto text = clipboard->text(QClipboard::Clipboard).toUtf8();
        // An X11 selection owner can require native event processing to answer.
        // Revalidate the original target after the OS read, never paste into a
        // new window/document that acquired focus while that read was pending.
        if (!lifetime || !view || stopping)
            return JS::js_undefined();
        auto current = windows.find(id);
        if (current == windows.end() || current->second.closing || current->second.view != view
            || current->second.ipc_byte_epoch != epoch)
            return JS::js_undefined();
        canonical = HubFrontend::canonical_view(*view);
        if (!canonical.has_value() || !canonical->has_traversable()
            || canonical->traversable().active_document().id() != top_document)
            return JS::js_undefined();
        focused = canonical->traversable().find(focused_id);
        if (!focused.has_value() || !active_lineage(*focused, canonical->traversable())
            || focused->active_document().id() != document)
            return JS::js_undefined();
        if (text.size() > 16 * 1024 * 1024)
            return preload_error(
                vm, "ERR_FIREFLY_EDIT_LIMIT", "System clipboard text exceeds 16 MiB"sv);
        auto decoded = String::from_utf8(StringView(text.data(), text.size()));
        if (decoded.is_error())
            return preload_error(
                vm, "ERR_FIREFLY_CLIPBOARD_DATA", "Invalid system clipboard text"sv);
        // The embedded Hub's clipboard is a genuine engine MIME store, separate
        // from the OS provider. Seed it only for this explicit native user action.
        auto plaintext = decoded.release_value();
        WebView::Application::the().WebView::Application::set_clipboard_text(
            plaintext, WebView::Application::ClipboardType::Text);
        Web::Clipboard::SystemClipboardItem item { .system_clipboard_representations
            = { { .name = "text/plain"_string, .data = plaintext.to_byte_string() } } };
        Vector<Web::Clipboard::SystemClipboardItem> items;
        items.append(move(item));
        // Hub's virtual clipboard read takes another UI roundtrip. Deliver this
        // exact OS snapshot through the same native focused-frame paste path
        // instead of rereading mutable global clipboard state asynchronously.
        auto host = focused->active_document().host();
        host->give_user_activation();
        host->async_paste_from_clipboard(move(items));
    } else {
        if (!QGuiApplication::clipboard())
            return preload_error(
                vm, "ERR_FIREFLY_UNSUPPORTED_API", "System clipboard is unavailable"sv);
        if (!found->second.editing_budget)
            found->second.editing_budget = std::make_shared<size_t>(0);
        auto budget = found->second.editing_budget;
        if (*budget >= 32)
            return preload_error(
                vm, "ERR_FIREFLY_EDIT_LIMIT", "Too many pending native editing requests"sv);
        ++*budget;
        auto top_document = root.active_document().id();
        auto document = focused->active_document().id();
        auto child = focused->active_document().host()->client().child_id();
        auto view_id = widget->view_id();
        auto epoch = found->second.ipc_byte_epoch;
        QPointer<QObject> lifetime(context.get());
        QPointer<Ladybird::WebContentView> view(widget);
        auto request = cut ? canonical->cut_selected_text() : canonical->selected_text();
        request->when_resolved([this, id, lifetime, view, top_document, focused_id, document, child,
                                   view_id, epoch, budget](ByteString& selection) {
            --*budget;
            if (!lifetime || !view || stopping)
                return;
            auto current = windows.find(id);
            if (current == windows.end() || current->second.closing || current->second.view != view
                || current->second.ipc_byte_epoch != epoch || view->view_id() != view_id)
                return;
            auto canonical = HubFrontend::canonical_view(*view);
            if (!canonical.has_value() || !canonical->has_traversable()
                || canonical->traversable().active_document().id() != top_document)
                return;
            auto focused = canonical->traversable().find(focused_id);
            if (!focused.has_value() || !active_lineage(*focused, canonical->traversable())
                || focused->active_document().id() != document || !focused->active_document().host()
                || !focused->active_document().host()->is_open()
                || focused->active_document().host()->client().child_id() != child)
                return;
            if (selection.length() > 16 * 1024 * 1024) {
                warnln(
                    "ERR_FIREFLY_EDIT_LIMIT: native selection exceeds the 16 MiB plaintext budget");
                return;
            }
            // Empty native selection is a real no-op, not clipboard clearing.
            if (!selection.is_empty())
                if (auto* clipboard = QGuiApplication::clipboard())
                    clipboard->setText(
                        QString::fromUtf8(selection.characters(), selection.length()),
                        QClipboard::Clipboard);
        });
        request->when_rejected([budget](Error&) { --*budget; });
    }

    return JS::js_undefined();
}
}
