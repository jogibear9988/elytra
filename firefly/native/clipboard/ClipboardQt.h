/** Shared bounded clipboard protocol over a specifically selected native Qt backend. */
#pragma once
#include <QJsonObject>
#include <QString>

namespace Firefly {
QJsonObject clipboard_request_qt(QJsonObject const&, QString const& platform);
}
