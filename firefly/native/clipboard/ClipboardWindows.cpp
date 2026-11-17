/** Qt's native Windows clipboard owns the real immutable byte representations. */
#include "Clipboard.h"
#include "ClipboardQt.h"

namespace Firefly {
QJsonObject clipboard_request(QJsonObject const& request)
{
    return clipboard_request_qt(request, QStringLiteral("windows"));
}
}
