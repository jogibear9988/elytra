/** Shared once-only synchronous exit notification for explicit and orderly termination. */
#pragma once
#include <LibJS/Runtime/Completion.h>

namespace JS {
class VM;
class Object;
}

namespace Mite {
struct ProcessExitState {
    bool emitted {};
};

JS::ThrowCompletionOr<int> emit_process_exit(
    JS::VM&, JS::Object&, ProcessExitState&, int status, bool force_code);
}
