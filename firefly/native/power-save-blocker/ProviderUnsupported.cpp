/** No synthetic blocker success when a native provider is unavailable. */
#include "Provider.h"

namespace Firefly::PowerSaveBlocker {
namespace {
    class UnsupportedProvider final : public Provider {
    public:
        Result start(Type) override
        {
            return { -1, false,
                m_stopped ? "ERR_FIREFLY_APP_EXITED" : "ERR_FIREFLY_POWER_SAVE_UNAVAILABLE",
                "Native power save blockers are unavailable" };
        }

        Result stop(int) override
        {
            return {};
        }

        Result is_started(int id) const override
        {
            return { id, false, {}, {} };
        }

        void shutdown() override
        {
            m_stopped = true;
        }

    private:
        bool m_stopped { false };
    };
}

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<UnsupportedProvider>();
}
}
