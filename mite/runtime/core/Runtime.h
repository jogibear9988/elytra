/**
 * Owns the LibJS VM, realm and application lifetime. Embedding hooks run on the VM owner
 * thread; teardown disconnects native callbacks before releasing JavaScript state.
 */

#pragma once

#include <LibJS/Runtime/Completion.h>
#include <functional>
#include <optional>

namespace JS {
class VM;
class Realm;
}

namespace Mite {
class ModuleLoader;
class EventLoop;

// Hooks run on the VM's owner thread. initialize sees the installed Mite globals
// and an active realm before the entry module executes. drive owns the external
// loop until application work ends; it must pump EventLoop and return its status.
// shutdown disconnects host callbacks while the realm and loader are still alive,
// including after a partially failed initialize. No callbacks may outlive run().
struct HostHooks {
    std::function<JS::ThrowCompletionOr<void>(JS::VM&, JS::Realm&, ModuleLoader&, EventLoop&)>
        initialize;
    std::function<int(EventLoop&)> drive;
    std::function<void()> shutdown;
    bool terminate_pending_work_on_stop { false };
    // Process-global OS handlers require explicit, coordinated host ownership.
    bool allow_process_signals { false };
    // An explicit host status wins over process.exitCode and pending entry TLA.
    std::optional<int> exit_code;
};

// Hosts provide UTF-8 argument bytes; native Windows CLI conversion is at wmain.
int run(int argc, char** argv, HostHooks const& = {});
}
