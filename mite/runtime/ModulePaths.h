#pragma once

#include <AK/StringView.h>
#include <LibURL/URL.h>
#include <filesystem>
#include <string>
#include <system_error>

namespace Mite::ModulePaths {
namespace fs = std::filesystem;

// Node module identity resolves links without expanding ordinary Windows 8.3
// names or changing the caller's spelling of non-link components.
fs::path real_path(fs::path const&, std::error_code&);

inline StringView view(std::string const& value)
{
    return { value.data(), value.size() };
}

inline std::string string(StringView value)
{
    return { value.characters_without_null_termination(), value.length() };
}

inline std::string path_string(fs::path const& path)
{
    auto preferred = path;
    auto utf8 = preferred.make_preferred().u8string();
    return { reinterpret_cast<char const*>(utf8.data()), utf8.size() };
}

inline fs::path path_from_utf8(std::string const& bytes)
{
    return fs::path(std::u8string(reinterpret_cast<char8_t const*>(bytes.data()), bytes.size()));
}

inline fs::path url_path(URL::URL const& url)
{
    auto path = url.file_path();
    return fs::path(std::u8string(reinterpret_cast<char8_t const*>(path.characters()), path.length()));
}

}
