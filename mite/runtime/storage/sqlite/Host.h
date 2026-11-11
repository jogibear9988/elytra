/** Owner-loop callback admission for real worker-thread SQLite operations. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;

namespace SQLite {
    GC::Ref<JS::Object> create_host(JS::Realm&, EventLoop&);
}
}
