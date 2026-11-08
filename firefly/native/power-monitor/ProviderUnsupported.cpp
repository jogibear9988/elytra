/** Unavailable providers never manufacture AC, nominal temperature or 100-percent speed. */
#include "Provider.h"

namespace Firefly::PowerMonitor {
namespace {
    Result unavailable()
    {
        return { 0, {}, false, "ERR_FIREFLY_POWER_UNAVAILABLE",
            "Native power monitoring is unavailable on this platform" };
    }

    class Unsupported final : public Provider {
    public:
        Result start() override
        {
            return unavailable();
        }

        void stop() override
        {
        }
    };
}

Result query(Query, int)
{
    return unavailable();
}

std::vector<std::string> Provider::capabilities()
{
    return {};
}

std::shared_ptr<Provider> Provider::create(EventCallback)
{
    return std::make_shared<Unsupported>();
}
}
