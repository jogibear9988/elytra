/** Validate the private fork channel without changing an embedder's CRT error policy. */
#pragma once
#include "platform/UV.h"
#ifdef _WIN32
#include <AK/Windows.h>
#include <cstdint>
#include <cstdlib>
#include <io.h>
#endif

namespace Mite {
#ifdef _WIN32
class ScopedDescriptorProbe final {
public:
    ScopedDescriptorProbe()
        : m_previous(_set_thread_local_invalid_parameter_handler(invalid_descriptor))
    {
    }

    ~ScopedDescriptorProbe()
    {
        _set_thread_local_invalid_parameter_handler(m_previous);
    }

    ScopedDescriptorProbe(ScopedDescriptorProbe const&) = delete;
    ScopedDescriptorProbe& operator=(ScopedDescriptorProbe const&) = delete;

private:
    // A forged role can name an absent CRT descriptor. The UCRT reports that
    // through its invalid-parameter handler before returning an invalid handle;
    // the default handler fast-fails instead of letting us reject the role.
    // Scope this behavior to the probing thread/call and restore the host's
    // previous handler, including on every early return below.
    static void __cdecl invalid_descriptor(
        wchar_t const*, wchar_t const*, wchar_t const*, unsigned, std::uintptr_t)
    {
    }

    _invalid_parameter_handler m_previous;
};
#endif

inline bool is_inherited_ipc_pipe(int descriptor)
{
#ifdef _WIN32
    ScopedDescriptorProbe guard;
    auto native = _get_osfhandle(descriptor);
    if (native == -1 || native == -2)
        return false;
    auto handle = reinterpret_cast<HANDLE>(native);
    DWORD flags {};
    if (!GetHandleInformation(handle, &flags) || GetFileType(handle) != FILE_TYPE_PIPE)
        return false;
#endif
    return uv_guess_handle(descriptor) == UV_NAMED_PIPE;
}
}
