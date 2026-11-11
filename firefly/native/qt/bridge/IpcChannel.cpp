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
                    if (state.ipc_byte_ready) {
                        receive_ipc_packet(id, epoch, bytes);
                        return;
                    }

                    auto hello = ApplicationIpc::decode_hello(bytes);
                    if (hello.is_error() || hello.value().epoch != epoch || !state.native_view()
                        || hello.value().pid
                            != static_cast<u64>(state.native_view()->web_content_pid())
                        || hello.value().document_namespace != document.namespace_id
                        || hello.value().document_local != document.local_id
                        || hello.value().navigable_namespace != navigable.namespace_id
                        || hello.value().navigable_local != navigable.local_id) {
                        retire_ipc_channel(id);
                        warnln("ERR_FIREFLY_IPC_BYTES: invalid document channel hello");
                        return;
                    }

                    state.ipc_byte_ready = true;
                    state.ipc_byte_deadline.clear();
                    flush_ipc_packets(id, epoch);
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
            .on_writable =
                [this, lifetime, id, epoch] {
                    if (lifetime && !stopping) {
                        flush_ipc_packets(id, epoch);
                        emit_renderer_port(id, epoch, "renderer-port-writable");
                    }
                },
            .on_closed =
                [this, lifetime, id, epoch](auto) {
                    if (!lifetime || stopping)
                        return;
                    auto found = windows.find(id);
                    if (found == windows.end() || found->second.ipc_byte_epoch != epoch)
                        return;
                    retire_ipc_channel(id);
                } });
    if (channel.is_error()) {
        state.ipc_byte_deadline.clear();
        warnln("ERR_FIREFLY_IPC_BYTES: cannot initialize document channel");
        return;
    }

    state.ipc_byte_channel = channel.release_value();
    auto probe = std::getenv("FIREFLY_IPC_BYTES_DIAGNOSTIC_ADMISSION");
    bool test_admission = probe && probe[0] == '1';
    if (test_admission) {
        probe_ipc_handoff(id, page, navigable, document, epoch, realm_id, 0);
        probe_ipc_handoff(id, page, navigable, document, epoch, realm_id, 1);
    }

    page.client().async_firefly_connect_ipc(move(endpoint.remote_handle), page.id().value(),
        navigable.namespace_id, navigable.local_id, document.namespace_id, document.local_id,
        MUST(String::from_utf8(realm_id.view())), epoch);
    if (test_admission)
        probe_ipc_handoff(id, page, navigable, document, epoch, realm_id, 2);
}

// Explicit native diagnostic only. The ordinary build path does not allocate
// these observers. An application page cannot manufacture a trusted handoff.
void Bridge::Impl::probe_ipc_handoff(int id, WebView::WebContentPage& page,
    Web::HTML::CrossProcessId nav, Web::HTML::CrossProcessId document, u64 epoch,
    ByteString realm_id, u8 scenario)
{
    auto pair = IPC::Transport::create_paired();
    if (pair.is_error())
        return;
    auto endpoint = pair.release_value();
    auto observer = Bytes::Channel::create(move(endpoint.local),
        { .on_data =
                [id, epoch, scenario](ReadonlyBytes) {
                    std::fprintf(stderr,
                        "FIREFLY_IPC_BYTES_BAD_HANDOFF_ACCEPTED window=%d epoch=%llu case=%u\n", id,
                        static_cast<unsigned long long>(epoch), scenario);
                },
            .on_writable = {},
            .on_closed =
                [id, epoch, scenario](Bytes::Channel::CloseReason reason) {
                    if (reason == Bytes::Channel::CloseReason::Peer)
                        std::fprintf(stderr,
                            "FIREFLY_IPC_BYTES_REJECTED window=%d epoch=%llu case=%u\n", id,
                            static_cast<unsigned long long>(epoch), scenario);
                } });
    if (observer.is_error())
        return;
    windows.at(id).ipc_admission_probes.append(observer.release_value());
    // Unknown realm and wrong document must not consume the real admission.
    // A duplicate correct handle follows the real handoff on the same control stream.
    auto realm
        = scenario == 0 ? "firefly.invalid-realm"_string : MUST(String::from_utf8(realm_id.view()));
    auto doc_local = scenario == 1 ? document.local_id ^ 0x8000000000000000ULL : document.local_id;
    page.client().async_firefly_connect_ipc(move(endpoint.remote_handle), page.id().value(),
        nav.namespace_id, nav.local_id, document.namespace_id, doc_local, move(realm), epoch);
}

void Bridge::Impl::retire_ipc_channel(int id)
{
    auto found = windows.find(id);
    if (found == windows.end())
        return;
    auto& state = found->second;
    reset_renderer_ports(id);
    state.ipc_byte_ready = false;
    state.ipc_byte_deadline.clear();
    state.ipc_byte_queue.clear();
    state.ipc_reassembler.reset();
    state.ipc_receive_reservation = nullptr;
    state.ipc_fragment_deadline.clear();
    state.ipc_packets.clear();
    state.ipc_refused.clear();
    if (state.ipc_byte_channel)
        state.ipc_byte_channel->close();
    state.ipc_byte_channel = nullptr;
    state.ipc_outgoing_failed = true;
    ipc_pending_count -= state.ipc_pending.size();
    state.ipc_pending.clear();
    ipc_outgoing_count -= state.ipc_outgoing.size();
    state.ipc_outgoing.clear();
}

bool Bridge::Impl::queue_ipc_packet(
    int id, ApplicationIpc::Kind kind, u32 request, bool success, ByteString const& wire)
{
    auto& state = windows.at(id);
    if (!state.ipc_byte_channel || !state.ipc_byte_channel->is_open())
        return false;
    auto reservation = state.ipc_wire_budget->reserve(wire.length());
    if (!reservation || wire.length() > ApplicationIpc::logical_wire_limit
        || state.ipc_byte_queue.size() >= ApplicationIpc::queue_limit) {
        retire_ipc_channel(id);
        return false;
    }

    state.ipc_byte_queue.append({ kind, request, success, wire, 0, move(reservation) });
    flush_ipc_packets(id, state.ipc_byte_epoch);
    return !!state.ipc_byte_channel;
}

void Bridge::Impl::flush_ipc_packets(int id, u64 epoch)
{
    auto found = windows.find(id);
    if (found == windows.end() || found->second.ipc_byte_epoch != epoch)
        return;
    auto& state = found->second;
    if (!state.ipc_byte_ready || !state.ipc_byte_channel || !state.ipc_byte_channel->is_open())
        return;
    while (!state.ipc_byte_queue.is_empty()
        && state.ipc_byte_channel->pending_send_count() < Bytes::max_pending) {
        auto& packet = state.ipc_byte_queue.first();
        // Never abandon a partially emitted logical message: the peer is already
        // holding its first fragment. Endpoint timeout retires the entire channel.
        if (packet.offset == 0 && packet.kind == ApplicationIpc::Kind::Event
            && !state.ipc_outgoing.contains(packet.request)) {
            state.ipc_byte_queue.take_first();
            continue;
        }

        auto fragment = ApplicationIpc::FragmentEncoder::encode_next(
            packet.kind, packet.success, epoch, packet.request, packet.wire.bytes(), packet.offset);
        if (fragment.is_error()) {
            retire_ipc_channel(id);
            return;
        }

        auto next = fragment.release_value();
        if (state.ipc_byte_channel->send(next.bytes.bytes()).is_error()) {
            retire_ipc_channel(id);
            return;
        }

        packet.offset = next.next_offset;
        if (next.complete)
            state.ipc_byte_queue.take_first();
    }
}

}
