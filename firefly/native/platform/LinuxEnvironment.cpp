/** Refuse unsupported Linux startup environments before GUI or VM initialization. */
#include "DesktopServices.h"
#include <QString>
#include <QtGlobal>

namespace Firefly {
ErrorOr<void> verify_desktop_environment()
{
    if (qEnvironmentVariable("QT_QPA_PLATFORM") != "xcb" || qEnvironmentVariableIsEmpty("DISPLAY"))
        return Error::from_string_literal(
            "Experimental Linux Firefly requires QT_QPA_PLATFORM=xcb and a native X11 DISPLAY");
    return {};
}

}
