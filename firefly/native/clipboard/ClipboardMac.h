/** Private Objective-C entry point for testing without changing the user's clipboard. */
#pragma once
#include <QJsonObject>
@class NSPasteboard;

namespace Firefly {
QJsonObject clipboard_request_on(NSPasteboard*, QJsonObject const&);
}
