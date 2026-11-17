/** Native-only PTY operations; no JavaScript values or borrowed VM storage. */
#pragma once
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace Mite::Pty {
struct Spawn {
    std::string executable, file, cwd;
    std::vector<std::string> args, env;
    int cols {}, rows {};
};

struct Process {
    int pid { -1 }, master { -1 };
    std::string name;
};

bool available();

int spawn(Spawn const&, Process&);

int read(Process&, char*, size_t, size_t&);

int write(Process&, char const*, size_t, size_t&);

int resize(Process&, int cols, int rows, int pixel_width, int pixel_height);

int signal(Process&, std::string const&);

int reap(Process&, bool& exited, int& code, int& signal);

void close_master(Process&);

void terminate_and_reap(Process&);

std::string foreground_process(Process const&);
// Private fresh-image mode. Call before VM/application initialization.
std::optional<int> maybe_run_pty_child(int argc, char** argv);
}
