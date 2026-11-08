/** Trusted parent-side channel handoff uses the already authenticated canonical preload binding. */
#include "Impl.h"
#include "native/ipc/application/Handshake.h"
#include <LibIPC/Transport.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <QPointer>
#include <UI/Qt/WebContentView.h>
#include <cstdio>
#include <cstdlib>

namespace Firefly {
void Bridge::Impl::bind_ipc_channel(int id, WebView::WebContentPage& page,
    Web::HTML::CrossProcessId navigable, Web::HTML::CrossProcessId document)
{
    auto& state = windows.at(id);
    if (state.ipc_byte_epoch || state.ipc_realm.is_empty() || !state.preload.has_value()
        || ipc_byte_next_epoch == NumericLimits<u64>::max())
        return;
    auto epoch = ++ipc_byte_next_epoch;
    state.ipc_byte_deadline = MonotonicTime::now() + AK::Duration::from_seconds(30);
    state.ipc_byte_epoch = epoch; // A failure retires this document, never grants a retry loop.
    auto pair = IPC::Transport::create_paired();
    if (pair.is_error()) {
        state.ipc_byte_deadline.clear();
        warnln("ERR_FIREFLY_IPC_BYTES: cannot create document channel");
        return;
    }

    auto endpoint = pair.release_value();
    auto doc = state.ipc_document, realm_id = state.ipc_realm;
    auto lifetime = QPointer<QObject>(context.get());
    auto channel = Bytes::Channel::create(move(endpoint.local),
        { .on_data =
                [this, lifetime, id, epoch, doc, realm_id, document, navigable](
                    ReadonlyBytes bytes) {
                    if (!lifetime || stopping || !ipc_current(id, doc, realm_id))
                        return;
                    auto& state = windows.at(id);
                    if (state.ipc_byte_epoch != epoch || !state.ipc_byte_channel)
                        return;
                    auto hello = ApplicationIpc::decode_hello(bytes);
                    if (state.ipc_byte_ready || hello.is_error() || hello.value().epoch != epoch
                        || hello.value().pid != static_cast<u64>(state.view->web_content_pid())
                        || hello.value().document_namespace != document.namespace_id
                        || hello.value().document_local != document.local_id
                        || hello.value().navigable_namespace != navigable.namespace_id
                        || hello.value().navigable_local != navigable.local_id) {
                        state.ipc_byte_channel->close();
                        state.ipc_byte_ready = false;
                        state.ipc_byte_deadline.clear();
                        state.ipc_byte_channel = nullptr;
                        warnln("ERR_FIREFLY_IPC_BYTES: invalid document channel hello");
                        return;
                    }

                    state.ipc_byte_ready = true;
                    state.ipc_byte_deadline.clear();
                    // Diagnostic records contain native identity only, never application data.
                    if (auto* value = std::getenv("FIREFLY_IPC_BYTES_DIAGNOSTIC");
                        value && value[0] == '1') {
                        std::fprintf(stderr,
                            "FIREFLY_IPC_BYTES_READY window=%d epoch=%llu pid=%llu "
                            "document=%llu:%llu\n",
                            id, static_cast<unsigned long long>(epoch),
                            static_cast<unsigned long long>(hello.value().pid),
                            static_cast<unsigned long long>(document.namespace_id),
                            static_cast<unsigned long long>(document.local_id));
                    }
                },
            .on_writable = {},
            .on_closed =
                [this, lifetime, id, epoch](auto) {
                    if (!lifetime || stopping)
                        return;
                    auto found = windows.find(id);
                    if (found == windows.end() || found->second.ipc_byte_epoch != epoch)
                        return;
                    found->second.ipc_byte_ready = false;
                    found->second.ipc_byte_deadline.clear();
                    found->second.ipc_byte_channel = nullptr;
                } });
    if (channel.is_error()) {
        state.ipc_byte_deadline.clear();
        warnln("ERR_FIREFLY_IPC_BYTES: cannot initialize document channel");
        return;
    }

    state.ipc_byte_channel = channel.release_value();
    page.client().async_firefly_connect_ipc(move(endpoint.remote_handle), page.id().value(),
        navigable.namespace_id, navigable.local_id, document.namespace_id, document.local_id,
        MUST(String::from_utf8(realm_id.view())), epoch);
}
}
