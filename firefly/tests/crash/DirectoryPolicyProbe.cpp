/** Actual native filesystem admission: no crash handler, desktop or external/user directory writes.
 */
#include "DirectoryPolicy.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
namespace fs = std::filesystem;
bool passed = true;

void check(bool condition, char const* message)
{
    if (!condition) {
        fprintf(stderr, "FAIL crash directory admission: %s\n", message);
        passed = false;
    }
}

std::string utf8(fs::path const& path)
{
    auto bytes = path.generic_u8string();
    return std::string(reinterpret_cast<char const*>(bytes.data()), bytes.size());
}

class Fixture {
public:
    Fixture()
    {
        std::error_code error;
        auto temporary = fs::temp_directory_path(error);
        if (error)
            return;
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt = 0; attempt < 32; ++attempt) {
            root = temporary
                / ("elytra-crash-directory-" + std::to_string(stamp) + "-"
                    + std::to_string(attempt));
            error.clear();
            if (fs::create_directory(root, error)) {
                owned = true;
                return;
            }

            if (error)
                return;
        }
    }

    ~Fixture()
    {
        if (owned) {
            std::error_code ignored;
            fs::remove_all(root, ignored);
        }
    }

    fs::path root;
    bool owned {};
};
}

int main()
{
    using Firefly::CrashReporting::validate_report_directory;
    Fixture fixture;
    if (!fixture.owned) {
        fprintf(stderr, "Cannot create unique owned fixture\n");
        return 1;
    }

    auto const& root = fixture.root;
    auto unicode = root / fs::path(u8"Grüße space#%-𝄞");
    std::error_code error;
    check(fs::create_directory(unicode, error) && !error, "create owned Unicode directory");
    auto file = root / "file.txt";
    {
        std::ofstream output(file);
        output << "owned fixture";
        check(output.good(), "create owned file");
    }

    check(!validate_report_directory(utf8(root)), "existing absolute directory admitted");
    check(!validate_report_directory(utf8(unicode)),
        "UTF-8 directory uses native Unicode path conversion");
    check(validate_report_directory(utf8(file)).default_error_condition()
            == std::errc::not_a_directory,
        "regular file refused");
    check(validate_report_directory(utf8(root / "missing")).default_error_condition()
            == std::errc::no_such_file_or_directory,
        "missing directory retains portable OS error");
    check(!fs::exists(root / "missing"), "admission never creates missing directories");
    for (auto value : { std::string(), std::string("relative"), std::string("../relative") })
        check(validate_report_directory(value) == std::errc::invalid_argument,
            "empty or relative path refused");
    // Keep each malformed case fully absolute on the running OS so rejection
    // demonstrates the byte/encoding rule rather than the independent path rule.
    auto prefix = utf8(root) + "/";
    for (auto suffix : { std::string("\0hidden", 7), std::string(32769, 'x'),
             std::string("\xc0\xaf", 2), std::string("\xed\xa0\x80", 3), std::string("\x80", 1),
             std::string("\xe2\x82", 2), std::string("\xf4\x90\x80\x80", 4) })
        check(validate_report_directory(prefix + suffix) == std::errc::invalid_argument,
            "absolute path encoding/NUL/budget refused before native query");
#ifdef _WIN32
    check(validate_report_directory("\\drive-relative") == std::errc::invalid_argument,
        "Windows root-relative path is not fully absolute");
    check(validate_report_directory(utf8(root.root_name()) + "relative")
            == std::errc::invalid_argument,
        "Windows drive-relative path refused");
#endif
    fs::create_directory_symlink(unicode, root / "directory-link", error);
    if (error) {
#ifdef _WIN32
        // Creating symlinks may require Windows Developer Mode or elevated token.
        // Report this separate privilege limitation rather than claim acceptance.
        auto privilege_denied = error == std::errc::permission_denied
            || error == std::errc::operation_not_permitted
            || (error.category() == std::system_category() && error.value() == 1314);
        check(privilege_denied, "unexpected Windows symlink failure");
        if (privilege_denied)
            fprintf(stderr, "SKIP Windows symlink privilege unavailable: %s\n",
                error.message().c_str());
#else
        check(false, "create owned directory symlink");
#endif
    } else {
        check(!validate_report_directory(utf8(root / "directory-link")),
            "directory symlink follows existing stat policy");
        fs::create_symlink(file, root / "file-link", error);
        check(!error
                && validate_report_directory(utf8(root / "file-link")).default_error_condition()
                    == std::errc::not_a_directory,
            "symlink to file refused");
        fs::create_directory_symlink(root / "missing", root / "dangling-link", error);
        check(!error
                && validate_report_directory(utf8(root / "dangling-link")).default_error_condition()
                    == std::errc::no_such_file_or_directory,
            "dangling directory symlink refused");
    }

    check(fs::is_directory(unicode) && fs::is_regular_file(file),
        "admission does not mutate owned targets");
    if (!passed)
        return 1;
    puts("PASS actual portable crash-directory "
         "absolute/existence/Unicode/type/symlink/encoding/budget admission; no crash capture or "
         "desktop started");
    return 0;
}
