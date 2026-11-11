/** No V8 dependency, debugger socket, session or synthetic inspector endpoint. */
#include "Inspector.h"
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Mite {
GC::Ref<JS::Object> create_inspector_host(JS::Realm& realm)
{
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(
        realm, "url"_utf16,
        +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            // The runtime installs no inspector backend. Reporting absence is
            // distinct from pretending that opening or attaching one succeeds.
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    return host;
}
}
