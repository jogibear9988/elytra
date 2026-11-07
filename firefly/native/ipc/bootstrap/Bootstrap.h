/** Trusted control-plane endpoint handoff. No page/global/preload API is installed here. */
#pragma once
#include <AK/Function.h>
#include <AK/String.h>
#include <AK/Utf16String.h>
#include <LibIPC/TransportHandle.h>

namespace IPC {
class Stub;
}

namespace Firefly::Bytes {
struct BootstrapCookie {
    u64 high;
    u64 low;
};

using BootstrapReceiver = Function<void(IPC::TransportHandle, BootstrapCookie)>;

void set_bootstrap_receiver(BootstrapReceiver);

void accept_bootstrap(IPC::TransportHandle, u64 high, u64 low);

struct NavigationBinding {
    u64 page;
    u64 navigable_namespace;
    u64 navigable_local;
    Utf16String navigation;
    String url;
    u64 epoch;
};

using NavigationReceiver = Function<void(IPC::Stub&, IPC::TransportHandle, NavigationBinding)>;
void set_navigation_receiver(NavigationReceiver);

void accept_navigation(IPC::Stub&, IPC::TransportHandle, u64 page, u64 navigable_namespace,
    u64 navigable_local, Utf16String navigation, String url, u64 epoch);

struct DocumentBinding {
    NavigationBinding navigation;
    u64 document_namespace;
    u64 document_local;
};

using DocumentReceiver = Function<void(IPC::Stub&, IPC::TransportHandle, DocumentBinding)>;
void set_document_receiver(DocumentReceiver);

void accept_document(IPC::Stub&, IPC::TransportHandle, u64 page, u64 navigable_namespace,
    u64 navigable_local, u64 document_namespace, u64 document_local, Utf16String navigation,
    String url, u64 epoch);
}
