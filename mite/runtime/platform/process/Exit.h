/** Shared once-only synchronous exit notification for explicit and orderly termination. */
#pragma once
#include <LibJS/Runtime/Completion.h>
#include <optional>

namespace JS {
class VM;
class Object;
}

namespace Mite {
struct ProcessExitState {
    GC_HOLDS_NO_CELL_EDGES;

    bool emitted {};
    bool worker {};
    std::optional<int> requested_code;
};

JS::ThrowCompletionOr<int> emit_process_exit(
    JS::VM&, JS::Object&, ProcessExitState&, int status, bool force_code);
}
