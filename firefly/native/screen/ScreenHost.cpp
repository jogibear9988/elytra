/** Keep OS snapshots separate from VM objects and queue through the existing bridge owner. */
#include "ScreenHost.h"
#include "Screen.h"
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QScreen>

namespace Firefly {
void install_screen_host(JS::Realm& realm, JS::Object& host)
{
    for (auto const& [name, read] : { std::pair { "screenSnapshot"_utf16, &screen_snapshot },
             std::pair { "cursorScreenPoint"_utf16, &cursor_screen_point } }) {
        host.define_native_function(
            realm, name,
            [read](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto bytes = QJsonDocument(read()).toJson(QJsonDocument::Compact);
                return JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
            },
            0, JS::default_attributes);
    }
}

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
