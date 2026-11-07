/** Runtime discovery is relative to the actual executable, independent of cwd and build paths. */
#include "RuntimePaths.h"

namespace Firefly {
std::filesystem::path runtime_directory(std::filesystem::path const& executable,
    std::filesystem::path const& configured, RuntimeLayout layout)
{
    if (executable.empty())
        return configured;
    auto directory = executable.parent_path();
    // macOS places the owned helper beside firefly in Contents/MacOS. On Linux,
    // upstream bin/WebContent also exists, so only the dedicated runtime tree is eligible.
    auto binary = layout == RuntimeLayout::MacOS ? directory
                                                 : directory.parent_path() / "firefly-runtime/bin";
    auto resources = layout == RuntimeLayout::MacOS ? binary.parent_path() / "Resources"
                                                    : binary.parent_path() / "share/Lagom";
    std::error_code error;
    if (std::filesystem::is_regular_file(binary / "WebContent", error)
        && std::filesystem::is_directory(resources, error))
        return binary;
    return configured;
}
}
