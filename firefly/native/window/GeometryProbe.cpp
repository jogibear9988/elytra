/** Exercise frame translation and integer boundaries without creating a GUI application. */
#include "Geometry.h"
#include <cstdio>
#include <limits>

int main()
{
    using Firefly::content_rectangle;
    QMargins frame(4, 28, 4, 4);
    if (content_rectangle(QRect(-800, -200, 640, 480), frame, false) != QRect(-796, -172, 632, 448))
        return 1;
    if (content_rectangle(QRect(-796, -172, 632, 448), frame, true) != QRect(-796, -172, 632, 448))
        return 2;
    if (content_rectangle(QRect(-10, 20, 640, 480), {}, false) != QRect(-10, 20, 640, 480))
        return 3;
    if (content_rectangle(QRect(0, 0, 1, 1), frame, false) != QRect(4, 28, 1, 1))
        return 4;
    auto maximum = std::numeric_limits<int>::max();
    auto minimum = std::numeric_limits<int>::min();
    if (content_rectangle(QRect(maximum - 5, 0, 4, 10), frame, true).has_value()
        || content_rectangle(QRect(minimum, 0, 1, 10), frame, true).has_value()
        || content_rectangle(QRect(0, maximum - 5, 10, 4), frame, true).has_value()
        || content_rectangle(QRect(0, minimum, 10, 1), frame, true).has_value())
        return 5;
    puts("PASS native checked outer/content DIP translation negative origins frame minima and "
         "overflow");
    return 0;
}
