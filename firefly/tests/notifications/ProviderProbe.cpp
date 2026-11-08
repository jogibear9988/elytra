/** Read-only native support/identity preflight; never show, authorize or schedule. */
#include "native/notifications/Provider.h"
#include <cstdio>

int main()
{
    // CMake builds this as a bare console executable, not an .app bundle.
    // It must fail identity preflight before calling the notification center.
    if (Firefly::Notifications::Provider::supported()) {
        fputs("FAIL bare notification probe must not claim bundled application support\n", stderr);
        return 1;
    }

    puts("PASS notification OS/bundle preflight only; authorization, scheduling and delegate "
         "mutation not exercised");
    return 0;
}
