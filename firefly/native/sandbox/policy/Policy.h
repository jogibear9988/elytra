/** Main-process lifecycle latch. OS sandbox implementation remains owned by Ladybird. */
#pragma once
#include "../../../../mite/runtime/core/NativeCaptureLifetime.h"
#include <optional>

namespace Firefly::SandboxPolicy {
struct Failure {
    char const* code;
    char const* message;
};

class Policy : public Mite::NativeCaptureOwner<Policy> {
public:
    ~Policy()
    {
        this->revoke_native_captures();
    }

    template <typename Visitor> void visit_capture_edges(Visitor&) const
    {
        // Only native policy/Qt data; callbacks are native transport hooks.
    }

    std::optional<Failure> enable(bool native_sandbox_enabled);

    std::optional<Failure> check_window(bool native_sandbox_enabled) const;

    void ready()
    {
        m_ready = true;
    }

    void stop()
    {
        revoke_native_captures();

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
