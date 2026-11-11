/** Compile actual OpenSSL, runtime and WinSDK header orders; no TLS/OS operations. */
#ifndef MITE_SSL_HEADER_ORDER
#define MITE_SSL_HEADER_ORDER 0
#endif

#if MITE_SSL_HEADER_ORDER == 0
#include "core/GCCaptures.h"

#include "platform/WindowsSockets.h"
#include <openssl/ssl.h>
#elif MITE_SSL_HEADER_ORDER == 1
#include "platform/WindowsSockets.h"
#include <openssl/ssl.h>

#include "core/GCCaptures.h"
#elif MITE_SSL_HEADER_ORDER == 2
#include "platform/UV.h"

#include "core/GCCaptures.h"
#include <openssl/ssl.h>
#elif MITE_SSL_HEADER_ORDER == 3
#include "platform/WindowsSockets.h"
#include <openssl/ssl.h>

#include "core/GCCaptures.h"
#include "platform/UV.h"
#elif MITE_SSL_HEADER_ORDER == 4
#include <AK/Time.h>
// The upstream native boundary remains cpp-only and follows upstream headers.
#ifdef AK_OS_WINDOWS
#include <AK/Windows.h>
#endif

#include "platform/UV.h"
#include "platform/WindowsSockets.h"
#include <openssl/ssl.h>
#elif MITE_SSL_HEADER_ORDER == 5
#include "core/GCCaptures.h"

#include "platform/network/tls/TrustStore.h"

#include "platform/UV.h"
#else
#error "Unknown OpenSSL socket-header regression order"
#endif

#include <type_traits>

#ifdef timeval
#error "The socket boundary leaked its temporary WinSDK timeval spelling"
#endif

using NewSSL = SSL* (*)(SSL_CTX*);
static_assert(std::is_same_v<decltype(&SSL_new), NewSSL>);

#ifdef AK_OS_WINDOWS
static_assert(std::is_same_v<decltype(timeval {}.tv_sec), long>);
static_assert(std::is_same_v<decltype(dummy_timeval {}.tv_sec), long>);
static_assert(sizeof(timeval) == sizeof(dummy_timeval));
using Select = int(WSAAPI*)(int, fd_set*, fd_set*, fd_set*, dummy_timeval const*);
static_assert(std::is_same_v<decltype(&::select), Select>);
#endif
