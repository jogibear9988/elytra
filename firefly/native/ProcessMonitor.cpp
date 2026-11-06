/*
 * Derived from Ladybird's ProcessMonitor integration interface.
 * Copyright (c) 2025, ayeteadoe <ayeteadoe@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause
 */
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <AK/Vector.h>
#include <LibCore/System.h>
#include <LibCore/Timer.h>
#include <LibWebView/ProcessMonitor.h>

namespace WebView {
namespace {
// Do not install a second SIGCHLD handler alongside libuv, or reap an
// unregistered application child. Keep timer ownership outside the pinned ABI.
AK::NeverDestroyed<HashMap<int, NonnullRefPtr<Core::Timer>>> monitors;
int next_monitor { 1 };
}
ProcessMonitor::ProcessMonitor(Function<void(pid_t, Optional<int>)> exit_handler)
    : m_on_process_exit(move(exit_handler))
{
    VERIFY(next_monitor < 0x7fffffff);
    m_signal_handle = next_monitor++;
    auto timer = Core::Timer::create_repeating(AK::Duration::from_milliseconds(10), [this] {
        Vector<pid_t> snapshot;
        for (auto pid : m_monitored_processes) snapshot.append(pid);
        for (auto pid : snapshot) {
            if (!m_monitored_processes.contains(pid)) continue;
            auto result = Core::System::waitpid(pid, WNOHANG);
            if (result.is_error() || result.value().pid != pid) continue;
            auto status = result.value().status;
            if (WIFEXITED(status) || WIFSIGNALED(status)) {
                m_monitored_processes.remove(pid);
                m_on_process_exit(pid, status);
            }
        }
    });
    timer->start();
    monitors->set(m_signal_handle, move(timer));
}
ProcessMonitor::~ProcessMonitor()
{
    if (auto timer = monitors->get(m_signal_handle); timer.has_value()) timer.value()->stop();
    monitors->remove(m_signal_handle);
}
void ProcessMonitor::add_process(pid_t pid)
{
    m_monitored_processes.set(pid, AK::HashSetExistingEntryBehavior::Keep);
}
}
