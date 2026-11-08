/** Passive network availability and conservative system-proxy detection; no endpoint requests. */
#pragma once
#include <memory>
#include <string>

namespace Firefly {
struct NetworkStateResult {
    bool value {};
    std::string code;
    std::string message;
};

class NetworkState {
public:
    static std::unique_ptr<NetworkState> create();

    virtual ~NetworkState() = default;
    virtual NetworkStateResult is_online() = 0;
    // True blocks direct networking: the current system configuration contains
    // a proxy/PAC/WPAD route this first transport slice cannot faithfully apply.
    // An undetermined or malformed snapshot returns an explicit error, never false.
    virtual NetworkStateResult has_proxy() = 0;
    virtual void stop() = 0;
};
}
