/** Explicit native diagnostics use fixed labels, never application data. */
#pragma once
#include <cstdlib>

namespace Firefly {
inline bool ipc_bytes_diagnostics_enabled()
{
    auto selected = [](char const* value) { return value && value[0] == '1' && value[1] == '\0'; };
    // The SDK deliberately removes arbitrary environment variables from helpers.
    // This owned name uses its existing LIBWEB_ diagnostic configuration prefix.
    return selected(std::getenv("FIREFLY_IPC_BYTES_DIAGNOSTIC"))
        || selected(std::getenv("LIBWEB_FIREFLY_IPC_BYTES_DIAGNOSTIC"));
}
}
