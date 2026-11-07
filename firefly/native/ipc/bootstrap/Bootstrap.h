/** Trusted control-plane endpoint handoff. No page/global/preload API is installed here. */
#pragma once
#include <AK/Function.h>
#include <LibIPC/TransportHandle.h>

namespace Firefly::Bytes {
struct BootstrapCookie {
    u64 high;
    u64 low;
};

using BootstrapReceiver = Function<void(IPC::TransportHandle, BootstrapCookie)>;

void set_bootstrap_receiver(BootstrapReceiver);

void accept_bootstrap(IPC::TransportHandle, u64 high, u64 low);
}
