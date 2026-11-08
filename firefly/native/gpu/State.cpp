/** Map only positively selected CPU painting and actual helper termination evidence. */
#include "State.h"
#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#endif

namespace Firefly::GPU {
State::State(bool cpu_requested)
    : m_cpu_requested(cpu_requested)
{
}

bool State::request_cpu()
{
    if (m_sealed)
        return false;
    m_cpu_requested = true;
    return true;
}

void State::seal()
{
    m_sealed = true;
}

bool State::cpu_requested() const
{
    return m_cpu_requested;
}

bool State::observe(bool actual_cpu_policy)
{
    auto before = snapshot();
    m_observed_cpu = actual_cpu_policy;
    return before != snapshot();
}

bool State::invalidate()
{
    auto before = snapshot();
    m_observed_cpu.reset();
    return before != snapshot();
}

std::string State::snapshot() const
{
    if (m_observed_cpu == true)
        return R"({"gpu_compositing":"disabled_software","rasterization":"disabled_software"})";
    return "{}";
}

Exit classify_exit(std::optional<int> status)
{
#ifndef _WIN32
    if (status) {
        if (WIFEXITED(*status)) {
            int code = WEXITSTATUS(*status);
            return { code == 0 ? "clean-exit" : "abnormal-exit", code };
        }

        if (WIFSIGNALED(*status)) {
            int signal = WTERMSIG(*status);
            // Core-dump signals are direct crash evidence. SIGKILL can be a user,
            // kernel or test kill; do not invent OOM or integrity-failure reasons.
            bool crash = signal == SIGABRT || signal == SIGBUS || signal == SIGFPE
                || signal == SIGILL || signal == SIGSEGV || signal == SIGTRAP;
            return { crash ? "crashed" : "killed", 128 + signal };
        }
    }
#else
    (void)status;
#endif
    return { "unknown", -1 };
}
}
