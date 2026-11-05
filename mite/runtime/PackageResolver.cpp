#include "ModuleLoader.h"
#include "ModulePaths.h"
#include "BuiltinNames.h"

#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibURL/Parser.h>
#include <algorithm>
#include <charconv>
#include <limits>
#include <string_view>
#include <vector>

namespace Mite {
namespace fs = std::filesystem;
using namespace ModulePaths;

static bool invalid_segments(std::string const& path)
{
    size_t begin = 0;
    while (begin <= path.size()) {
        auto end = path.find_first_of("/\\", begin);
        if (end == std::string::npos)
            end = path.size();
        auto decoded = string(URL::percent_decode(view(path.substr(begin, end - begin))));
        for (char& ch : decoded) {
            if (ch >= 'A' && ch <= 'Z')
                ch += 'a' - 'A';
        }
        if (decoded.empty() || decoded == "." || decoded == ".." || decoded == "node_modules")
            return true;
        if (end == path.size())
            break;
        begin = end + 1;
    }
    return false;
}

static std::string substitute(std::string target, Optional<std::string> const& pattern)
{
    if (!pattern.has_value())
        return target;
    size_t offset = 0;
    while ((offset = target.find('*', offset)) != std::string::npos) {
        target.replace(offset, 1, *pattern);
        offset += pattern->size();
    }
    return target;
}

static bool index_key(std::string const& key)
{
    if (key.empty() || (key.size() > 1 && key[0] == '0'))
        return false;
    uint32_t value = 0;
    auto parsed = std::from_chars(key.data(), key.data() + key.size(), value);
    return parsed.ec == std::errc() && parsed.ptr == key.data() + key.size()
        && value < std::numeric_limits<uint32_t>::max();
}

JS::ThrowCompletionOr<Optional<fs::path>> ModuleLoader::package_scope(fs::path const& filename)
{
    auto directory = filename.parent_path();
    while (!directory.empty() && directory.filename() != "node_modules") {
        if (TRY(read_package(directory)).has_value())
            return Optional<fs::path> { directory };
        auto parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }
    return Optional<fs::path> {};
}

JS::ThrowCompletionOr<ModuleLoader::PackageTarget> ModuleLoader::resolve_target(fs::path const& root,
    AK::JsonValue const& value, Optional<std::string> const& pattern, bool imports, bool require, size_t depth)
{
    using State = PackageTarget::State;
    if (depth > 64)
        return error("ERR_MITE_PACKAGE_RESOLUTION_LIMIT", "Package target nesting/alias limit exceeded");
    if (value.is_null())
        return PackageTarget { State::Blocked, {} };
    if (value.is_string()) {
        auto target = string(value.as_string());
        if (!target.starts_with("./")) {
            if (!imports || target.starts_with("../") || target.starts_with('/'))
                return error("ERR_INVALID_PACKAGE_TARGET", "Package target must start with './': " + target);
            if (target.starts_with("node:")) {
                auto url = URL::Parser::basic_parse(view(target));
                if (url.has_value())
                    return PackageTarget { State::Resolved, url.release_value() };
            }
            if (URL::Parser::basic_parse(view(target)).has_value())
                return error("ERR_INVALID_PACKAGE_TARGET", "Invalid external package target: " + target);
            auto resolved = TRY(resolve_package(substitute(target, pattern), root / "package.json", require, depth + 1));
            return PackageTarget { State::Resolved, std::move(resolved) };
        }
        if (invalid_segments(target.substr(2)))
            return error("ERR_INVALID_PACKAGE_TARGET", "Invalid path segments in package target: " + target);
        if (pattern.has_value() && invalid_segments(*pattern))
            return error("ERR_INVALID_MODULE_SPECIFIER", "Invalid path segments in package subpath", true);
        auto base = path_to_url(root / "package.json");
        auto resolved = URL::Parser::basic_parse(view(substitute(target, pattern)), base);
        if (!resolved.has_value())
            return error("ERR_INVALID_PACKAGE_TARGET", "Invalid package target URL: " + target);
        return PackageTarget { State::Resolved, resolved.release_value() };
    }
    if (value.is_array()) {
        Optional<JS::Completion> last_error;
        for (auto const& item : value.as_array().values()) {
            auto attempt = resolve_target(root, item, pattern, imports, require, depth + 1);
            if (attempt.is_error()) {
                auto exception = attempt.error_value();
                auto code = exception.is_object() ? exception.as_object().get_without_side_effects("code"_utf16) : JS::js_undefined();
                if (!code.is_string() || code.as_string().utf16_string_view() != "ERR_INVALID_PACKAGE_TARGET"_utf16)
                    return attempt.release_error();
                last_error = attempt.release_error();
                continue;
            }
            auto result = attempt.release_value();
            if (result.state == State::Unmatched)
                continue;
            if (result.state == State::Blocked) {
                last_error.clear();
                continue;
            }
            return result;
        }
        if (last_error.has_value())
            return last_error.release_value();
        return PackageTarget { State::Blocked, {} };
    }
    if (value.is_object()) {
        std::vector<std::pair<std::string, AK::JsonValue const*>> conditions;
        value.as_object().for_each_member([&](auto const& key, auto const& target) { conditions.emplace_back(string(key), &target); });
        for (auto const& entry : conditions) {
            if (index_key(entry.first))
                return error("ERR_INVALID_PACKAGE_CONFIG", "Package condition objects cannot contain numeric keys");
        }
        for (auto const& [condition, target] : conditions) {
            if (condition != "default" && condition != "node" && condition != "node-addons"
                && condition != (require ? "require" : "import"))
                continue;
            auto result = TRY(resolve_target(root, *target, pattern, imports, require, depth + 1));
            if (result.state != State::Unmatched)
                return result;
        }
        return PackageTarget {};
    }
    return error("ERR_INVALID_PACKAGE_TARGET", "Package targets must be strings, objects, arrays or null");
}

JS::ThrowCompletionOr<ModuleLoader::PackageTarget> ModuleLoader::resolve_map(fs::path const& root,
    AK::JsonObject const& map, std::string const& match, bool imports, bool require, size_t depth)
{
    if (match.ends_with('/'))
        return error("ERR_INVALID_MODULE_SPECIFIER", "Package subpaths must not end with '/'", true);
    if (match.find('*') == std::string::npos) {
        auto direct = map.get(view(match));
        if (direct.has_value())
            return resolve_target(root, *direct, {}, imports, require, depth + 1);
    }
    std::vector<std::pair<std::string, AK::JsonValue const*>> patterns;
    map.for_each_member([&](auto const& key, auto const& target) {
        auto pattern = string(key);
        auto star = pattern.find('*');
        if (star != std::string::npos && star == pattern.rfind('*'))
            patterns.emplace_back(std::move(pattern), &target);
    });
    std::stable_sort(patterns.begin(), patterns.end(), [](auto const& a, auto const& b) {
        auto prefix_a = a.first.find('*');
        auto prefix_b = b.first.find('*');
        return prefix_a == prefix_b ? a.first.size() > b.first.size() : prefix_a > prefix_b;
    });
    for (auto const& [key, target] : patterns) {
        auto star = key.find('*');
        auto prefix = key.substr(0, star);
        auto suffix = key.substr(star + 1);
        if (match.starts_with(prefix) && match.ends_with(suffix) && match.size() >= key.size()) {
            auto pattern = match.substr(star, match.size() - star - suffix.size());
            return resolve_target(root, *target, pattern, imports, require, depth + 1);
        }
    }
    return PackageTarget {};
}

JS::ThrowCompletionOr<URL::URL> ModuleLoader::resolve_exports(fs::path const& root,
    std::string const& subpath, AK::JsonValue const& exports, bool require, size_t depth)
{
    bool subpaths = false;
    bool conditions = false;
    if (exports.is_object()) {
        exports.as_object().for_each_member([&](auto const& key, auto const&) {
            if (key.starts_with('.'))
                subpaths = true;
            else
                conditions = true;
        });
    }
    if (subpaths && conditions)
        return error("ERR_INVALID_PACKAGE_CONFIG", "Package exports cannot mix subpaths and condition keys");
    PackageTarget result;
    if (subpath == "." && !subpaths) {
        result = TRY(resolve_target(root, exports, {}, false, require, depth + 1));
    } else if (exports.is_object() && subpaths) {
        result = TRY(resolve_map(root, exports.as_object(), subpath, false, require, depth + 1));
    }
    if (result.state != PackageTarget::State::Resolved)
        return error("ERR_PACKAGE_PATH_NOT_EXPORTED", "Package subpath is not exported: " + subpath);
    return result.url.release_value();
}

JS::ThrowCompletionOr<URL::URL> ModuleLoader::resolve_package(std::string const& request,
    fs::path const& parent_file, bool require, size_t depth)
{
    if (depth > 64)
        return error("ERR_MITE_PACKAGE_RESOLUTION_LIMIT", "Package target nesting/alias limit exceeded");
    if (m_host_modules.contains(request))
        return URL::Parser::basic_parse(view("mite-host:" + request)).release_value();
    // Reserve documented built-ins even before their implementations land;
    // a package of the same name must not impersonate an unsupported host API.
    if (request.starts_with("node:") || bare_builtin(request)) {
        auto url = URL::Parser::basic_parse(view(request.starts_with("node:") ? request : "node:" + request));
        VERIFY(url.has_value());
        return url.release_value();
    }
    auto scope = TRY(package_scope(parent_file));
    if (request.starts_with('#')) {
        if (require) {
            if (!scope.has_value())
                return error("MODULE_NOT_FOUND", "Cannot find module '" + request + "'");
            auto package = TRY(read_package(*scope));
            auto imports = package->as_object().get("imports"sv);
            if (!imports.has_value() || imports->is_null())
                return error("MODULE_NOT_FOUND", "Cannot find module '" + request + "'");
        }
        if (request == "#")
            return error("ERR_INVALID_MODULE_SPECIFIER", "Invalid package import specifier: " + request, true);
        if (scope.has_value()) {
            auto package = TRY(read_package(*scope));
            auto imports = package->as_object().get_object("imports"sv);
            if (imports.has_value()) {
                auto result = TRY(resolve_map(*scope, *imports, request, true, require, depth + 1));
                if (result.state == PackageTarget::State::Resolved)
                    return result.url.release_value();
            }
        }
        return error("ERR_PACKAGE_IMPORT_NOT_DEFINED", "Package import is not defined: " + request, true);
    }
    if (request.empty())
        return error("ERR_INVALID_MODULE_SPECIFIER", "Package name must not be empty", true);
    auto name_end = request.find('/');
    if (request.starts_with('@')) {
        if (name_end == std::string::npos || name_end == 1 || name_end + 1 == request.size())
            return error("ERR_INVALID_MODULE_SPECIFIER", "Invalid scoped package name: " + request, true);
        name_end = request.find('/', name_end + 1);
    }
    auto name = request.substr(0, name_end);
    if (name.starts_with('.') || name.find_first_of("\\%") != std::string::npos)
        return error("ERR_INVALID_MODULE_SPECIFIER", "Invalid package name: " + name, true);
    auto subpath = name_end == std::string::npos ? std::string(".") : "." + request.substr(name_end);
    if (scope.has_value()) {
        auto package = TRY(read_package(*scope));
        auto self = package->as_object().get_string("name"sv);
        auto exports = package->as_object().get("exports"sv);
        if (self.has_value() && string(*self) == name && exports.has_value() && (exports->is_string() || exports->is_object() || exports->is_array()))
            return resolve_exports(*scope, subpath, *exports, require, depth + 1);
    }
    auto directory = parent_file.parent_path();
    while (!directory.empty()) {
        if (directory.filename() != "node_modules") {
            auto utf8_name = std::u8string(reinterpret_cast<char8_t const*>(name.data()), name.size());
            auto root = directory / "node_modules" / fs::path(utf8_name);
            std::error_code ec;
            if (fs::is_directory(root, ec)) {
                auto package = TRY(read_package(root));
                if (package.has_value()) {
                    auto exports = package->as_object().get("exports"sv);
                    if (exports.has_value() && (exports->is_string() || exports->is_object() || exports->is_array()))
                        return resolve_exports(root, subpath, *exports, require, depth + 1);
                }
                if (subpath == ".") {
                    auto found = TRY(legacy_path(root));
                    if (found.has_value())
                        return path_to_url(*found);
                    return error(require ? "MODULE_NOT_FOUND" : "ERR_MODULE_NOT_FOUND", "Cannot find package entry: " + name);
                }
                auto relative = subpath.substr(2);
                if (require) {
                    auto path = root / fs::path(std::u8string(reinterpret_cast<char8_t const*>(relative.data()), relative.size()));
                    auto found = TRY(legacy_path(path));
                    if (found.has_value())
                        return path_to_url(*found);
                    return error("MODULE_NOT_FOUND", "Cannot find package subpath: " + request);
                }
                auto resolved = URL::Parser::basic_parse(view(subpath), path_to_url(root / "package.json"));
                if (!resolved.has_value())
                    return error("ERR_INVALID_MODULE_SPECIFIER", "Invalid package subpath: " + request, true);
                return resolved.release_value();
            }
        }
        auto parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }
    return error(require ? "MODULE_NOT_FOUND" : "ERR_MODULE_NOT_FOUND", "Cannot find package: " + name);
}

}
