/** Scope native Qt display notifications to the bridge owner and its GUI thread. */
#include "ScreenHost.h"
#include <QGuiApplication>
#include <QScreen>

namespace Firefly {
void watch_screen_changes(QObject& context, std::function<void()> changed)
{
    auto connect = [&context, changed](QScreen* screen) {
        QObject::connect(screen, &QScreen::geometryChanged, &context, [changed] { changed(); });
        QObject::connect(
            screen, &QScreen::availableGeometryChanged, &context, [changed] { changed(); });
        QObject::connect(
            screen, &QScreen::logicalDotsPerInchChanged, &context, [changed] { changed(); });
        QObject::connect(screen, &QScreen::orientationChanged, &context, [changed] { changed(); });
    };

    for (auto* screen : QGuiApplication::screens())
        connect(screen);
    QObject::connect(
        qGuiApp, &QGuiApplication::screenAdded, &context, [connect, changed](QScreen* screen) {
            connect(screen);
            changed();
        });
    QObject::connect(qGuiApp, &QGuiApplication::screenRemoved, &context, [changed] { changed(); });
    QObject::connect(
        qGuiApp, &QGuiApplication::primaryScreenChanged, &context, [changed] { changed(); });
}
}
