/** Run only against a caller-provided isolated X server; native peers keep their owner loop alive.
 */
#include "Clipboard.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeData>
#include <QTimer>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {
void require(bool condition, char const* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL X11 clipboard: %s\n", message);
        std::exit(1);
    }
}

QByteArray text()
{
    QByteArray result("Grüße 🪲\nline");
    result.append('\0');
    result.append("tail");
    return result;
}

QJsonArray representations()
{
    QJsonArray result;
    for (auto const& pair :
        { std::pair { "text/plain", text() }, std::pair { "text/html", QByteArray("<b>hello</b>") },
            std::pair { "text/rtf", QByteArray("{\\rtf1 hello}") } })
        result.append(QJsonObject {
            { "format", pair.first }, { "data", QString::fromLatin1(pair.second.toBase64()) } });
    return result;
}

void write_owned()
{
    auto result = Firefly::clipboard_request(
        { { "op", "write" }, { "items", QJsonArray { representations() } } });
    require(!result.contains("error"), "owned publication failed");
}

void read_owned(bool unknown)
{
    using Firefly::clipboard_request;
    require(clipboard_request({ { "op", "readText" } })["text"].toString().toUtf8() == text(),
        "external text bytes changed");
    require(clipboard_request({ { "op", "has" }, { "format", "text/html" } })["has"].toBool(),
        "external HTML metadata absent");
    auto read = clipboard_request({ { "op", "read" } });
    if (unknown) {
        require(read["error"].toObject()["code"] == "ERR_FIREFLY_UNSUPPORTED_API",
            "unknown external format was silently dropped");
        return;
    }

    auto items = read["items"].toArray();
    require(!read.contains("error") && items.size() == 1, "external item snapshot failed");
    auto values = items[0].toArray();
    require(values.size() == 3, "external representation count changed");
    for (auto const& expected : representations())
        require(values.contains(expected), "external representation bytes changed");
}
}

int main(int argc, char** argv)
{
    // Check before constructing QGuiApplication, so accidental local execution
    // cannot activate an app or read/replace the user's desktop clipboard.
    if (qEnvironmentVariable("FIREFLY_TEST_ISOLATED_X11") != "1") {
        fputs("Refusing clipboard probe without an explicitly isolated X11 server\n", stderr);
        return 77;
    }

    QGuiApplication app(argc, argv);
    if (app.platformName() != "xcb" || app.arguments().size() != 2)
        return 1;
    auto mode = app.arguments()[1];
    QTimer::singleShot(0, &app, [&] {
        auto* board = app.clipboard();
        if (mode == "serve-owned") {
            write_owned();
        } else if (mode == "serve-external" || mode == "serve-unknown") {
            auto* data = new QMimeData;
            for (auto const& entry : representations()) {
                auto value = entry.toObject();
                data->setData(value["format"].toString(),
                    QByteArray::fromBase64(value["data"].toString().toLatin1()));
            }

            if (mode == "serve-unknown")
                data->setData("application/x-elytra-unknown", "external");
            board->setMimeData(data);
            require(board->ownsClipboard(), "independent writer cannot acquire ownership");
        } else if (mode == "read-native") {
            // Independent Qt reader does not use the Firefly wire/backend.
            auto* data = board->mimeData();
            require(data != nullptr, "independent reader has no provider");
            for (auto const& entry : representations()) {
                auto value = entry.toObject();
                require(data->data(value["format"].toString())
                        == QByteArray::fromBase64(value["data"].toString().toLatin1()),
                    "independent reader byte mismatch");
            }
        } else if (mode == "read-owned" || mode == "read-unknown") {
            read_owned(mode == "read-unknown");
        } else if (mode == "local") {
            write_owned();
            read_owned(false);
            auto invalid = Firefly::clipboard_request({ { "op", "write" },
                { "items",
                    QJsonArray { QJsonArray {
                        QJsonObject { { "format", "image/png" }, { "data", "eA==" } } } } } });
            require(invalid.contains("error"), "unsupported write accepted");
            read_owned(false);
            Firefly::clipboard_request({ { "op", "clear" } });
            require(Firefly::clipboard_request({ { "op", "readText" } })["text"] == "",
                "clear left text");
            require(Firefly::clipboard_request({ { "op", "read" } })["items"].toArray().isEmpty(),
                "clear left items");
        } else {
            require(false, "unknown clipboard probe mode");
        }

        if (mode.startsWith("serve-")) {
            puts("READY");
            fflush(stdout);
        } else {
            puts("PASS native X11 clipboard operation");
            app.exit(0);
        }
    });
    QTimer::singleShot(30000, &app, [&] { app.exit(1); });
    return app.exec();
}
