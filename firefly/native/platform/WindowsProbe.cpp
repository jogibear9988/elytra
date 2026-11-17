/** Read genuine Windows/Qt provider data without creating a visible window or changing OS state. */
#include "../locale/Locale.h"
#include "../screen/Screen.h"
#include "../screen/ScreenWindows.h"
#include "DesktopServices.h"
#include <QGuiApplication>
#include <QJsonArray>
#include <QScreen>
#include <QtCore/qt_windows.h>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    // Exercise the production conversion without rotating any native display.
    if (Firefly::windows_display_rotation(DMDO_DEFAULT) != 0
        || Firefly::windows_display_rotation(DMDO_90) != 270
        || Firefly::windows_display_rotation(DMDO_180) != 180
        || Firefly::windows_display_rotation(DMDO_270) != 90
        || Firefly::windows_display_rotation(0xffffffffUL) != -1)
        return 10;
    if (argc == 2 && std::strcmp(argv[1], "--rotation-only") == 0) {
        std::puts("PASS native Windows clockwise rotation conversion and unknown-value refusal");
        return 0;
    }

    if (Firefly::verify_desktop_environment().is_error())
        return 2;
    QGuiApplication application(argc, argv);
    if (QGuiApplication::platformName() != QStringLiteral("windows"))
        return 3;
    wchar_t native_locale[LOCALE_NAME_MAX_LENGTH] {};
    auto length = GetUserDefaultLocaleName(native_locale, LOCALE_NAME_MAX_LENGTH);
    if (length < 2
        || Firefly::Locale::system_locale() != QString::fromWCharArray(native_locale, length - 1))
        return 4;
    if (Firefly::Locale::preferred_languages().isEmpty())
        return 5;
    auto snapshot = Firefly::screen_snapshot();
    if (snapshot.contains("error"))
        return 6;
    auto displays = snapshot["displays"].toArray();
    if (displays.isEmpty() || displays.size() != QGuiApplication::screens().size())
        return 7;
    bool primary_found = false;
    for (auto value : displays) {
        auto display = value.toObject();
        if (display["id"] == snapshot["primaryId"])
            primary_found = true;
        auto bounds = display["bounds"].toObject();
        if (bounds["width"].toInt() <= 0 || bounds["height"].toInt() <= 0
            || display["scaleFactor"].toDouble() <= 0)
            return 8;
    }

    if (!primary_found || Firefly::cursor_screen_point().contains("error"))
        return 9;
    std::puts("PASS native Windows locale, monitor identities/DIP geometry and cursor provider");
    return 0;
}
