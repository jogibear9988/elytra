/** Unsupported systems report no native power capabilities and explicit query failures. */
#include "native/power-monitor/Provider.h"
#include <cstdio>
using namespace Firefly::PowerMonitor;

int main()
{
    if (!Provider::capabilities().empty() || query(Query::BatteryPower).code.empty()
        || query(Query::SpeedLimit).code.empty() || Provider::create({})->start().code.empty())
        return 1;
    puts("PASS unsupported power queries and events fail explicitly");
    return 0;
}
