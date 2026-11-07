/**
 * Starts the packaged application using its recorded runtime/resources. Resolve paths
 * relative to the bundle rather than a developer checkout or working directory.
 */

// Finder entry point: locate the bundled runtime and app without a working-directory assumption.
#include <cstdio>
#include <filesystem>
#include <mach-o/dyld.h>
#include <string>
#include <unistd.h>
#include <vector>
#ifndef FIREFLY_ELECTRON_COMPAT
#define FIREFLY_ELECTRON_COMPAT 0
#endif

int main(int argc, char** argv)
{
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> storage(size);
    if (_NSGetExecutablePath(storage.data(), &size))
        return 1;
    auto directory = std::filesystem::weakly_canonical(storage.data()).parent_path();
    auto runtime = (directory / "firefly").string();
    auto entry = (directory.parent_path() / "Resources/app/main.mjs").string();
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
    std::perror("Cannot launch bundled Firefly");
    return 1;
}
