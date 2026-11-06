# ADR 0058: null-safe execution-context deallocation

Status: implemented and tested with the local macOS C++ size profile. Other native
platforms and the size-profile renderer acceptance remain pending.

## Evidence

Apple Clang's -Oz build reproducibly aborts at startup on Ladybird
97ad425b5125f20c516e1a8d3b5bb02f391fc975. Rust reports that the embedder passed a
null execution context. Disassembly of initialize_host_defined_realm shows
unconditional deallocation calls for moved-from NonnullOwnPtr owners. The C++
ExecutionContext::operator delete forwards the pointer directly to Rust, whose
js_execution_context_destroy requires a non-null context.

Calling a deallocation function for a null delete operand is permitted by the
[C++ delete-expression rules](https://eel.is/c++draft/expr.delete#6). Relying on
the -O3 optimizer eliminating those calls is not a stable C++/Rust boundary.
This finding does not by itself diagnose the separate Windows access violation.

## Decision

Keep Ladybird unmodified. Our LibJSObjects graph substitutes one owned compile
adapter for ExecutionContext.cpp. The adapter includes the original complete
translation unit, routing only its js_execution_context_destroy call through a
null-checking wrapper. Non-null contexts retain the exact upstream destruction
path. No Rust ABI, object layout, allocation or other execution-context operation
changes. The original source/license remains in the pinned upstream checkout.

A configure/build guard checks the exact operator-delete body and exactly one
destroy call. Source-list replacement must find exactly one original TU. An
upstream fix or changed seam requires review and may allow removing this adapter.
The same adapter is installed in standalone Mite and the renderer's shared LibJS
objects; there is no Mite dependency on LibWeb/Firefly.

## Validation

Before adaptation, the size-profile CLI aborts even for console.log(42), and the
runtime suites fail with the execution-context panic. The new SDK probe directly
exercises null deallocation, real VM/realm construction, ownership movement and
cleanup. With the adapter, all 38 local Mite CTest suites pass under -Oz (26.26s).
The exact generated command guard checks -Oz follows upstream -O3. The original
failure and disassembly are retained in build/size-mite-*-trace.log and
build/size-realm-disassembly.log; corrected results are in
build/size-mite-adapter-{build,test}.log.

The local LLDB launch did not produce a debugger observation; its owned debugger
processes were terminated. Diagnosis uses the reproducible panic, source contract,
disassembly and passing native regression, not an invented debugger stack trace.
Renderer/native Windows/Linux acceptance of this new adapter remains independent.
