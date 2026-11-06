// Native bring-up diagnostics only; the shipping CLI has no crash interception.
#include "core/Runtime.h"
#include <AK/Windows.h>
// AK deliberately removes the legacy Windows IN annotation. DbgHelp still
// requires it in some SDK declarations; restore it only while parsing that SDK.
#pragma push_macro("IN")
#ifndef IN
#define IN
#endif
#include <DbgHelp.h>
#pragma pop_macro("IN")
#include <cstdio>
#include <cstring>

#if !defined(_M_X64)
#error This diagnostic probe currently requires native Windows x64
#endif

namespace {
LONG WINAPI report_exception(EXCEPTION_POINTERS* exception)
{
    fprintf(stderr, "MITE_NATIVE_EXCEPTION code=0x%08lx address=%p\n",
        exception->ExceptionRecord->ExceptionCode, exception->ExceptionRecord->ExceptionAddress);
    auto process = GetCurrentProcess();
    auto context = *exception->ContextRecord;
    if (exception->ExceptionRecord->NumberParameters >= 2) {
        fprintf(stderr, "MITE_NATIVE_ACCESS operation=%llu address=0x%llx\n",
            static_cast<unsigned long long>(exception->ExceptionRecord->ExceptionInformation[0]),
            static_cast<unsigned long long>(exception->ExceptionRecord->ExceptionInformation[1]));
    }

    if (!context.Rip) {
        // An indirect call to zero leaves a return address at RSP. A tail jump
        // need not, so label this as a candidate until the call site is reviewed.
        // Read safely and require a loaded module; never dereference fault memory.
        DWORD64 candidate = 0;
        SIZE_T bytes_read = 0;
        if (ReadProcessMemory(process, reinterpret_cast<void const*>(context.Rsp), &candidate,
                sizeof(candidate), &bytes_read)
            && bytes_read == sizeof(candidate) && candidate
            && SymGetModuleBase64(process, candidate)) {
            fprintf(stderr, "MITE_NATIVE_NULL_PC_CALLER_CANDIDATE 0x%llx\n",
                static_cast<unsigned long long>(candidate));
            // Only this local unwind copy changes; execution is never resumed.
            context.Rip = candidate;
            context.Rsp += sizeof(candidate);
        }
    }

    STACKFRAME64 frame {};
    frame.AddrPC = { context.Rip, 0, AddrModeFlat };
    frame.AddrFrame = { context.Rbp, 0, AddrModeFlat };
    frame.AddrStack = { context.Rsp, 0, AddrModeFlat };
    for (unsigned i = 0; i < 64 && frame.AddrPC.Offset; ++i) {
        alignas(SYMBOL_INFO) unsigned char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        auto resolved = SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol);
        fprintf(stderr, "MITE_NATIVE_FRAME %u 0x%llx %s+0x%llx\n", i,
            static_cast<unsigned long long>(frame.AddrPC.Offset),
            resolved ? symbol->Name : "unresolved", static_cast<unsigned long long>(displacement));
        IMAGEHLP_LINE64 line {};
        line.SizeOfStruct = sizeof(line);
        DWORD line_displacement = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &line_displacement, &line))
            fprintf(stderr, "MITE_NATIVE_SOURCE %s:%lu\n", line.FileName, line.LineNumber);
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context,
                nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
            break;
    }

    fflush(stderr);
    // Keep the actual failing exception status; this is diagnostic output, not recovery.
    return EXCEPTION_EXECUTE_HANDLER;
}
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE)) {
        fprintf(stderr, "Native symbol initialization failed: %lu\n", GetLastError());
        return 2;
    }

    SetUnhandledExceptionFilter(report_exception);
    if (argc == 2 && strcmp(argv[1], "--diagnostic-crash") == 0)
        RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    return Mite::run(argc, argv);
}
