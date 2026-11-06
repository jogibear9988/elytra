/** Bounded native clipboard request protocol; platform providers never retain VM objects. */
#pragma once
#include <QJsonObject>

namespace Firefly {
QJsonObject clipboard_request(QJsonObject const&);
}
