/** Expand real Windows short-name aliases before libuv computes relative watch filenames. */
#pragma once
#include "platform/UV.h"
#include <string>
#include <utility>

#ifdef _WIN32
#include <AK/Windows.h>
#endif

namespace Mite::WatcherSupport {
inline int expand_event_watch_path(std::string& path)
{
#ifdef _WIN32
    // libuv's Windows fs-event backend expands each changed filename through
    // GetLongPathNameW, then subtracts the watched directory's UTF-16 length.
    // A short alias (e.g. RUNNER~1) must therefore be expanded at admission too.
    // This preserves symlinks/junctions; filesystem realpath would change them.
    auto count = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, path.data(), static_cast<int>(path.size()), nullptr, 0);

    if (!count)
        return uv_translate_sys_error(GetLastError());

    std::wstring wide(static_cast<size_t>(count), L'\0');

    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.data(),
            static_cast<int>(path.size()), wide.data(), count)
        != count)
        return uv_translate_sys_error(GetLastError());

    auto needed = GetLongPathNameW(wide.c_str(), nullptr, 0);

    if (!needed)
        return uv_translate_sys_error(GetLastError());

    if (needed > 1048577)
        return UV_ENAMETOOLONG;
    std::wstring expanded(needed, L'\0');

    auto length = GetLongPathNameW(wide.c_str(), expanded.data(), needed);

    if (!length)
        return uv_translate_sys_error(GetLastError());

    if (length >= needed)
        return UV_ENOBUFS; // The native name changed between the two bounded queries.
    auto bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, expanded.data(),
        static_cast<int>(length), nullptr, 0, nullptr, nullptr);

    if (!bytes)
        return uv_translate_sys_error(GetLastError());

    if (bytes > 1048576)
        return UV_ENAMETOOLONG;
    std::string result(static_cast<size_t>(bytes), '\0');

    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, expanded.data(),
            static_cast<int>(length), result.data(), bytes, nullptr, nullptr)
        != bytes)
        return uv_translate_sys_error(GetLastError());

    path = std::move(result);
#else
    (void)path;
#endif
    return 0;
}
}
