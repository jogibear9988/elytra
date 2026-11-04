#pragma once

#include <AK/StringView.h>
#include <LibURL/URL.h>
#include <filesystem>
#include <string>

namespace Mite::ModulePaths {
namespace fs = std::filesystem;

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
    auto utf8 = path.generic_u8string();
    return { reinterpret_cast<char const*>(utf8.data()), utf8.size() };
}

inline fs::path url_path(URL::URL const& url)
{
    auto path = url.file_path();
    return fs::path(std::u8string(reinterpret_cast<char8_t const*>(path.characters()), path.length()));
}

}
