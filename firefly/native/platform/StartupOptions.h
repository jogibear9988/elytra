/** Executable-owned import policy; application arguments never become host options. */
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace Firefly {
struct StartupOptions {
    bool electron_compat { false };
    bool help { false };
    std::string error;
    // Borrow argv storage for the full run; include the terminating null pointer.
    std::vector<char*> arguments;

    int argc() const
    {
        return arguments.empty() ? 0 : static_cast<int>(arguments.size()) - 1;
    }
};

inline StartupOptions startup_options(int argc, char** argv)
{
    StartupOptions result;
    int entry = 1;
    if (entry < argc && std::string_view(argv[entry]) == "--electron-compat") {
        result.electron_compat = true;
        ++entry;
    }

    if (entry < argc && std::string_view(argv[entry]) == "--help") {
        result.help = true;
        return result;
    }

    if (entry >= argc) {
        result.error = "Missing application entry";
        return result;
    }

    if (std::string_view(argv[entry]).starts_with('-')) {
        result.error = "Unsupported Firefly CLI option";
        return result;
    }

    result.arguments.push_back(argv[0]);

    for (int index = entry; index < argc; ++index)
        result.arguments.push_back(argv[index]);

    result.arguments.push_back(nullptr);

    return result;
}
}
