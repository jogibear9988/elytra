/** Exercise production lifecycle against real isolated WebView settings, without Qt/GUI startup. */
#include "ThemeSettings.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace {
struct OwnedDirectory {
    std::filesystem::path path;

    ~OwnedDirectory()
    {
        if (!path.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(path, ignored);
        }
    }
};
}

int main()
{
    std::error_code error;
    auto temporary = std::filesystem::temp_directory_path(error);
    if (error)
        return 1;
    OwnedDirectory directory;
    auto serial = std::chrono::steady_clock::now().time_since_epoch().count();
    for (int attempt = 0; attempt < 100; ++attempt) {
        auto candidate = temporary
            / ("firefly-theme-settings-" + std::to_string(serial) + "-" + std::to_string(attempt));
        if (std::filesystem::create_directory(candidate, error)) {
            directory.path = std::move(candidate);
            break;
        }

        if (error && error != std::errc::file_exists)
            return 1;
        error.clear();
    }

    if (directory.path.empty())
        return 1;
    auto filename = directory.path / "settings.json";
    {
        std::ofstream stream(filename);
        stream << "{}";
        if (!stream)
            return 1;
    }

    auto encoded = filename.string();
    auto settings = WebView::Settings::create(ByteString(encoded.c_str()));
    auto appearance = settings.appearance();
    appearance.interface_theme = WebView::ThemePreference::Light;
    appearance.website_color_scheme = WebView::ThemePreference::Dark;
    appearance.show_menu_bar = true;
    settings.set_appearance(appearance);
    unsigned resolutions = 0;
    Firefly::ThemeSettings lifecycle([&]() -> WebView::Settings& {
        ++resolutions;
        return settings;
    });
    lifecycle.defer();
    lifecycle.set_source(WebView::ThemePreference::Light);
    lifecycle.set_source(WebView::ThemePreference::Dark);
    if (resolutions || settings.appearance().interface_theme != WebView::ThemePreference::Light)
        return 1; // No profile access or mutation before native initialization.
    lifecycle.activate();
    if (resolutions != 1 || settings.appearance().interface_theme != WebView::ThemePreference::Dark
        || settings.appearance().website_color_scheme != WebView::ThemePreference::Dark
        || !settings.appearance().show_menu_bar)
        return 1;
    lifecycle.set_source(WebView::ThemePreference::Light);
    if (resolutions != 2 || settings.appearance().interface_theme != WebView::ThemePreference::Light
        || settings.appearance().website_color_scheme != WebView::ThemePreference::Light)
        return 1;
    // A new application launch must reset an earlier explicit/persisted preference.
    lifecycle.defer();
    if (resolutions != 2)
        return 1;
    lifecycle.activate();
    if (resolutions != 3
        || settings.appearance().interface_theme != WebView::ThemePreference::System
        || settings.appearance().website_color_scheme != WebView::ThemePreference::System)
        return 1;
    auto reloaded = WebView::Settings::create(ByteString(encoded.c_str()));
    if (reloaded.appearance().interface_theme != WebView::ThemePreference::System
        || reloaded.appearance().website_color_scheme != WebView::ThemePreference::System
        || !reloaded.appearance().show_menu_bar)
        return 1;
    std::puts(
        "PASS real profile theme lifecycle: deferred access, last pre-ready selection, live "
        "updates, launch reset and persisted independent fields; no GUI/system preference changes");
    return 0;
}
