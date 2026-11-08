/** No fabricated desktop sources or permission success on unsupported platforms. */
#include "Provider.h"
#include <utility>

namespace Firefly::DesktopCapture {
bool preflight()
{
    return false;
}

void enumerate(Completion complete)
{
    if (complete)
        complete({ {}, "ERR_FIREFLY_SCREEN_CAPTURE_UNAVAILABLE",
            "Native desktop source enumeration is unavailable on this platform" });
}
}
