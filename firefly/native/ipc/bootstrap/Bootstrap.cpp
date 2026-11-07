/** Keep registration in one native IPC library instance across the helper's generated ABI. */
#include "Bootstrap.h"
#include <AK/NeverDestroyed.h>
#include <AK/RefPtr.h>

namespace Firefly::Bytes {
namespace {
    struct Receiver : RefCounted<Receiver> {
        explicit Receiver(BootstrapReceiver callback)
            : callback(move(callback))
        {
        }

        BootstrapReceiver callback;
    };

    NeverDestroyed<RefPtr<Receiver>> receiver;
}

void set_bootstrap_receiver(BootstrapReceiver callback)
{
    receiver.get() = callback ? adopt_ref(*new Receiver(move(callback))) : RefPtr<Receiver> {};
}

void accept_bootstrap(IPC::TransportHandle handle, u64 high, u64 low)
{
    auto callback = receiver.get();
    if (!callback || !(high || low)) {
        dbgln("Firefly byte bootstrap unavailable/invalid; closing transferred endpoint");
        return;
    }

    callback->callback(move(handle), { high, low });
}
}
