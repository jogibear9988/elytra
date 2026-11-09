/** Native helper-only permission policy. No page/preload setter or VM callback is exposed. */
#pragma once
#include <AK/Optional.h>
#include <LibWeb/Export.h>
#include <LibWeb/PermissionsAPI/Permissions.h>

namespace Web::DOM {
class Document;
}

namespace Firefly::RendererPermissions {
struct Policy {
    u64 epoch {};
    bool check_enabled {};
    bool request_enabled {};
};

using Query = bool (*)(Web::DOM::Document&, u64 epoch, StringView permission);

WEB_API void enable();

WEB_API void set_query(Query);

WEB_API bool active();

WEB_API Policy policy();

WEB_API bool apply_policy(u64 epoch, bool check_enabled, bool request_enabled);

WEB_API Optional<Web::PermissionsAPI::PermissionState> query(
    Web::PermissionsAPI::PermissionDescriptor const&, Web::HTML::EnvironmentSettingsObject&);

WEB_API Web::PermissionsAPI::PermissionState request_state(
    Web::PermissionsAPI::PermissionDescriptor const&, Web::HTML::EnvironmentSettingsObject&);
}
