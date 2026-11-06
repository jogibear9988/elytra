/** Bind real AppKit-backed preferences and their owner-scoped change notifications. */
#include "../screen/ScreenHost.h"
#include "../theme/ThemeHost.h"
#include "DesktopServices.h"

namespace Firefly {
ErrorOr<void> verify_desktop_environment()
{
    return {};
}

void install_desktop_services(
    JS::Realm& realm, JS::Object& host, QObject& context, std::function<void(char const*)> notify)
{
    install_screen_host(realm, host);
    install_theme_host(realm, host, [notify] { notify("theme-changed"); });
    watch_screen_changes(context, [notify] { notify("screen-changed"); });
    watch_system_theme(context, [notify] { notify("theme-changed"); });
}
}
