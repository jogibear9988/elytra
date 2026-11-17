/**
 * Native Mite executable entry. Converts platform arguments to the runtime UTF-8 contract and
 * delegates VM/application ownership to the embedding entry point.
 */

#include "core/Runtime.h"
#include "platform/process/SignalPolicy.h"
#include <cstdio>

static int run_cli(int argc, char** argv)
{
    Mite::HostHooks hooks;
    hooks.allow_process_signals = true;
    return Mite::run(argc, argv, hooks);
}

#ifdef _WIN32
// Keep upstream's Windows include boundary local to this platform entry point.
#include "platform/UV.h"
#include "platform/process/WindowsArguments.h"
#include <fcntl.h>
#include <io.h>

int wmain(int argc, wchar_t** wide_argv)
{
    // uv_spawn explicitly duplicates requested stdio, but Windows otherwise
    // inherits any inheritable handles. Do not let a stdio:'ignore' child retain
    // the CLI launcher's output pipes after this parent has already exited.
    // This is process-start policy, not an implicit change in embedded run().
    uv_disable_stdio_inheritance();
    // Preserve raw CLI pipe bytes: CRT text input translates CRLF/Ctrl-Z, and
    // text output expands LF. Node leaves those bytes unchanged. Configure only
    // CLI-owned descriptors; embedded hosts retain their own CRT stream policy.
    for (auto* stream : { stdin, stdout, stderr }) {
        auto descriptor = _fileno(stream);
        if (descriptor >= 0 && _setmode(descriptor, _O_BINARY) == -1) {
            fputs("mite: cannot configure binary CLI stdio\n", stderr);
            return 9;
        }
    }

    Mite::WindowsArguments arguments;
    if (!arguments.initialize(argc, wide_argv)) {
        fprintf(stderr, "mite: cannot encode command-line argument as UTF-8 (Windows error %lu)\n",
            arguments.error());
        return 9;
    }

    return run_cli(argc, arguments.data());
}
#else
int main(int argc, char** argv)
{
    if (!Mite::ignore_broken_pipe_signal()) {
        fputs("mite: cannot configure broken-pipe signal policy\n", stderr);
        return 9;
    }

    return run_cli(argc, argv);
}
#endif
