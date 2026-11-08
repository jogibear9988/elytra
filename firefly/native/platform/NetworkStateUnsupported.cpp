/** Explicit unavailable backend until actual native network-state acceptance exists. */
#include "NetworkState.h"

namespace Firefly {
namespace {
    class UnsupportedNetworkState final : public NetworkState {
    public:
        NetworkStateResult is_online() override
        {
            return unavailable();
        }

        NetworkStateResult has_proxy() override
        {
            return unavailable();
        }

        void stop() override
        {
            stopped = true;
        }

    private:
        NetworkStateResult unavailable() const
        {
            return { false, stopped ? "ERR_FIREFLY_APP_EXITED" : "ERR_FIREFLY_NETWORK_UNAVAILABLE",
                "Native network state unavailable" };
        }

        bool stopped {};
    };
}

std::unique_ptr<NetworkState> NetworkState::create()
{
    return std::make_unique<UnsupportedNetworkState>();
}
}
