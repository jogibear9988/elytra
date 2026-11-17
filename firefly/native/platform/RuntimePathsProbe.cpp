/** Exercise the production path selector without initializing Qt, LibJS or any GUI service. */
#include "RuntimePaths.h"
#include <iostream>
#include <string_view>

namespace {
int run(std::string_view layout, std::filesystem::path executable, std::filesystem::path fallback)
{
    if (layout != "macos" && layout != "linux" && layout != "windows")
        return 2;
    auto native_layout = layout == "macos" ? Firefly::RuntimeLayout::MacOS
        : layout == "windows"              ? Firefly::RuntimeLayout::Windows
                                           : Firefly::RuntimeLayout::Linux;
    auto selected = Firefly::runtime_directory(executable, fallback, native_layout);
    auto utf8 = selected.u8string();
    std::cout << std::string(reinterpret_cast<char const*>(utf8.data()), utf8.size()) << '\n';
    return 0;
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    if (argc != 4)
        return 2;
    auto layout = std::wstring_view(argv[1]);
    return run(layout == L"windows" ? "windows"
            : layout == L"macos"    ? "macos"
            : layout == L"linux"    ? "linux"
                                    : "invalid",
        argv[2], argv[3]);
}
#else
int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    return run(argv[1], std::filesystem::u8path(argv[2]), std::filesystem::u8path(argv[3]));
}
#endif
