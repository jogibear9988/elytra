/** No launch or trash success without a native desktop provider. */
#include "Provider.h"

namespace Firefly::Shell {
namespace {
    class UnsupportedProvider final : public Provider {
    public:
        Result reveal(std::string const&) override
        {
            return failure();
        }

        void open_external(std::string, bool, Completion callback) override
        {
            callback(failure());
        }

        void open_path(std::string, Completion callback) override
        {
            callback(failure());
        }

        void trash(std::string, Completion callback) override
        {
            callback(failure());
        }

        void stop() override
        {
            m_stopped = true;
        }

    private:
        Result failure() const
        {
            return { m_stopped ? "ERR_FIREFLY_APP_EXITED" : "ERR_FIREFLY_SHELL_UNAVAILABLE",
                "Native desktop shell operations are unavailable" };
        }

        bool m_stopped { false };
    };
}

std::unique_ptr<Provider> Provider::create()
{
    return std::make_unique<UnsupportedProvider>();
}
}
