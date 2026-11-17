/** PTY providers must be implemented and accepted per platform. */
#include "platform/UV.h"
#include "platform/terminal/pty/Posix.h"
#include <cstring>

namespace Mite::Pty {
bool available()
{
    return false;
}

int spawn(Spawn const&, Process&)
{
    return UV_ENOSYS;
}

int read(Process&, char*, size_t, size_t&)
{
    return UV_ENOSYS;
}

int write(Process&, char const*, size_t, size_t&)
{
    return UV_ENOSYS;
}

int resize(Process&, int, int, int, int)
{
    return UV_ENOSYS;
}

int signal(Process&, std::string const&)
{
    return UV_ENOSYS;
}

int reap(Process&, bool&, int&, int&)
{
    return UV_ENOSYS;
}

void close_master(Process&)
{
}

void terminate_and_reap(Process&)
{
}

std::string foreground_process(Process const&)
{
    return {};
}

std::optional<int> maybe_run_pty_child(int argc, char** argv)
{
    if (argc >= 2 && !std::strcmp(argv[1], "--mite-pty-child-v1"))
        return 127;
    return {};
}
}
