/** Unimplemented platforms must never invent online/offline or proxy-free observations. */
#include "native/platform/NetworkState.h"
#include <cstdio>

int main()
{
    auto provider = Firefly::NetworkState::create();
    if (!provider || provider->is_online().code != "ERR_FIREFLY_NETWORK_UNAVAILABLE"
        || provider->has_proxy().code != "ERR_FIREFLY_NETWORK_UNAVAILABLE")
        return 1;
    provider->stop();
    provider->stop();
    if (provider->is_online().code != "ERR_FIREFLY_APP_EXITED"
        || provider->has_proxy().code != "ERR_FIREFLY_APP_EXITED")
        return 1;
    puts("PASS explicit unavailable network backend and idempotent retirement");
}
