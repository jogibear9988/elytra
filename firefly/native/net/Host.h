/** Trusted native network-state queries; no provider or monitor is exposed to renderers. */
#pragma once
#include "../platform/NetworkState.h"
#include <memory>

namespace JS {
class Realm;
class Object;
}

namespace Firefly::Network {
// Native test hosts may supply a controlled provider; production owns the OS provider.
std::shared_ptr<NetworkState> install_host(
    JS::Realm&, JS::Object&, std::shared_ptr<NetworkState> provider = {});
}
