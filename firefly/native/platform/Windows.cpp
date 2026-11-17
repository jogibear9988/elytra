/** Native Windows desktop services; unsupported preference providers fail explicitly. */
#include "../screen/ScreenHost.h"
#include "DesktopServices.h"
#include "GCIncludes.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QGuiApplication>
#include <QThread>

namespace Firefly {
ErrorOr<void> verify_desktop_environment()
{
    // Called before QApplication construction: do not initialize a GUI or
    // silently select offscreen/X11 when the native Windows plugin is absent.
    auto platform = qEnvironmentVariable("QT_QPA_PLATFORM");
    if (!platform.isEmpty() && platform != QStringLiteral("windows"))
        return Error::from_string_literal("Firefly Windows requires the native windows Qt backend");
    return {};
}

void install_desktop_services(
    JS::Realm& realm, JS::Object& host, QObject& context, std::function<void(char const*)> notify)
{
    VERIFY(qGuiApp && QGuiApplication::platformName() == QStringLiteral("windows")
        && QThread::currentThread() == qGuiApp->thread());
    install_screen_host(realm, host);
    watch_screen_changes(context, [notify] { notify("screen-changed"); });
    host.define_direct_property("themeAvailable"_utf16, JS::Value(false), JS::default_attributes);
    for (auto name : { "themeSnapshot"_utf16, "themeSetSource"_utf16 }) {
        host.define_native_function(
            realm, name,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                GC::Root error { JS::Error::create(vm.realm(),
                    "Windows theme and accessibility preferences are not implemented"_utf16) };
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, "ERR_FIREFLY_UNSUPPORTED_API"_utf16),
                    JS::default_attributes);
                return JS::throw_completion(error);
            },
            0, JS::default_attributes);
    }
}
}
