/** Compile the real unavailable provider without Qt or any OS preferences/permission action. */
#include "native/system-preferences/Provider.h"
#include <cstdio>

int main()
{
    auto provider = Firefly::SystemPreferences::Provider::create();
    bool passed = true;
    auto check = [&](char const* code) {
        auto string = provider->get_user_default("fixture", "string");
        auto boolean = provider->get_user_default("fixture", "boolean");
        auto status = provider->media_access_status("camera");
        passed &= string.code == code && boolean.code == code && status.code == code;
        passed &= string.json.empty() && boolean.json.empty() && status.json.empty();
        passed &= provider->set_user_default("fixture", "boolean", "true").code == code;
        passed &= provider->subscribe(0, "local", "fixture", [&](auto) { passed = false; }).code
            == code;
        passed &= provider->unsubscribe(0, "local").code == code;
    };

    check("ERR_FIREFLY_SYSTEM_PREFERENCES_UNAVAILABLE");
    provider->stop();
    provider->stop();
    check("ERR_FIREFLY_APP_EXITED");
    if (!passed)
        return 1;
    puts("PASS actual unavailable preferences provider rejects defaults, media and notifications "
         "without fabricated data");
    return 0;
}
