/** Select only owned runtime layouts; never confuse an upstream build helper with our renderer. */
#pragma once
#include <filesystem>

namespace Firefly {
enum class RuntimeLayout {
    MacOS,
    Linux,
};

std::filesystem::path runtime_directory(std::filesystem::path const& executable,
    std::filesystem::path const& configured, RuntimeLayout);
}
