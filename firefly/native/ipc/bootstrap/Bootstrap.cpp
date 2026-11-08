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

    struct NavigationCallback : RefCounted<NavigationCallback> {
        explicit NavigationCallback(NavigationReceiver value)
            : callback(move(value))
        {
        }

        NavigationReceiver callback;
    };

    NeverDestroyed<RefPtr<NavigationCallback>> navigation_receiver;

    struct DocumentCallback : RefCounted<DocumentCallback> {
        explicit DocumentCallback(DocumentReceiver value)
            : callback(move(value))
        {
        }

        DocumentReceiver callback;
    };

    NeverDestroyed<RefPtr<DocumentCallback>> document_receiver;

    struct IpcCallback : RefCounted<IpcCallback> {
        explicit IpcCallback(IpcReceiver value)
            : callback(move(value))
        {
        }

        IpcReceiver callback;
    };

    NeverDestroyed<RefPtr<IpcCallback>> ipc_receiver;

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

void set_navigation_receiver(NavigationReceiver callback)
{
    navigation_receiver.get() = callback ? adopt_ref(*new NavigationCallback(move(callback)))
                                         : RefPtr<NavigationCallback> {};
}

void accept_navigation(IPC::Stub& stub, IPC::TransportHandle handle, u64 page, u64 ns, u64 local,
    Utf16String navigation, String url, u64 epoch)
{
    auto callback = navigation_receiver.get();
    if (!callback || !epoch || !ns || !local || navigation.is_empty()
        || navigation.length_in_code_units() > 1024 || url.is_empty() || url.bytes().size() > 8192)
        return; // Dropping the transferred handle closes an unadmitted endpoint.
    callback->callback(stub, move(handle), { page, ns, local, move(navigation), move(url), epoch });
}

void set_document_receiver(DocumentReceiver callback)
{
    document_receiver.get()
        = callback ? adopt_ref(*new DocumentCallback(move(callback))) : RefPtr<DocumentCallback> {};
}

void accept_document(IPC::Stub& stub, IPC::TransportHandle handle, u64 page, u64 ns, u64 local,
    u64 document_ns, u64 document_local, Utf16String navigation, String url, u64 epoch)
{
    auto callback = document_receiver.get();
    if (!callback || !epoch || !ns || !local || !document_ns || !document_local
        || navigation.is_empty() || navigation.length_in_code_units() > 1024 || url.is_empty()
        || url.bytes().size() > 8192)
        return;
    callback->callback(stub, move(handle),
        { { page, ns, local, move(navigation), move(url), epoch }, document_ns, document_local });
}

void set_ipc_receiver(IpcReceiver callback)
{
    ipc_receiver.get()
        = callback ? adopt_ref(*new IpcCallback(move(callback))) : RefPtr<IpcCallback> {};
}

void accept_ipc(IPC::Stub& stub, IPC::TransportHandle handle, u64 page, u64 ns, u64 local,
    u64 document_ns, u64 document_local, String realm, u64 epoch)
{
    auto callback = ipc_receiver.get();
    if (!callback || !epoch || !ns || !local || !document_ns || !document_local || realm.is_empty()
        || realm.bytes().size() > 1024)
        return;
    callback->callback(
        stub, move(handle), { page, ns, local, document_ns, document_local, move(realm), epoch });
}

}
