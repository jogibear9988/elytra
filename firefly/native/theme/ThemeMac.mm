/** Read actual macOS preferences without changing global defaults or creating a window. */
#include "ThemeMac.h"
#include <AppKit/AppKit.h>
#include <QObject>
#include <QPointer>

namespace Firefly {
QJsonObject system_theme_snapshot()
{
    @autoreleasepool {
        auto* workspace = [NSWorkspace sharedWorkspace];
        auto* style = [[NSUserDefaults standardUserDefaults] stringForKey:@"AppleInterfaceStyle"];
        return { { "dark", [style isEqualToString:@"Dark"] != NO },
            { "shouldUseHighContrastColors",
                workspace.accessibilityDisplayShouldIncreaseContrast != NO },
            { "shouldUseInvertedColorScheme",
                workspace.accessibilityDisplayShouldInvertColors != NO },
            { "prefersReducedTransparency",
                workspace.accessibilityDisplayShouldReduceTransparency != NO },
            { "shouldDifferentiateWithoutColor",
                workspace.accessibilityDisplayShouldDifferentiateWithoutColor != NO } };
    }
}

void watch_accessibility_theme(QObject& context, std::function<void()> changed)
{
    @autoreleasepool {
        QPointer<QObject> guard(&context);
        auto* center = [NSWorkspace sharedWorkspace].notificationCenter;
        auto token =
            [center addObserverForName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                                object:nil
                                 queue:[NSOperationQueue mainQueue]
                            usingBlock:^(NSNotification*) {
                                if (guard)
                                    changed();
                            }];
        [token retain];
        // A queued notification may outlive disconnect; its QPointer guard revokes it.
        QObject::connect(&context, &QObject::destroyed, [center, token] {
            [center removeObserver:token];
            [token release];
        });
    }
}
}
