/** Portable native path admission without POSIX-only stat or platform-code/errno confusion. */
#include "DirectoryPolicy.h"
#include <cstdint>
#include <filesystem>

namespace Firefly::CrashReporting {
namespace {
    bool valid_utf8(std::string_view bytes)
    {
        for (size_t index = 0; index < bytes.size();) {
            auto first = static_cast<uint8_t>(bytes[index++]);
            if (!first)
                return false;
            if (first < 0x80)
                continue;
            unsigned remaining;
            uint32_t point;
            uint32_t minimum;
            if (first >= 0xc2 && first <= 0xdf) {
                remaining = 1;
                point = first & 0x1f;
                minimum = 0x80;
            } else if (first >= 0xe0 && first <= 0xef) {
                remaining = 2;
                point = first & 0x0f;
                minimum = 0x800;
            } else if (first >= 0xf0 && first <= 0xf4) {
                remaining = 3;
                point = first & 7;
                minimum = 0x10000;
            } else
                return false;
            if (remaining > bytes.size() - index)
                return false;
            while (remaining--) {
                auto next = static_cast<uint8_t>(bytes[index++]);
                if ((next & 0xc0) != 0x80)
                    return false;
                point = (point << 6) | (next & 0x3f);
            }

            if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff))
                return false;
        }

        return true;
    }
}

std::error_code validate_report_directory(std::string_view path)
{
    // Validate before Windows' UTF-8 -> native-wide path conversion. In
    // particular, malformed UTF-8 must not throw through an exceptions-free host.
    if (path.empty() || path.size() > 32768 || !valid_utf8(path))
        return std::make_error_code(std::errc::invalid_argument);
    auto bytes = std::u8string_view(reinterpret_cast<char8_t const*>(path.data()), path.size());
    std::filesystem::path native(bytes);
    // Native Windows semantics require both root name and root directory, so
    // C:relative and drive-relative \directory never pass as fully absolute.
    if (!native.is_absolute())
        return std::make_error_code(std::errc::invalid_argument);
    std::error_code error;
    auto status = std::filesystem::status(native, error);
    if (error)
        return error;
    if (status.type() == std::filesystem::file_type::not_found)
        return std::make_error_code(std::errc::no_such_file_or_directory);
    if (!std::filesystem::is_directory(status))
        return std::make_error_code(std::errc::not_a_directory);
    return {};
}
}
