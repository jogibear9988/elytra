/** Read-only native X11 display probe: never create windows or move the cursor. */
#include "X11.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QTimer>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--no-gui") == 0) {
        QCoreApplication application(argc, argv);
        auto screens = Firefly::x11_screen_snapshot();
        auto cursor = Firefly::x11_cursor_screen_point();
        if (screens["error"].toObject()["code"] != "ERR_FIREFLY_DISPLAY_UNAVAILABLE"
            || cursor["error"].toObject()["code"] != "ERR_FIREFLY_DISPLAY_UNAVAILABLE")
            return 1;
        return 0;
    }

    QGuiApplication application(argc, argv);
    QTimer::singleShot(0, [&] {
        auto screens = Firefly::x11_screen_snapshot();
        auto cursor = Firefly::x11_cursor_screen_point();
        auto bytes = QJsonDocument(QJsonObject { { "screens", screens }, { "cursor", cursor } })
                         .toJson(QJsonDocument::Compact);
        fwrite(bytes.data(), 1, bytes.size(), stdout);
        fputc('\n', stdout);
        application.exit(screens.contains("error") || cursor.contains("error") ? 1 : 0);
    });
    return application.exec();
}
