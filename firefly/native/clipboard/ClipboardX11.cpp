/** Select the genuine xcb clipboard; no offscreen or alternate platform fallback. */
#include "Clipboard.h"
#include "ClipboardQt.h"

namespace Firefly {
QJsonObject clipboard_request(QJsonObject const& request)
{
    return clipboard_request_qt(request, QStringLiteral("xcb"));
}
}
