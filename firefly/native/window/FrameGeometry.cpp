/** Pure checked arithmetic, separately testable without loading a GUI platform plugin. */
#include "Geometry.h"
#include <algorithm>
#include <limits>

namespace Firefly {
std::optional<QRect> content_rectangle(QRect requested, QMargins frame, bool content)
{
    qint64 x = requested.x(), y = requested.y();
    qint64 width = requested.width(), height = requested.height();
    if (!content) {
        x += frame.left();
        y += frame.top();
        width = std::max<qint64>(1, width - frame.left() - frame.right());
        height = std::max<qint64>(1, height - frame.top() - frame.bottom());
    }

    constexpr auto minimum = std::numeric_limits<int>::min();
    constexpr auto maximum = std::numeric_limits<int>::max();
    if (width < 1 || height < 1 || x < minimum || y < minimum
        || x + width - 1 + frame.right() > maximum || y + height - 1 + frame.bottom() > maximum
        || x - frame.left() < minimum || y - frame.top() < minimum)
        return {};
    return QRect(static_cast<int>(x), static_cast<int>(y), static_cast<int>(width),
        static_cast<int>(height));
}
}
