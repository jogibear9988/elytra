/** Read-only native preferences and notification ownership; no QApplication or visible windows. */
#include "ThemeMac.h"
#include <AppKit/AppKit.h>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QObject>
#include <cstdio>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    @autoreleasepool {
        auto value = Firefly::system_theme_snapshot();
        for (auto const& field :
            { "dark", "shouldUseHighContrastColors", "shouldUseInvertedColorScheme",
                "prefersReducedTransparency", "shouldDifferentiateWithoutColor" }) {
            if (!value[field].isBool())
                return 1;
        }

        int calls = 0;
        auto* center = [NSWorkspace sharedWorkspace].notificationCenter;
        {
            QObject context;
            Firefly::watch_accessibility_theme(context, [&calls] { ++calls; });
            [center postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                                  object:nil];
            QCoreApplication::processEvents();
            if (calls != 1)
                return 2;
        }

        [center postNotificationName:NSWorkspaceAccessibilityDisplayOptionsDidChangeNotification
                              object:nil];
        QCoreApplication::processEvents();
        if (calls != 1)
            return 3;
        auto bytes = QJsonDocument(value).toJson(QJsonDocument::Compact);
        fwrite(bytes.data(), 1, bytes.size(), stdout);
        fputc('\n', stdout);
        return 0;
    }
}
