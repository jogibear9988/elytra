/**
 * Spawn a fresh executable image before establishing the terminal session. No
 * application/VM code runs after fork, and no Node/NAPI implementation is used.
 * The private child reports exec/setup errno through a CLOEXEC startup pipe.
 */
#include "platform/terminal/pty/Posix.h"
#include "platform/UV.h"
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace Mite::Pty {
namespace {
    int error()
    {
        return uv_translate_sys_error(errno);
    }

    int owned_fd(int fd)
    {
        if (fd < 0)
            return -1;
        if (fd < 4) {
            auto copy = fcntl(fd, F_DUPFD_CLOEXEC, 4);
            auto saved = errno;
            close(fd);
            errno = saved;
            return copy;
        }

        if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {
            auto saved = errno;
            close(fd);
            errno = saved;
            return -1;
        }

        return fd;
    }

    std::vector<char*> pointers(std::vector<std::string>& values)
    {
        std::vector<char*> result;
        for (auto& value : values)
            result.push_back(value.data());
        result.push_back(nullptr);
        return result;
    }

    [[noreturn]] void child_error(int value)
    {
        auto* bytes = reinterpret_cast<char*>(&value);
        size_t offset = 0;
        while (offset < sizeof(value)) {
            auto size = ::write(3, bytes + offset, sizeof(value) - offset);
            if (size < 0 && errno == EINTR)
                continue;
            if (size <= 0)
                break;
            offset += static_cast<size_t>(size);
        }

        _exit(127);
    }
}

bool available()
{
#if defined(__APPLE__)
    return true;
#elif defined(__GLIBC__) && defined(__GLIBC_PREREQ)
    return __GLIBC_PREREQ(2, 34);
#else
    return false;
#endif
}

std::optional<int> maybe_run_pty_child(int argc, char** argv)
{
    if (argc < 2 || std::strcmp(argv[1], "--mite-pty-child-v1"))
        return {};
    if (argc < 4)
        return 127;
    if (fcntl(3, F_SETFD, FD_CLOEXEC) == -1)
        return 127;
    if (setsid() == -1)
        child_error(errno);
    char name[1024];
    auto result = ttyname_r(STDIN_FILENO, name, sizeof(name));
    if (result)
        child_error(result);
    auto slave = ::open(name, O_RDWR);
    if (slave == -1)
        child_error(errno);
    if (ioctl(slave, TIOCSCTTY, 0) == -1)
        child_error(errno);
    close(slave);
    if (chdir(argv[2]) == -1)
        child_error(errno);
    sigset_t empty;
    sigemptyset(&empty);
    if (sigprocmask(SIG_SETMASK, &empty, nullptr) == -1)
        child_error(errno);
    for (int number = 1; number < NSIG; ++number) {
        if (number != SIGKILL && number != SIGSTOP)
            ::signal(number, SIG_DFL);
    }

    execvp(argv[3], argv + 3);
    child_error(errno);
}

int spawn(Spawn const& options, Process& process)
{
    int master = -1, slave = -1, input = -1, output = -1;
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    auto status = posix_spawn_file_actions_init(&actions);
    if (status)
        return uv_translate_sys_error(status);
    status = posix_spawnattr_init(&attributes);
    if (status) {
        posix_spawn_file_actions_destroy(&actions);
        return uv_translate_sys_error(status);
    }

    auto cleanup = [&] {
        posix_spawn_file_actions_destroy(&actions);
        posix_spawnattr_destroy(&attributes);
        for (auto fd : { master, slave, input, output })
            if (fd >= 0)
                close(fd);
    };

    master = owned_fd(posix_openpt(O_RDWR | O_NOCTTY));
    if (master < 0 || grantpt(master) || unlockpt(master)) {
        status = error();
        cleanup();
        return status;
    }

    char name[1024];
#ifdef __APPLE__
    if (ioctl(master, TIOCPTYGNAME, name) == -1) {
        status = error();
        cleanup();
        return status;
    }
#else
    status = ptsname_r(master, name, sizeof(name));
    if (status) {
        cleanup();
        return uv_translate_sys_error(status);
    }
#endif
    slave = owned_fd(::open(name, O_RDWR | O_NOCTTY));
    if (slave < 0) {
        status = error();
        cleanup();
        return status;
    }

    winsize size {};
    size.ws_col = static_cast<unsigned short>(options.cols);
    size.ws_row = static_cast<unsigned short>(options.rows);
    termios mode {};
    if (tcgetattr(slave, &mode) == -1) {
        status = error();
        cleanup();
        return status;
    }
#ifdef IUTF8
    mode.c_iflag |= IUTF8;
#endif
    if (tcsetattr(slave, TCSANOW, &mode) == -1 || ioctl(slave, TIOCSWINSZ, &size) == -1) {
        status = error();
        cleanup();
        return status;
    }

    int pipe[2];
    if (::pipe(pipe) == -1) {
        status = error();
        cleanup();
        return status;
    }

    input = owned_fd(pipe[0]);
    output = owned_fd(pipe[1]);
    if (input < 0 || output < 0) {
        status = error();
        cleanup();
        return status;
    }

    for (int index = 0; index < 3 && !status; ++index)
        status = posix_spawn_file_actions_adddup2(&actions, slave, index);
    if (!status)
        status = posix_spawn_file_actions_adddup2(&actions, output, 3);
#ifdef __APPLE__
    short flags = POSIX_SPAWN_CLOEXEC_DEFAULT;
#else
    short flags = 0;
    // glibc's closefrom action closes only child descriptors after explicit dup2.
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 34)
    if (!status)
        status = posix_spawn_file_actions_addclosefrom_np(&actions, 4);
#else
    status = ENOSYS;
#endif
#else
    status = ENOSYS;
#endif
#endif
    if (!status)
        status = posix_spawnattr_setflags(&attributes, flags);
    auto arguments = std::vector<std::string> { options.executable, "--mite-pty-child-v1",
        options.cwd, options.file };
    arguments.insert(arguments.end(), options.args.begin(), options.args.end());
    auto environment = options.env;
    auto args = pointers(arguments), env = pointers(environment);
    pid_t pid = -1;
    if (!status)
        status = posix_spawn(
            &pid, options.executable.c_str(), &actions, &attributes, args.data(), env.data());
    if (status) {
        cleanup();
        return uv_translate_sys_error(status);
    }

    close(output);
    output = -1;
    close(slave);
    slave = -1;
    // No JavaScript is pumped during this bounded native startup handshake.
    int child_status = 0;
    size_t received = 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    for (;;) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now())
                             .count();
        if (remaining <= 0) {
            status = UV_ETIMEDOUT;
            break;
        }

        pollfd descriptor { input, POLLIN | POLLHUP, 0 };
        auto ready = poll(&descriptor, 1, static_cast<int>(remaining));
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready <= 0) {
            status = ready ? error() : UV_ETIMEDOUT;
            break;
        }

        auto amount = ::read(input, reinterpret_cast<char*>(&child_status) + received,
            sizeof(child_status) - received);
        if (amount < 0 && errno == EINTR)
            continue;
        if (amount < 0) {
            status = error();
            break;
        }

        if (!amount) {
            status = received ? UV_EIO : 0;
            break;
        }

        received += static_cast<size_t>(amount);
        if (received == sizeof(child_status)) {
            status = uv_translate_sys_error(child_status ? child_status : EIO);
            break;
        }
    }

    if (!status && fcntl(master, F_SETFL, O_NONBLOCK) == -1)
        status = error();
    if (status) {
        kill(pid, SIGKILL);
        while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) { }
        cleanup();
        return status;
    }

    process = { static_cast<int>(pid), master, name };
    master = -1;
    cleanup();
    return 0;
}

