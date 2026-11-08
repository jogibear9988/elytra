/** Read-only crash-directory admission, independent of crash capture and GUI services. */
#pragma once
#include <string_view>
#include <system_error>

namespace Firefly::CrashReporting {
// Requires a fully absolute UTF-8 path to an existing directory. Follows directory
// symlinks, matching stat; ownership/private permissions remain the report store's
// responsibility when it opens its descriptor. Does not create or canonicalize.
std::error_code validate_report_directory(std::string_view utf8_path);
}
