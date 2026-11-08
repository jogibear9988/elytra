/** Exercise the actual unavailable provider without Qt, a desktop, or an OS shell action. */
#include "native/shell/Provider.h"
#include <cstdio>

int main()
{
    auto provider = Firefly::Shell::Provider::create();
    bool passed = true;
    unsigned callbacks = 0;
    auto check = [&](char const* expected) {
        auto reveal = provider->reveal("/unavailable/fixture");
        passed &= reveal.code == expected && !reveal.message.empty();
        auto complete = [&, expected](Firefly::Shell::Result result) {
            ++callbacks;
            passed &= result.code == expected && !result.message.empty();
        };

        provider->open_external("https://example.invalid", false, complete);
        provider->open_path("/unavailable/fixture", complete);
        provider->trash("/unavailable/fixture", complete);
    };

    check("ERR_FIREFLY_SHELL_UNAVAILABLE");
    provider->stop();
    provider->stop();
    check("ERR_FIREFLY_APP_EXITED");
    if (!passed || callbacks != 6)
        return 1;
    std::puts("PASS actual unavailable shell provider rejects every operation and remains stopped; "
              "no OS actions");
    return 0;
}
