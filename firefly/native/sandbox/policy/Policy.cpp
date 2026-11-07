/** Never report successful enforcement when native helper launch policy disables sandboxing. */
#include "Policy.h"

namespace Firefly::SandboxPolicy {
std::optional<Failure> Policy::enable(bool native_sandbox_enabled)
{
    if (m_stopped)
        return Failure { "ERR_FIREFLY_APP_EXITED", "Application has exited" };
    if (m_ready)
        return Failure { "ERR_FIREFLY_SANDBOX_TOO_LATE",
            "enableSandbox must be called before readiness" };

    if (!native_sandbox_enabled)
        return Failure { "ERR_FIREFLY_SANDBOX_DISABLED",
            "Native helper launch policy disables sandboxing" };

    m_required = true;
    return {};
}

std::optional<Failure> Policy::check_window(bool native_sandbox_enabled) const
{
    if (m_stopped)
        return Failure { "ERR_FIREFLY_APP_EXITED", "Application has exited" };
    if (!m_ready)
        return Failure { "ERR_FIREFLY_NOT_READY", "Window creation requires readiness" };
    // Firefly has no unsandboxed renderer mode, even without the explicit latch.
    if (!native_sandbox_enabled)
        return Failure { "ERR_FIREFLY_SANDBOX_DISABLED",
            "Native helper launch policy disables sandboxing" };

    return {};
}
}
