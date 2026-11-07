/** Explicit host-owned signal subscriptions; JavaScript is entered only on the VM loop. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_signal_host(JS::Realm&, EventLoop&, bool allow);
}
