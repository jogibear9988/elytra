/** Guard before GUI initialization; drive real geometry while an independent X11 peer observes. */
#include "Geometry.h"
#include <QApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <cstdio>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {
QJsonArray rectangle(QRect rect)
{
    return { rect.x(), rect.y(), rect.width(), rect.height() };
}

void reply(QJsonObject value)
{
    auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
    printf("%s\n", bytes.constData());
    fflush(stdout);
}
}

int main(int argc, char** argv)
{
    // This check runs before QApplication can connect to or activate the user's desktop.
    if (qEnvironmentVariable("FIREFLY_TEST_ISOLATED_X11") != "1"
        || qEnvironmentVariable("QT_QPA_PLATFORM") != "xcb"
        || qEnvironmentVariableIsEmpty("DISPLAY")) {
        fputs("Refusing geometry probe without an explicitly isolated X11 server\n", stderr);
        return 77;
    }

    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    if (app.platformName() != "xcb" || app.arguments().size() != 2)
        return 1;
    auto mode = app.arguments()[1];
    if (mode != "framed" && mode != "frameless")
        return 2;
    QWidget widget(
        nullptr, mode == "frameless" ? Qt::Window | Qt::FramelessWindowHint : Qt::Window);
    widget.setWindowTitle("Firefly isolated geometry acceptance");
    widget.resize(440, 300);
    // Match the runtime's native-window creation before showing the widget.
    // The observer must receive the actual QWindow handle after the loop starts,
    // rather than an eagerly sampled QWidget ID during native initialization.
    widget.winId();
    widget.show();
    QTimer::singleShot(0, &widget, [&] {
        auto* window = widget.windowHandle();
        reply({ { "window", QString::number(window->winId()) },
            { "widgetWindow", QString::number(widget.winId()) },
            { "widgetVisible", widget.isVisible() }, { "nativeVisible", window->isVisible() },
            { "nativeExposed", window->isExposed() },
            { "offscreen", widget.testAttribute(Qt::WA_DontShowOnScreen) } });
    });
    QSocketNotifier input(STDIN_FILENO, QSocketNotifier::Read, &app);
    QObject::connect(&input, &QSocketNotifier::activated, &app, [&] {
        std::string line;
        if (!std::getline(std::cin, line)) {
            app.quit();
            return;
        }

        auto request = QJsonDocument::fromJson(QByteArray::fromStdString(line)).object();
        auto op = request["op"].toString();
        bool ok = false;
        if (op == "get") {
            reply({ { "outer", rectangle(Firefly::window_bounds(widget, false)) },
                { "content", rectangle(Firefly::window_bounds(widget, true)) } });
            return;
        } else if (op == "set") {
            auto rect = request["bounds"].toArray();
            if (rect.size() == 4)
                ok = Firefly::set_window_bounds(widget,
                    QRect(rect[0].toInt(), rect[1].toInt(), rect[2].toInt(), rect[3].toInt()),
                    request["content"].toBool(), request["position"].toBool(true));
        } else if (op == "center") {
            ok = Firefly::center_window(widget);
        }

        reply({ { "ok", ok } });
    });
    return app.exec();
}
