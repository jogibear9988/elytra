/** Real OS pseudoterminals owned by one Mite VM/event loop. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_pty_host(JS::Realm&, EventLoop&);
}
