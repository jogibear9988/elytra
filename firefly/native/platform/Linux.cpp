/** Bind native X11 display snapshots and owner-scoped changes; preferences still fail explicitly.
 */
#include "../screen/ScreenHost.h"
#include "DesktopServices.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly {
void install_desktop_services(
    JS::Realm& realm, JS::Object& host, QObject& context, std::function<void(char const*)> notify)
{
    host.define_direct_property("themeAvailable"_utf16, JS::Value(false), JS::default_attributes);
    install_screen_host(realm, host);
    watch_screen_changes(context, [notify] { notify("screen-changed"); });
    // Theme/accessibility preferences remain unavailable; do not return invented booleans.
    for (auto name : { "themeSnapshot"_utf16, "themeSetSource"_utf16 }) {
        host.define_native_function(
            realm, name,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                GC::Root error { JS::Error::create(vm.realm(),
                    Utf16String::from_utf8(
                        "Linux system-preference services are not implemented"sv)) };
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, "ERR_FIREFLY_UNSUPPORTED_API"_utf16),
                    JS::default_attributes);
                return JS::throw_completion(error);
            },
            0, JS::default_attributes);
    }
}
}
