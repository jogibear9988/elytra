/** Read AppKit/CoreGraphics display metadata without creating windows or moving the cursor. */
#include "Screen.h"
#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>
#include <QJsonArray>
#include <cmath>

namespace Firefly {
namespace {
    QJsonObject rectangle(NSRect rect, CGFloat primary_height)
    {
        return { { "x", std::llround(NSMinX(rect)) },
            { "y", std::llround(primary_height - NSMaxY(rect)) },
            { "width", std::llround(NSWidth(rect)) }, { "height", std::llround(NSHeight(rect)) } };
    }
}

QJsonObject screen_snapshot()
{
    @autoreleasepool {
        auto* screens = [NSScreen screens];
        QJsonArray displays;
        double primary_id = -1;
        auto primary_height = screens.count ? NSHeight(screens[0].frame) : 0;
        for (NSScreen* screen in screens) {
            auto id = [screen.deviceDescription[@"NSScreenNumber"] unsignedIntValue];
            if (!displays.size())
                primary_id = id;
            auto bounds = rectangle(screen.frame, primary_height);
            auto work = rectangle(screen.visibleFrame, primary_height);
            double frequency = 0;
            if (auto mode = CGDisplayCopyDisplayMode(id)) {
                frequency = CGDisplayModeGetRefreshRate(mode);
                CGDisplayModeRelease(mode);
            }

            displays.append(QJsonObject { { "id", static_cast<double>(id) }, { "bounds", bounds },
                { "workArea", work },
                { "size",
                    QJsonObject { { "width", bounds["width"] }, { "height", bounds["height"] } } },
                { "workAreaSize",
                    QJsonObject { { "width", work["width"] }, { "height", work["height"] } } },
                { "scaleFactor", screen.backingScaleFactor },
                { "rotation", static_cast<int>(std::llround(CGDisplayRotation(id))) },
                { "internal", CGDisplayIsBuiltin(id) != 0 }, { "detected", true },
                { "displayFrequency", frequency },
                { "label", QString::fromUtf8(screen.localizedName.UTF8String) },
                { "touchSupport", "unknown" }, { "accelerometerSupport", "unknown" } });
        }

        return { { "primaryId", primary_id }, { "displays", displays } };
    }
}

QJsonObject cursor_screen_point()
{
    @autoreleasepool {
        auto* screens = [NSScreen screens];
        auto position = [NSEvent mouseLocation];
        auto height = screens.count ? NSHeight(screens[0].frame) : 0;
        return { { "x", std::llround(position.x) }, { "y", std::llround(height - position.y) } };
    }
}
}
