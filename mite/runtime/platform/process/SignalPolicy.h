/** Executable-owned POSIX startup policy. Embedding run() never changes host signals. */
#pragma once

#ifndef _WIN32
#include <csignal>
#endif

namespace Mite {
inline bool ignore_broken_pipe_signal()
{
#ifdef _WIN32
    return true;
#else
    // libuv reports write EPIPE only if the default SIGPIPE action does not kill
    // the process first. Standalone hosts opt in before running application code.
    return std::signal(SIGPIPE, SIG_IGN) != SIG_ERR;
#endif
}
}
