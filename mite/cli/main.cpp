/**
 * Native Mite executable entry. Converts platform arguments to the runtime UTF-8 contract and
 * delegates VM/application ownership to the embedding entry point.
 */

#include "core/Runtime.h"
#include "platform/process/SignalPolicy.h"
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
// Keep upstream's Windows include boundary local to this platform entry point.
#include <AK/Windows.h>
#include <fcntl.h>
#include <io.h>
#include <uv.h>

int wmain(int argc, wchar_t** wide_argv)
{
    // uv_spawn explicitly duplicates requested stdio, but Windows otherwise
    // inherits any inheritable handles. Do not let a stdio:'ignore' child retain
    // the CLI launcher's output pipes after this parent has already exited.
    // This is process-start policy, not an implicit change in embedded run().
    uv_disable_stdio_inheritance();
    // The CRT text default turns every LF written by console.log/error into
    // CRLF, including when output is a child-process pipe. Node writes these
    // bytes unchanged. Set CLI-owned output descriptors before any diagnostics;
    // embedded hosts retain control over their own CRT stream configuration.
    for (auto* stream : { stdout, stderr }) {
        auto descriptor = _fileno(stream);
        if (descriptor >= 0 && _setmode(descriptor, _O_BINARY) == -1) {
            fputs("mite: cannot configure binary CLI output\n", stderr);
            return 9;
        }
    }

    static_assert(sizeof(wchar_t) == 2);
    std::vector<std::string> storage;
    storage.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        auto size = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (!size) {
            fprintf(stderr,
                "mite: cannot encode command-line argument as UTF-8 (Windows error %lu)\n",
                GetLastError());
            return 9;
        }

        std::string bytes(static_cast<size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_argv[i], -1, bytes.data(), size,
                nullptr, nullptr)
            != size) {
            fprintf(stderr,
                "mite: cannot encode command-line argument as UTF-8 (Windows error %lu)\n",
                GetLastError());
            return 9;
        }

        bytes.pop_back(); // The conversion count includes the terminating NUL.
        storage.push_back(std::move(bytes));
    }

    std::vector<char*> argv;
    argv.reserve(static_cast<size_t>(argc) + 1);
    for (auto& bytes : storage)
        argv.push_back(bytes.data());
    argv.push_back(nullptr);
    return Mite::run(argc, argv.data());
}
#else
int main(int argc, char** argv)
{
    if (!Mite::ignore_broken_pipe_signal()) {
        fputs("mite: cannot configure broken-pipe signal policy\n", stderr);
        return 9;
    }

    return Mite::run(argc, argv);
}
#endif
