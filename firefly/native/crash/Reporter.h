/** Opt-in process-lifetime local diagnostics; no LibJS or upload implementation lives here. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Optional.h>
#include <AK/StringBuilder.h>
#include <LibWebView/Export.h>
#include <LibWebView/ProcessType.h>

namespace Firefly::CrashReporting {
// Set by the trusted embedder before upstream profile initialization.
// Browser report migration and automatic disposal must not own application reports.
WEBVIEW_API void begin_embedded_profile();

WEBVIEW_API bool owns_embedded_profile();

WEBVIEW_API ByteString directory();

WEBVIEW_API ErrorOr<void> set_directory(StringView);

WEBVIEW_API ErrorOr<void> start(StringView bounded_configuration);

WEBVIEW_API ByteString parameters();

// Called by the guarded upstream compositions, never from a fatal signal handler.
WEBVIEW_API Optional<ByteString> directory_override();

WEBVIEW_API void append_description(StringBuilder&, WebView::ProcessType);

WEBVIEW_API void append_saved_description(StringBuilder&, WebView::ProcessType);
}
