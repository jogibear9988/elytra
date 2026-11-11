/** Qt widgets never stand in for canonical document authority; resolve the genuine in-process Hub
 * view. */
#include "Frontend.h"
#include <AK/Debug.h>
#include <LibWebCommon/Clipboard/SystemClipboard.h>
#include <LibWebView/ViewImplementation.h>
#include <LibWebView/WebContentPage.h>
#include <QApplication>
#include <QThread>
#include <UI/Qt/RemoteView.h>

namespace Firefly::HubFrontend {
namespace {
    bool ready;
}

void initialize()
{
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
    VERIFY(!ready && WebView::Application::is_initialized());
    ready = true;
}

void shutdown()
{
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
    ready = false;
}

Optional<WebView::ViewImplementation&> canonical_view(Ladybird::RemoteView const& remote)
{
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
    if (!ready || Ladybird::RemoteView::find(remote.view_id()) != &remote)
        return {};
    return WebView::ViewImplementation::find_view_by_id(remote.view_id());
}

bool owns_page(Ladybird::RemoteView const& remote, WebView::WebContentPage const& page)
{
    auto native = canonical_view(remote);
    return native.has_value() && page.is_open() && &page.view() == &*native;
}

WebView::Settings& settings()
{
    return WebView::Application::settings();
}

void unsupported(StringView operation)
{
    warnln("ERR_FIREFLY_UNSUPPORTED_BROWSER_UI: {}", operation);
}

WebView::Translator* translator()
{
    return nullptr;
}

void resolve_external_url_handler(URL::URL const&, WebView::ExternalURLHandlerCallback callback)
{
    // External application launch belongs to Firefly shell.openExternal, not an
    // implicit browser handler lookup/permission prompt in an embedded page.
    unsupported("implicit external URL handler"sv);
    callback(nullptr);
}

void display_error_dialog(StringView message)
{
    // Diagnostics must not create an unowned browser QMessageBox. The explicit
    // application dialog API keeps its separate owned lifecycle and admission.
    warnln("ERR_FIREFLY_UNSUPPORTED_BROWSER_UI: browser dialog: {}", message);
}

bool supports_private_browsing_windows()
{
    return false;
}

void set_clipboard_text(String text, WebView::Application::ClipboardType type)
{
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
    // Keep the pre-migration embedded engine clipboard semantics. Firefly's
    // explicit desktop clipboard API remains its independently owned provider.
    WebView::Application::the().WebView::Application::set_clipboard_text(move(text), type);
}

Utf16String clipboard_text(WebView::Application::ClipboardType type)
{
    VERIFY(qApp && QThread::currentThread() == qApp->thread());
    return WebView::Application::the().WebView::Application::clipboard_text(type);
}

Web::Clipboard::SystemClipboardItem clipboard_item()
{
    // Preserve the actual embedded engine MIME store without inventing OS
    // rich-clipboard integration or creating a browser frontend.
    return WebView::Application::the().WebView::Application::clipboard_item();
}

void insert_clipboard_item(Web::Clipboard::SystemClipboardItem item)
{
    WebView::Application::the().WebView::Application::insert_clipboard_item(move(item));
}
}
