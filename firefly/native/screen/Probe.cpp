/** Read-only native acceptance: no QApplication, windows, cursor changes or renderer processes. */
#include "Screen.h"
#include <CoreGraphics/CoreGraphics.h>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <cmath>
#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    auto snapshot = Firefly::screen_snapshot();
    auto displays = snapshot["displays"].toArray();
    if (displays.isEmpty()) {
        fputs("Native display service unavailable\n", stderr);
        return 1;
    }

    if (snapshot["primaryId"].toDouble() != CGMainDisplayID())
        return 2;
    for (auto const& value : displays) {
        auto display = value.toObject();
        auto id = static_cast<CGDirectDisplayID>(display["id"].toDouble());
        if (!CGDisplayIsActive(id) || !CGDisplayPixelsWide(id) || !CGDisplayPixelsHigh(id))
            return 3;
        auto bounds = display["bounds"].toObject();
        auto native = CGDisplayBounds(id);
        if (std::abs(bounds["x"].toDouble() - native.origin.x) > 1
            || std::abs(bounds["y"].toDouble() - native.origin.y) > 1
            || std::abs(bounds["width"].toDouble() - native.size.width) > 1
            || std::abs(bounds["height"].toDouble() - native.size.height) > 1)
            return 4;
        auto work = display["workArea"].toObject();
        if (work["width"].toDouble() < 0 || work["height"].toDouble() < 0
            || work["x"].toDouble() < bounds["x"].toDouble()
            || work["y"].toDouble() < bounds["y"].toDouble()
            || work["x"].toDouble() + work["width"].toDouble()
                > bounds["x"].toDouble() + bounds["width"].toDouble()
            || work["y"].toDouble() + work["height"].toDouble()
                > bounds["y"].toDouble() + bounds["height"].toDouble())
            return 5;
        if (display["scaleFactor"].toDouble() <= 0 || display["label"].toString().isEmpty())
            return 6;
    }

    auto bytes = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
    fwrite(bytes.data(), 1, bytes.size(), stdout);
    fputc('\n', stdout);
    return 0;
}
