/** Diagnostic-only physical helper selection; never an application runtime option. */
#pragma once
#include <AK/Array.h>
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Optional.h>
#include <AK/Utf8View.h>
#include <AK/Vector.h>
#include <LibMain/Main.h>
#include <LibWebCommon/WebView/Utilities.h>
#include <filesystem>
#include <string_view>
#ifndef AK_OS_WINDOWS
#include <unistd.h>
#endif

namespace Firefly::Diagnostics {
inline constexpr auto helper_names = Array {
    "AudioServer"sv,
    "Compositor"sv,
    "Hub"sv,
    "ImageDecoder"sv,
    "MediaServer"sv,
    "RequestServer"sv,
    "VideoDecoderServer"sv,
    "WebContent"sv,
    "WebRTCClient"sv,
#if defined(AK_OS_MACOS)
    "ProcessReaper"sv,
#endif
};

inline std::filesystem::path path(StringView text)
{
    return std::filesystem::path(std::u8string_view(
        reinterpret_cast<char8_t const*>(text.characters_without_null_termination()),
        text.length()));
}

inline ByteString utf8_path(std::filesystem::path const& path)
{
    auto value = path.generic_u8string();

    return ByteString(reinterpret_cast<char const*>(value.data()), value.size());
}

inline ErrorOr<void> physical(std::filesystem::path const& path, bool directory)
{
    std::error_code error;
    auto status = std::filesystem::symlink_status(path, error);

    if (error || std::filesystem::is_symlink(status)
        || (directory ? !std::filesystem::is_directory(status)
                      : !std::filesystem::is_regular_file(status)))
        return Error::from_string_literal("Helper directory requires physical files/directories");

    return {};
}

inline ErrorOr<std::filesystem::path> validate_directory(StringView value)
{
    if (value.is_empty() || value.length() > 32768 || value.contains('\0')
        || !Utf8View(value).validate())
        return Error::from_string_literal("Invalid helper directory text");

    auto directory = path(value);

    if (!directory.is_absolute() || directory != directory.lexically_normal())
        return Error::from_string_literal("Helper directory must be an absolute normalized path");

    TRY(physical(directory, true));

    std::error_code error;
    auto canonical = std::filesystem::canonical(directory, error);

    if (error || canonical != directory)
        return Error::from_string_literal("Helper directory must have no symlink ancestors");

    for (auto name : helper_names) {
        auto file = directory / path(name);
#if defined(AK_OS_WINDOWS)
        file += ".exe";
#endif
        TRY(physical(file, false));
#ifndef AK_OS_WINDOWS
        if (access(file.c_str(), X_OK) != 0)
            return Error::from_string_literal("Selected helper is not executable");
#endif
    }

    return directory;
}

struct HelperDirectoryOption {
    Optional<ByteString> selected;
    Vector<StringView> arguments;

    ErrorOr<void> parse(Main::Arguments const& original)
    {
        arguments.append(original.strings.first());

        for (size_t index = 1; index < original.strings.size(); ++index) {
            auto argument = original.strings[index];
            Optional<StringView> value;
            if (argument == "--helper-directory"sv) {
                if (++index == original.strings.size())
                    return Error::from_string_literal("Missing helper directory argument");

                value = original.strings[index];
            } else if (argument.starts_with("--helper-directory="sv)) {
                value = argument.substring_view(19);
            } else {
                arguments.append(argument);

                continue;
            }

            if (selected.has_value())
                return Error::from_string_literal("Duplicate helper directory argument");

            selected = ByteString(*value);
        }

        if (selected.has_value())
            selected = utf8_path(TRY(validate_directory(*selected)));

        return {};
    }
};

// Call after the base constructor selected platform paths, but BEFORE initialize
// can launch a helper. Existing alternate lookup candidates may not escape it.
inline ErrorOr<void> verify_selected_paths(ByteString const& selected)
{
    auto directory = TRY(validate_directory(selected));
#if defined(AK_OS_MACOS)
    auto resources = directory.parent_path() / "Resources";
#else
    auto resources = directory.parent_path() / "share/Lagom";
#endif
    TRY(physical(resources, true));

    TRY(physical(resources / "themes/Default.ini", false));

    std::error_code error;
    if (std::filesystem::canonical(path(WebView::s_ladybird_resource_root), error) != resources
        || error)
        return Error::from_string_literal("Selected helper resources escaped the physical package");

    for (auto name : helper_names) {
        auto wanted = directory / path(name);
#if defined(AK_OS_WINDOWS)
        wanted += ".exe";
#endif
        for (auto const& candidate : TRY(WebView::get_paths_for_helper_process(name))) {
            auto file = path(candidate);

            if (std::filesystem::exists(file, error)
                && (std::filesystem::canonical(file, error) != wanted || error))
                return Error::from_string_literal(
                    "Helper lookup has a different existing fallback");
        }
    }

    return {};
}
}
