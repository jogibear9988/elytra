#include "Provider.h"

namespace Firefly::SystemPreferences {
namespace {
    class Unsupported final : public Provider {
    public:
        ValueResult get_user_default(std::string const&, std::string const&) override
        {
            return unavailable_value();
        }

        Result set_user_default(std::string const&, std::string const&, std::string const&) override
        {
            return unavailable();
        }

        ValueResult media_access_status(std::string const&) override
        {
            return unavailable_value();
        }

        Result subscribe(
            int, std::string const&, std::optional<std::string> const&, EventCallback) override
        {
            return unavailable();
        }

        Result unsubscribe(int, std::string const&) override
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
            return { stopped ? "ERR_FIREFLY_APP_EXITED"
                             : "ERR_FIREFLY_SYSTEM_PREFERENCES_UNAVAILABLE",
                "Native system preferences unavailable" };
        }

        ValueResult unavailable_value() const
        {
            auto error = unavailable();
            return { error.code, error.message, {} };
        }

        bool stopped {};
    };
}

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<Unsupported>();
}
}
