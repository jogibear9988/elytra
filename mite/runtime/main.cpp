#include "Runtime.h"
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
// Keep upstream's Windows include boundary local to this platform entry point.
#include <AK/Windows.h>

int wmain(int argc, wchar_t** wide_argv)
{
    static_assert(sizeof(wchar_t) == 2);
    std::vector<std::string> storage;
    storage.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; ++i) {
        auto size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_argv[i], -1, nullptr, 0, nullptr, nullptr);
        if (!size) {
            fprintf(stderr, "mite: cannot encode command-line argument as UTF-8 (Windows error %lu)\n", GetLastError());
            return 9;
        }
        std::string bytes(static_cast<size_t>(size), '\0');
        if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_argv[i], -1, bytes.data(), size, nullptr, nullptr) != size) {
            fprintf(stderr, "mite: cannot encode command-line argument as UTF-8 (Windows error %lu)\n", GetLastError());
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
    return Mite::run(argc, argv);
}
#endif
