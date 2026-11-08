/** Explicit platform gap; application-local Qt shortcuts are not global shortcuts. */
#include "Provider.h"

namespace Firefly::GlobalShortcut {
namespace {
    class Unsupported final : public Provider {
    public:
        Result register_shortcut(int, Chord const&) override
        {
            return unavailable();
        }

        Result unregister_shortcut(int) override
        {
            return unavailable();
        }

        void stop() override
        {
            stopped = true;
        }

    private:
        Result unavailable() const
        {
            return { false,
                stopped ? "ERR_FIREFLY_APP_EXITED" : "ERR_FIREFLY_GLOBAL_SHORTCUT_UNAVAILABLE",
                "Native global shortcuts unavailable" };
        }

        bool stopped {};
    };
}

std::unique_ptr<Provider> Provider::create(EventCallback)
{
    return std::make_unique<Unsupported>();
}
}
