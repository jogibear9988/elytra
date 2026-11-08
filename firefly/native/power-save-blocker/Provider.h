/** Host-owned logical blockers backed by real OS assertions. Calls are synchronous and owner-only.
 */
#pragma once
#include <cstddef>
#include <memory>
#include <string>

namespace Firefly::PowerSaveBlocker {
enum class Type { AppSuspension, DisplaySleep };

struct Result {
    int id { -1 };
    bool flag { false };
    std::string code;
    std::string message;
};

class Provider {
public:
    static constexpr size_t request_limit = 64;
    static std::unique_ptr<Provider> create();

    virtual ~Provider() = default;
    virtual Result start(Type) = 0;
    virtual Result stop(int) = 0;
    virtual Result is_started(int) const = 0;
    // Permanently revokes logical requests. Failed native releases remain owned
    // for bounded destructor retries; shutdown does not promise OS release success.
    virtual void shutdown() = 0;
};
}
