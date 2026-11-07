/** Independent native lifecycle/state proof, including refusal of disabled launch policy. */
#include "native/sandbox/policy/Policy.h"
#include <cstdlib>
#include <cstring>
#include <iostream>
using Firefly::SandboxPolicy::Policy;

namespace {
void check(bool condition, char const* message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}
}

static bool code(
    std::optional<Firefly::SandboxPolicy::Failure> const& failure, char const* expected)
{
    return failure.has_value() && std::strcmp(failure->code, expected) == 0;
}

int main()
{
    Policy policy;
    check(!policy.required(), "initial latch");
    check(code(policy.enable(false), "ERR_FIREFLY_SANDBOX_DISABLED") && !policy.required(),
        "disabled native policy accepted");
    check(!policy.enable(true) && !policy.enable(true) && policy.required(),
        "early idempotent enforcement");
    check(code(policy.check_window(true), "ERR_FIREFLY_NOT_READY"), "early window accepted");
    policy.ready();
    check(code(policy.enable(true), "ERR_FIREFLY_SANDBOX_TOO_LATE"), "late enable accepted");
    check(!policy.check_window(true), "protected native window refused");
    check(code(policy.check_window(false), "ERR_FIREFLY_SANDBOX_DISABLED") && policy.required(),
        "later disabled policy bypassed enforcement");
    policy.stop();
    check(code(policy.enable(true), "ERR_FIREFLY_APP_EXITED")
            && code(policy.check_window(true), "ERR_FIREFLY_APP_EXITED"),
        "stopped policy accepted work");
    Policy defaults;
    defaults.ready();
    check(!defaults.check_window(true) && defaults.check_window(false).has_value(),
        "default launch guard was weaker");
    std::cout
        << "PASS native global sandbox latch, phase, launch-policy mismatch and stop guards\n";
}
