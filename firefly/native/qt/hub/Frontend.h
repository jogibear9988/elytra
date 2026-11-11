/** Embedded frontend services: native authority and explicitly bounded platform operations, not
 * browser UI. */
#pragma once
#include <AK/Function.h>
#include <AK/Optional.h>
#include <LibWebView/Application.h>
#include <LibWebView/ExternalURLHandler.h>
#include <LibWebView/Translator.h>

namespace Ladybird {
class RemoteView;
}

namespace Firefly::HubFrontend {
void initialize();

void shutdown();

Optional<WebView::ViewImplementation&> canonical_view(Ladybird::RemoteView const&);

bool owns_page(Ladybird::RemoteView const&, WebView::WebContentPage const&);

WebView::Settings& settings();

void unsupported(StringView operation);

WebView::Translator* translator();

void resolve_external_url_handler(URL::URL const&, WebView::ExternalURLHandlerCallback);

void display_error_dialog(StringView);

void set_clipboard_text(
    String, WebView::Application::ClipboardType = WebView::Application::ClipboardType::Text);

Utf16String clipboard_text(WebView::Application::ClipboardType);

Web::Clipboard::SystemClipboardItem clipboard_item();

void insert_clipboard_item(Web::Clipboard::SystemClipboardItem);

bool supports_private_browsing_windows();
}
