/** Owner-loop keyboard queries and passive layout-change polling. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;

namespace Keyboard {
    GC::Ref<JS::Object> create_host(JS::Realm&, EventLoop&);
}
}
