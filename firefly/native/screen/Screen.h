/** Native display data in primary-top-left device-independent coordinates. */
#pragma once
#include <QJsonObject>

namespace Firefly {
QJsonObject screen_snapshot();

QJsonObject cursor_screen_point();
}
