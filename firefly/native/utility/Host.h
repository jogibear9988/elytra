/** Firefly-owned utility children; Mite only supplies its native event loop. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
}

namespace Firefly::Utility {
GC::Ref<JS::Object> create_host(
    JS::VM&, JS::Realm&, Mite::EventLoop&, bool electron_compat = false);

JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_parent_host(
    JS::VM&, JS::Realm&, Mite::EventLoop&, int fd = 3);
}
