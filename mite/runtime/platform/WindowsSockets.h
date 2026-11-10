/** One real WinSDK socket import shared by libuv, OpenSSL and upstream's native boundary. */
#pragma once

#ifdef _WIN32
#include <AK/Time.h>

// Upstream AK owns timeval on Windows. Match AK/Windows.h's actual Winsock
// spelling without importing that cpp-only header or its SystemApi initializer.
// Import Winsock before other libraries can import it without this boundary.
#pragma push_macro("timeval")
#define timeval dummy_timeval
#include <winsock2.h>
#pragma pop_macro("timeval")
#endif
