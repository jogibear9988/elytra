/** Route the public display provider to real native X11 snapshots, including explicit errors. */
#include "Screen.h"
#include "X11.h"

namespace Firefly {
QJsonObject screen_snapshot()
{
    return x11_screen_snapshot();
}

QJsonObject cursor_screen_point()
{
    return x11_cursor_screen_point();
}
}
