/** Process-wide native measurements. No retained JS state, timers or event-loop handles. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Mite {
void install_process_timing(JS::Realm&, JS::Object& host);

void install_performance_timing(JS::Realm&, JS::Object& host);
}
