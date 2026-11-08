/** Shared profile preference lifecycle; system-theme discovery remains platform-owned. */
#pragma once
#include <AK/Optional.h>
#include <LibWebView/Settings.h>
#include <functional>

namespace Firefly {
class ThemeSettings {
public:
    // Resolve settings lazily: main-process theme selection precedes profile creation.
    explicit ThemeSettings(std::function<WebView::Settings&()>);

    void defer();

    void activate();

    void set_source(WebView::ThemePreference);

private:
    void apply(WebView::ThemePreference);

    std::function<WebView::Settings&()> m_settings;
    bool m_ready { true };
    Optional<WebView::ThemePreference> m_pending_source;
};

void defer_theme_settings();

void activate_theme_settings();

void set_theme_settings_source(WebView::ThemePreference);
}
