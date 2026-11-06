/** Bounded macOS clipboard protocol; the probe uses an isolated named pasteboard. */
#pragma once
#include <QJsonObject>

namespace Firefly {
QJsonObject clipboard_request(QJsonObject const&);
}
