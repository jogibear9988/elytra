/** Compile real runtime/WinSDK headers in every supported include order; no OS calls. */
#ifndef MITE_UV_HEADER_ORDER
#define MITE_UV_HEADER_ORDER 0
#endif

#if MITE_UV_HEADER_ORDER == 0
#include <AK/Time.h>

#include "platform/UV.h"
#elif MITE_UV_HEADER_ORDER == 1
#include "platform/UV.h"

#include <AK/Time.h>
#elif MITE_UV_HEADER_ORDER == 2
#include <AK/Time.h>
// Upstream's Win32 boundary belongs in cpp files, after its other headers.
#ifdef AK_OS_WINDOWS
#include <AK/Windows.h>
#endif
#include "platform/UV.h"
#else
#error "Unknown libuv header regression order"
#endif

#include "core/GCCaptures.h"
#include <type_traits>

#ifdef timeval
#error "The libuv boundary leaked its temporary WinSDK timeval spelling"
#endif

static_assert(std::is_same_v<decltype(uv_loop_t {}.data), void*>);

#ifdef AK_OS_WINDOWS
// Check the actual imported WinSDK declarations as well as upstream's type.
// Neither type's layout nor a public Winsock function signature is replaced.
static_assert(std::is_same_v<decltype(timeval {}.tv_sec), long>);
static_assert(std::is_same_v<decltype(dummy_timeval {}.tv_sec), long>);
static_assert(sizeof(timeval) == sizeof(dummy_timeval));
using Select = int(WSAAPI*)(int, fd_set*, fd_set*, fd_set*, dummy_timeval const*);
static_assert(std::is_same_v<decltype(&::select), Select>);
#endif
