/** Exit callbacks run while native services still exist, without an event-loop checkpoint. */
#include "Exit.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>

namespace Mite {
JS::ThrowCompletionOr<int> emit_process_exit(
    JS::VM& vm, JS::Object& process, ProcessExitState& state, int status, bool force_code)
{
    if (force_code)
        TRY(process.set(
            "exitCode"_utf16, JS::Value(status), JS::Object::ShouldThrowExceptions::Yes));
    if (!state.emitted) {
        // Mark before invocation: a listener's process.exit must terminate at once
        // rather than recursively emitting or running the remaining listeners.
        state.emitted = true;
        auto emit = TRY(process.get("emit"_utf16));
        if (!emit.is_undefined())
            TRY(JS::call(vm, emit, JS::Value(&process),
                JS::PrimitiveString::create(vm, "exit"_utf16), JS::Value(status)));
    }

    auto final = TRY(process.get("exitCode"_utf16));
    if (final.is_undefined())
        return 0;
    if (!final.is_int32())
        return vm.throw_completion<JS::TypeError>("Exit code must be an int32"_utf16);
    return final.as_i32();
}
}
