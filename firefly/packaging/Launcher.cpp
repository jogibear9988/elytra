// Finder entry point: locate the bundled runtime and app without a working-directory assumption.
#include <mach-o/dyld.h>
#include <unistd.h>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
int main(int argc, char** argv)
{
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> storage(size);
    if (_NSGetExecutablePath(storage.data(), &size)) return 1;
    auto directory = std::filesystem::weakly_canonical(storage.data()).parent_path();
    auto runtime = (directory / "firefly").string();
    auto entry = (directory.parent_path() / "Resources/app/main.mjs").string();
    std::vector<char*> arguments { runtime.data(), entry.data() };
    for (int i = 1; i < argc; ++i) arguments.push_back(argv[i]);
    arguments.push_back(nullptr);
    execv(runtime.c_str(), arguments.data());
    std::perror("Cannot launch bundled Firefly");
    return 1;
}
