/** Unsupported systems never simulate notification presentation. */
#include "Provider.h"
#include <utility>

namespace Firefly::Notifications {
namespace {
    class Unsupported final : public Provider {
    public:
        explicit Unsupported(EventCallback callback)
            : m_callback(std::move(callback))
        {
        }

        void show(uint64_t token, Options) override
        {
            if (m_callback)
                m_callback({ token, "failed", "Native notifications are unavailable", -1 });
        }

        void close(uint64_t) override
        {
        }

        void stop() override
        {
            m_callback = {};
        }

    private:
        EventCallback m_callback;
    };
}

bool Provider::supported()
{
    return false;
}

std::shared_ptr<Provider> Provider::create(EventCallback callback)
{
    return std::make_shared<Unsupported>(std::move(callback));
}
}
