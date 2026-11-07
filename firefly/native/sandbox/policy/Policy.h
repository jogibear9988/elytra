/** Main-process lifecycle latch. OS sandbox implementation remains owned by Ladybird. */
#pragma once
#include <optional>

namespace Firefly::SandboxPolicy {
struct Failure {
    char const* code;
    char const* message;
};

class Policy {
public:
    std::optional<Failure> enable(bool native_sandbox_enabled);

    std::optional<Failure> check_window(bool native_sandbox_enabled) const;

    void ready()
    {
        m_ready = true;
    }

    void stop()
    {
        m_stopped = true;
    }

    bool required() const
    {
        return m_required;
    }

private:
    bool m_required { false };
    bool m_ready { false };
    bool m_stopped { false };
};
}
