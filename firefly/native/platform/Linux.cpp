/** Experimental native X11 runtime; missing display/preferences APIs fail at their VM boundary. */
#include "DesktopServices.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QString>
#include <QtGlobal>

namespace Firefly {
ErrorOr<void> verify_desktop_environment()
{
    if (qEnvironmentVariable("QT_QPA_PLATFORM") != "xcb" || qEnvironmentVariableIsEmpty("DISPLAY"))
        return Error::from_string_literal(
            "Experimental Linux Firefly requires QT_QPA_PLATFORM=xcb and a native X11 DISPLAY");
    return {};
}

void install_desktop_services(
    JS::Realm& realm, JS::Object& host, QObject&, std::function<void(char const*)>)
{
    host.define_direct_property("themeAvailable"_utf16, JS::Value(false), JS::default_attributes);
    // Do not fabricate screen IDs, accessibility values or notification timers.
    // Windows, clipboard and IPC can function independently of these unfinished services.
    for (auto name : { "screenSnapshot"_utf16, "cursorScreenPoint"_utf16, "themeSnapshot"_utf16,
             "themeSetSource"_utf16 }) {
        host.define_native_function(
            realm, name,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                GC::Root error { JS::Error::create(vm.realm(),
                    Utf16String::from_utf8(
                        "Linux display/system-preference services are not implemented"sv)) };
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, "ERR_FIREFLY_UNSUPPORTED_API"_utf16),
                    JS::default_attributes);
                return JS::throw_completion(error);
            },
            0, JS::default_attributes);
    }
}
}
