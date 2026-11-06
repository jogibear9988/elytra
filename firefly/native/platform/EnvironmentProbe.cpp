/** Check the actual Linux startup refusal without constructing any GUI application. */
#include "DesktopServices.h"
#include <QByteArray>
#include <QtGlobal>
#include <cstdio>

int main()
{
    // Deliberately set invalid combinations, never connect to the user's display.
    qputenv("QT_QPA_PLATFORM", "wayland");
    qputenv("DISPLAY", ":unreachable");
    if (!Firefly::verify_desktop_environment().is_error())
        return 1;
    qputenv("QT_QPA_PLATFORM", "xcb");
    qunsetenv("DISPLAY");
    if (!Firefly::verify_desktop_environment().is_error())
        return 2;
    puts("PASS native Linux runtime refuses non-X11 and absent display before GUI initialization");
    return 0;
}
