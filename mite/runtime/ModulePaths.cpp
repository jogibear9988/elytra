#include "ModulePaths.h"

#ifdef _WIN32
#    include <uv.h>
#    include <sys/stat.h>
#endif

namespace Mite::ModulePaths {

fs::path real_path(fs::path const& input, std::error_code& error)
{
#ifndef _WIN32
    return fs::canonical(input, error);
#else
    // GetFinalPathNameByHandle (used by filesystem::canonical) expands 8.3
    // aliases. Node's non-native realpath walks components instead. Use libuv's
    // UTF-8 lstat/readlink for symlinks AND directory junctions; do not merely
    // return an absolute path, which would give linked modules separate caches.
    auto pending = fs::absolute(input, error).lexically_normal();
    if (error)
        return {};
    auto fail = [&](int status) -> fs::path {
        auto code = std::errc::io_error;
        if (status == UV_ENOENT)
            code = std::errc::no_such_file_or_directory;
        else if (status == UV_ENOTDIR)
            code = std::errc::not_a_directory;
        else if (status == UV_EACCES || status == UV_EPERM)
            code = std::errc::permission_denied;
        else if (status == UV_ELOOP)
            code = std::errc::too_many_symbolic_link_levels;
        error = std::make_error_code(code);
        return {};
    };
    unsigned links = 0;
    for (;;) {
        auto resolved = pending.root_path();
        uv_fs_t request {};
        auto root = path_string(resolved);
        auto status = uv_fs_lstat(nullptr, &request, root.c_str(), nullptr);
        uv_fs_req_cleanup(&request);
        if (status < 0)
            return fail(status);
        bool restart = false;
        auto parts = pending.relative_path();
        for (auto part = parts.begin(); part != parts.end(); ++part) {
            auto candidate = resolved / *part;
            auto bytes = path_string(candidate);
            status = uv_fs_lstat(nullptr, &request, bytes.c_str(), nullptr);
            auto mode = request.statbuf.st_mode;
            uv_fs_req_cleanup(&request);
            if (status < 0)
                return fail(status);
            if ((mode & S_IFMT) != S_IFLNK) {
                resolved = std::move(candidate);
                continue;
            }
            // Validate the target first: broken links/cycles must fail, never
            // become a fresh identity or an unbounded component walk.
            status = uv_fs_stat(nullptr, &request, bytes.c_str(), nullptr);
            uv_fs_req_cleanup(&request);
            if (status < 0)
                return fail(status);
            if (++links > 64)
                return fail(UV_ELOOP);
            status = uv_fs_readlink(nullptr, &request, bytes.c_str(), nullptr);
            auto target = status < 0 ? fs::path {} : path_from_utf8(static_cast<char const*>(request.ptr));
            uv_fs_req_cleanup(&request);
            if (status < 0)
                return fail(status);
            auto replacement = target.is_absolute() ? target : resolved / target;
            while (++part != parts.end())
                replacement /= *part;
            pending = replacement.lexically_normal();
            restart = true;
            break;
        }
        if (!restart) {
            error.clear();
            return resolved;
        }
    }
#endif
}

}
