/** Keep OS snapshots separate from VM objects and queue through the existing bridge owner. */
#include "ScreenHost.h"
#include "Screen.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QJsonDocument>

namespace Firefly {
void install_screen_host(JS::Realm& realm, JS::Object& host)
{
    for (auto const& [name, read] : { std::pair { "screenSnapshot"_utf16, &screen_snapshot },
             std::pair { "cursorScreenPoint"_utf16, &cursor_screen_point } }) {
        host.define_native_function(
            realm, name,
            [read](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto snapshot = read();
                if (snapshot.contains("error")) {
                    auto message = snapshot["error"].toObject()["message"].toString().toUtf8();
                    GC::Root error { JS::Error::create(vm.realm(),
                        Utf16String::from_utf8(StringView(message.constData(), message.size()))) };
                    error->define_direct_property("code"_utf16,
                        JS::PrimitiveString::create(vm, "ERR_FIREFLY_DISPLAY_UNAVAILABLE"_utf16),
                        JS::default_attributes);
                    return JS::throw_completion(error);
                }

                auto bytes = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
                return JS::PrimitiveString::create(
                    vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
            },
            0, JS::default_attributes);
    }
}

}
