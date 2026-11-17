/** Borrowed process output with descriptor-specific native write semantics. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_stdio_host(JS::Realm&, EventLoop&);
// Process-ending paths only: restore borrowed status flags without closing
// anchors, flushing data, pumping a loop or invoking application callbacks.
void restore_stdio_before_process_exit();
}
