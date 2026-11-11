/** Independent worker VM threads communicate only through bounded owned bytes. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_worker_host(JS::VM&, JS::Realm&, EventLoop&);
}
