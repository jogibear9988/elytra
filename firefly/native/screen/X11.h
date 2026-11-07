/** Read X11 display/cursor snapshots on the GUI thread without retaining JS/renderer objects.
 * Failures are explicit {error:{code,message}} records; no synthetic display IDs are allocated.
 */
#pragma once
#include <QJsonObject>

namespace Firefly {
QJsonObject x11_screen_snapshot();

QJsonObject x11_cursor_screen_point();
}
