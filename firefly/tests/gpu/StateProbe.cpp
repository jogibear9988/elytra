/** Native policy/termination evidence without a GPU, GUI, browser or VM. */
#include "native/gpu/State.h"
#include <cstdio>
#ifndef _WIN32
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

int main()
{
    using Firefly::GPU::State;
    auto check = [](bool value, char const* message) {
        if (!value)
            std::fprintf(stderr, "FAIL native GPU policy: %s\n", message);
        return value;
    };

    State state;
    if (!check(state.snapshot() == "{}", "uninitialized policy is unknown")
        || !check(state.request_cpu() && state.request_cpu(), "early CPU selection is idempotent")
        || !check(state.snapshot() == "{}", "request alone is not an observed backend"))
        return 1;
    state.seal();
    if (!check(!state.request_cpu(), "sealed native initialization refuses late changes")
        || !check(
            !state.observe(false) && state.snapshot() == "{}", "unforced backend remains unknown"))
        return 1;
    if (!check(state.observe(true), "native-selected CPU policy changes status")
        || !check(state.snapshot()
                == R"({"gpu_compositing":"disabled_software","rasterization":"disabled_software"})",
            "only selected software fields reported")
        || !check(!state.observe(true), "unchanged native policy does not manufacture updates"))
        return 1;
    if (!check(state.invalidate() && state.snapshot() == "{}" && !state.invalidate(),
            "terminal host cleanup revokes policy knowledge once"))
        return 1;
    State launched(true);
    if (!check(launched.cpu_requested() && launched.snapshot() == "{}",
            "launch selection waits for actual initialization"))
        return 1;
    auto unknown = Firefly::GPU::classify_exit({});
    if (!check(unknown.reason == "unknown" && unknown.exit_code == -1,
            "missing evidence is not a crash"))
        return 1;
#ifndef _WIN32
    for (int code : { 0, 7, -SIGKILL }) {
        pid_t child = fork();
        if (child == 0) {
            if (code < 0)
                raise(-code);
            _exit(code);
        }

        if (child < 0)
            return 1;
        int status;
        if (waitpid(child, &status, 0) != child)
            return 1;
        auto exit = Firefly::GPU::classify_exit(status);
        auto expected = code == 0 ? "clean-exit" : code > 0 ? "abnormal-exit" : "killed";
        if (!check(exit.reason == expected && exit.exit_code == (code < 0 ? 128 - code : code),
                "actual owned child wait status classification"))
            return 1;
    }

    if (!check(Firefly::GPU::classify_exit(SIGSEGV).reason == "crashed",
            "synthetic fatal-signal classification"))
        return 1;
#endif
    std::puts("PASS native selected CPU/unknown policy, sealing and actual owned-child exit "
              "classification; no GPU or desktop started");
    return 0;
}
