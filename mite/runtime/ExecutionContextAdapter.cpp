// Preserve the upstream translation unit, routing only its deallocation call.
// C++ may call class-specific operator delete with nullptr for a null operand.
#include <LibJS/Embedding/ABI.h>

static void mite_checked_execution_context_destroy(JSExecutionContext* context)
{
    if (context)
        js_execution_context_destroy(context);
}

#define js_execution_context_destroy mite_checked_execution_context_destroy
#include <LibJS/Runtime/ExecutionContext.cpp>
#undef js_execution_context_destroy
