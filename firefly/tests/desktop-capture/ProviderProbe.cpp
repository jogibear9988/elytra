/** No permission request, source enumeration, screenshot, Qt app or visible window. */
#include "native/desktop-capture/Provider.h"
#include <cstdio>
#include <vector>
#ifdef __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#endif

int main()
{
#ifdef __APPLE__
    bool permission = CGPreflightScreenCaptureAccess();
    if (Firefly::DesktopCapture::preflight() != permission) {
        fputs("FAIL desktop capture native permission preflight\n", stderr);
        return 1;
    }

    uint32_t count = 0;
    if (CGGetActiveDisplayList(0, nullptr, &count) != kCGErrorSuccess || count > 1024)
        return 2;
    std::vector<CGDirectDisplayID> displays(count);
    if (CGGetActiveDisplayList(count, displays.data(), &count) != kCGErrorSuccess)
        return 3;
    for (auto id : displays)
        if (!id || !CGDisplayIsActive(id))
            return 4;
    // Neither permission outcome is an authorization to enumerate in this probe.
    // Permission can belong to a responsible launcher, not an installed app.
    puts("PASS actual no-prompt desktop preflight and native display metadata; enumeration and "
         "capture not exercised");
#else
    bool completed = false;
    if (Firefly::DesktopCapture::preflight())
        return 1;
    Firefly::DesktopCapture::enumerate([&](auto result) {
        completed
            = result.sources.empty() && result.code == "ERR_FIREFLY_SCREEN_CAPTURE_UNAVAILABLE";
    });
    if (!completed)
        return 2;
    puts("PASS unsupported desktop provider fails explicitly without fabricated sources");
#endif
    return 0;
}
