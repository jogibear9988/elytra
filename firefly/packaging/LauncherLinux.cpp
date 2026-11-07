/** Start the packaged Linux app relative to /proc/self/exe, preserving native child ownership. */
#ifndef __linux__
#error This launcher requires native Linux
#endif
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <unistd.h>
#include <vector>
#ifndef FIREFLY_ENTRY
#define FIREFLY_ENTRY "app/main.mjs"
#endif
#ifndef FIREFLY_ELECTRON_COMPAT
#define FIREFLY_ELECTRON_COMPAT 0
#endif

int main(int argc, char** argv)
{
    std::error_code error;
    auto executable = std::filesystem::canonical("/proc/self/exe", error);
    if (error) {
        std::fputs("Cannot locate packaged Firefly launcher\n", stderr);
        return 1;
    }

    auto directory = executable.parent_path();
    auto runtime = (directory / "bin/firefly").string();
    auto entry = (directory / FIREFLY_ENTRY).string();
    // This runtime currently supports X11. Preserve an explicit caller choice
    // so the runtime can refuse unsupported Wayland rather than silently switch.
    if (setenv("QT_QPA_PLATFORM", "xcb", 0) != 0)
        return 1;
    std::vector<char*> arguments { runtime.data() };
#if FIREFLY_ELECTRON_COMPAT
    char compatibility[] = "--electron-compat";
    arguments.push_back(compatibility);
#endif
    arguments.push_back(entry.data());
    for (int i = 1; i < argc; ++i)
        arguments.push_back(argv[i]);
    arguments.push_back(nullptr);
    execv(runtime.c_str(), arguments.data());
    std::perror("Cannot launch packaged Firefly");
    return 1;
}