int read(Process& process, char* bytes, size_t capacity, size_t& count)
{
    ssize_t amount;
    do {
        amount = ::read(process.master, bytes, capacity);
    } while (amount < 0 && errno == EINTR);
    if (amount < 0)
        return errno == EIO ? UV_EOF : error();
    count = static_cast<size_t>(amount);
    return amount ? 0 : UV_EOF;
}

int write(Process& process, char const* bytes, size_t size, size_t& count)
{
    ssize_t amount;
    do {
        amount = ::write(process.master, bytes, size);
    } while (amount < 0 && errno == EINTR);
    if (amount < 0)
        return error();
    count = static_cast<size_t>(amount);
    return 0;
}

int resize(Process& process, int cols, int rows, int width, int height)
{
    winsize size {};
    size.ws_col = static_cast<unsigned short>(cols);
    size.ws_row = static_cast<unsigned short>(rows);
    size.ws_xpixel = static_cast<unsigned short>(width);
    size.ws_ypixel = static_cast<unsigned short>(height);
    return ioctl(process.master, TIOCSWINSZ, &size) == -1 ? error() : 0;
}

int signal(Process& process, std::string const& name)
{
    int number = 0;
    if (name == "SIGHUP")
        number = SIGHUP;
    else if (name == "SIGTERM")
        number = SIGTERM;
    else if (name == "SIGKILL")
        number = SIGKILL;
    else if (name == "SIGINT")
        number = SIGINT;
    else if (name == "SIGQUIT")
        number = SIGQUIT;
    else if (name == "SIGUSR1")
        number = SIGUSR1;
    else if (name == "SIGUSR2")
        number = SIGUSR2;
    else if (name == "SIGCONT")
        number = SIGCONT;
    else if (name == "SIGSTOP")
        number = SIGSTOP;
    else
        return UV_EINVAL;
    return kill(process.pid, number) == -1 ? error() : 0;
}

int reap(Process& process, bool& exited, int& code, int& signal)
{
    int status = 0;
    pid_t result;
    do {
        result = waitpid(process.pid, &status, WNOHANG);
    } while (result < 0 && errno == EINTR);
    if (result < 0)
        return error();
    exited = result > 0;
    if (exited) {
        code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
        signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
    }

    return 0;
}

void close_master(Process& process)
{
    if (process.master >= 0)
        close(process.master);
    process.master = -1;
}

void terminate_and_reap(Process& process)
{
    close_master(process);
    if (process.pid > 0) {
        kill(process.pid, SIGKILL);
        while (waitpid(process.pid, nullptr, 0) < 0 && errno == EINTR) { }
        process.pid = -1;
    }
}

std::string foreground_process(Process const& process)
{
    auto group = tcgetpgrp(process.master);
    if (group < 0)
        return {};
#ifdef __APPLE__
    int mib[] { CTL_KERN, KERN_PROC, KERN_PROC_PID, group };
    kinfo_proc info {};
    size_t size = sizeof(info);
    if (sysctl(mib, 4, &info, &size, nullptr, 0) || size != sizeof(info))
        return {};
    return info.kp_proc.p_comm;
#else
    auto path = "/proc/" + std::to_string(group) + "/cmdline";
    auto fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return {};
    char name[4096];
    auto length = ::read(fd, name, sizeof(name));
    close(fd);
    if (length <= 0)
        return {};
    auto end = static_cast<size_t>(length);
    for (size_t index = 0; index < end; ++index)
        if (!name[index]) {
            end = index;
            break;
        }

    return { name, end };
#endif
}
}
