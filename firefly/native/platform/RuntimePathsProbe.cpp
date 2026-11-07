/** Exercise the production path selector without initializing Qt, LibJS or any GUI service. */
#include "RuntimePaths.h"
#include <iostream>
#include <string_view>

int main(int argc, char** argv)
{
    if (argc != 4)
        return 2;
    auto layout = std::string_view(argv[1]);
    if (layout != "macos" && layout != "linux")
        return 2;
    auto selected = Firefly::runtime_directory(argv[2], argv[3],
        layout == "macos" ? Firefly::RuntimeLayout::MacOS : Firefly::RuntimeLayout::Linux);
    std::cout << selected.string() << '\n';
    return 0;
}
