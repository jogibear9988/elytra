/** Apply the same genuine profile settings on every desktop platform, without platform queries. */
#include "ThemeSettings.h"
#include <LibWebView/Application.h>

namespace Firefly {
ThemeSettings::ThemeSettings(std::function<WebView::Settings&()> settings)
    : m_settings(std::move(settings))
{
}

void ThemeSettings::apply(WebView::ThemePreference preference)
{
    auto& settings = m_settings();
    auto appearance = settings.appearance();
    appearance.interface_theme = preference;
    appearance.website_color_scheme = preference;
    settings.set_appearance(appearance);
}

void ThemeSettings::defer()
{
    m_ready = false;
    // Each main-process launch starts at system, independent of persisted overrides.
    m_pending_source = WebView::ThemePreference::System;
}

void ThemeSettings::activate()
{
    m_ready = true;
    if (m_pending_source.has_value())
        apply(*m_pending_source);
}

void ThemeSettings::set_source(WebView::ThemePreference preference)
{
    m_pending_source = preference;
    if (m_ready)
        apply(preference);
}

namespace {
    ThemeSettings& profile_theme()
    {
        static ThemeSettings settings(
            []() -> WebView::Settings& { return WebView::Application::settings(); });
        return settings;
    }
}

void defer_theme_settings()
{
    profile_theme().defer();
}

void activate_theme_settings()
{
    profile_theme().activate();
}

void set_theme_settings_source(WebView::ThemePreference preference)
{
    profile_theme().set_source(preference);
}
}
