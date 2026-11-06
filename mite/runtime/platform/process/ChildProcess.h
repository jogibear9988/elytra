/**
 * Owns spawned processes, native pipes and pending exit/close notifications. Native callbacks
 * return through the runtime loop; argument/environment snapshots cannot retain mutable JS
 * storage.
 */

#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_child_process_host(JS::VM&, JS::Realm&, EventLoop&);
}
