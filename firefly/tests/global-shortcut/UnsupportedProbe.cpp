/** Exact unavailable backend; no platform integration or local-shortcut fallback. */
#include "native/global-shortcut/Provider.h"
#include <cstdio>

int main()
{
    auto provider = Firefly::GlobalShortcut::Provider::create([](int) { });
    if (provider->register_shortcut(1, { "A", 2 }).code != "ERR_FIREFLY_GLOBAL_SHORTCUT_UNAVAILABLE"
        || provider->unregister_shortcut(1).code != "ERR_FIREFLY_GLOBAL_SHORTCUT_UNAVAILABLE")
        return 1;
    provider->stop();
    provider->stop();
    if (provider->register_shortcut(2, { "A", 2 }).code != "ERR_FIREFLY_APP_EXITED"
        || provider->unregister_shortcut(1).code != "ERR_FIREFLY_APP_EXITED")
        return 1;
    puts("PASS exact unavailable globalShortcut backend and permanent stopped lifecycle; no native "
         "platform acceptance claim");
    return 0;
}
