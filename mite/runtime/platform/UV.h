/** Keep upstream's Windows timeval and Winsock declarations at one libuv boundary. */
#pragma once
#include <AK/Time.h>

#ifdef AK_OS_WINDOWS
// AK/Time.h owns timeval on Windows. Use the same Winsock-only name as
// AK/Windows.h so both boundaries agree, without importing that cpp-only
// header (and its native SystemApi initializer) into public runtime headers.
#pragma push_macro("timeval")
#define timeval dummy_timeval
#include <uv.h>
#pragma pop_macro("timeval")
#else
#include <uv.h>
#endif
