/** Native-selected painting policy, independent of VM or renderer/device heuristics. */
#pragma once
#include <optional>
#include <string>

namespace Firefly::GPU {
class State {
public:
    explicit State(bool cpu_requested = false);

    bool request_cpu();

    void seal();

    bool cpu_requested() const;

    bool observe(bool actual_cpu_policy);

    bool invalidate();

    std::string snapshot() const;

private:
    bool m_cpu_requested {};
    bool m_sealed {};
    std::optional<bool> m_observed_cpu;
};

struct Exit {
    std::string reason;
    int exit_code {};
};

// Status is the actual POSIX wait status supplied by the owned process monitor.
// An absent status remains unknown; a disconnect alone is never called a crash.
Exit classify_exit(std::optional<int> wait_status);
}
