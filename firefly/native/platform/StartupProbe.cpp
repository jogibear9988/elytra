/** Pure native argument checks; no Qt, VM, window, or Electron is initialized. */
#include "StartupOptions.h"
#include <cstdio>
#include <initializer_list>
#include <string>
#include <vector>

static Firefly::StartupOptions parse(std::initializer_list<char const*> values)
{
    // Keep owned backing alive because production options intentionally borrow argv.
    static std::vector<std::string> strings;
    strings.clear();
    for (auto* value : values)
        strings.emplace_back(value);
    std::vector<char*> arguments;
    for (auto& value : strings)
        arguments.push_back(value.data());
    return Firefly::startup_options(static_cast<int>(arguments.size()), arguments.data());
}

int main()
{
    auto options = parse({ "firefly", "main.mjs", "--electron-compat", "a b", "Grüße" });
    if (options.electron_compat || !options.error.empty() || options.argc() != 5
        || std::string(options.arguments[2]) != "--electron-compat"
        || std::string(options.arguments[3]) != "a b"
        || std::string(options.arguments[4]) != "Grüße" || options.arguments.back() != nullptr)
        return 1;
    options = parse({ "firefly", "--electron-compat", "main.mjs", "a b" });
    if (!options.electron_compat || !options.error.empty() || options.argc() != 3
        || std::string(options.arguments[0]) != "firefly"
        || std::string(options.arguments[1]) != "main.mjs"
        || std::string(options.arguments[2]) != "a b" || options.arguments.back() != nullptr)
        return 2;
    for (auto values : { std::initializer_list<char const*> { "firefly" },
             { "firefly", "--electron-compat" }, { "firefly", "--unknown", "main.mjs" },
             { "firefly", "--electron-compat", "--electron-compat", "main.mjs" } }) {
        if (parse(values).error.empty())
            return 3;
    }

    if (!parse({ "firefly", "--help" }).help
        || !parse({ "firefly", "--electron-compat", "--help" }).help)
        return 4;
    std::puts("PASS native host-option stripping, app-argument preservation and explicit refusal");
    return 0;
}
