/** Native system preferences, independent of application-local theme overrides. */
#pragma once
#include <QJsonObject>
#include <functional>

class QObject;

namespace Firefly {
QJsonObject system_theme_snapshot();

void watch_accessibility_theme(QObject&, std::function<void()>);
}
