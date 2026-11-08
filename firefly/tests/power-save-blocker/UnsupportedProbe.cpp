/** Real unsupported-provider policy; compiling locally does not accept another native platform. */
#include "native/power-save-blocker/Provider.h"
#include <cstdio>

int main()
{
    using namespace Firefly::PowerSaveBlocker;
    auto provider = Provider::create();
    for (auto type : { Type::AppSuspension, Type::DisplaySleep }) {
        auto result = provider->start(type);
        if (result.code != "ERR_FIREFLY_POWER_SAVE_UNAVAILABLE" || result.flag || result.id != -1)
            return 1;
    }

    for (int id : { -1, 0, 1, 2147483647 }) {
        if (provider->stop(id).flag || provider->is_started(id).flag)
            return 1;
    }

    provider->shutdown();
    provider->shutdown();
    if (provider->start(Type::AppSuspension).code != "ERR_FIREFLY_APP_EXITED"
        || provider->is_started(0).flag || provider->stop(0).flag)
        return 1;
    puts("PASS actual unsupported power-save provider explicit refusal, unknown IDs and permanent "
         "shutdown");
    puts("Provider policy only; no Windows/Linux OS assertion support is claimed");
    return 0;
}
