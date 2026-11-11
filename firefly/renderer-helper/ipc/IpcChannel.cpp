/** Admit one raw endpoint for the exact live configured preload; duplicates cannot revive it. */
#include "IpcEndpoint.h"
#include "native/ipc/application/Handshake.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include <LibCore/System.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/PageClient.h>

namespace Firefly {
void IpcEndpoint::attach_channel(
    WebContent::PageClient& page, IPC::TransportHandle handle, Bytes::IpcBinding binding)
{
    if (!m_owner || m_owner->is_detached() || !m_document || !m_document->is_fully_active()
        || m_byte_epoch || m_realm != binding.realm || !binding.epoch)
        return;
    auto nav = m_document->navigable();
    if (!nav || !nav->is_top_level_traversable() || &nav->page() != &page.page()
        || page.id().value() != binding.page
        || nav->id()
            != Web::HTML::CrossProcessId { binding.navigable_namespace, binding.navigable_local }
        || m_document->id()
            != Web::HTML::CrossProcessId { binding.document_namespace, binding.document_local })
        return;
    m_byte_epoch = binding.epoch;
    auto transport = handle.create_transport();
    if (transport.is_error()) {
        retire_channel();
        return;
    }

    auto weak = make_weak_ptr();
    auto created = Bytes::Channel::create(transport.release_value(),
        { .on_data =
                [weak](ReadonlyBytes bytes) {
                    if (auto owner = weak.strong_ref())
                        owner->receive_packet(bytes);
                },
            .on_writable =
                [weak] {
                    if (auto owner = weak.strong_ref())
                        owner->flush_packets();
                },
            .on_closed =
                [weak](auto) {
                    if (auto owner = weak.strong_ref())
                        owner->retire_channel();
                } });
    if (created.is_error()) {
        retire_channel();
        return;
    }

    m_byte_channel = created.release_value();
    auto hello = ApplicationIpc::encode_hello(
        { binding.epoch, static_cast<u64>(Core::System::getpid()), binding.document_namespace,
            binding.document_local, binding.navigable_namespace, binding.navigable_local });
    if (m_byte_channel->send(hello.span()).is_error())
        retire_channel();
    else
        flush_packets();
}

bool IpcEndpoint::send_packet(
    ApplicationIpc::Kind kind, u32 id, bool success, ByteString const& wire)
{
    if (m_byte_failed || !m_owner || m_owner->is_detached() || !m_document->is_fully_active())
        return false;
    auto reservation = m_wire_budget->reserve(wire.length());
    if (!reservation || wire.length() > ApplicationIpc::logical_wire_limit
        || m_byte_queue.size() >= ApplicationIpc::queue_limit) {
        retire_channel();
        return false;
    }

    m_byte_queue.append({ kind, id, success, wire, 0, move(reservation) });
    flush_packets();
    return !m_byte_failed;
}

void IpcEndpoint::flush_packets()
{
    if (m_byte_failed || !m_owner || m_owner->is_detached() || !m_document->is_fully_active()
        || !m_byte_channel || !m_byte_channel->is_open())
        return;
    while (!m_byte_queue.is_empty() && m_byte_channel->pending_send_count() < Bytes::max_pending) {
        auto& queued = m_byte_queue.first();
        if ((queued.kind == ApplicationIpc::Kind::Invoke
                || queued.kind == ApplicationIpc::Kind::Send)
            && !m_pending.contains(queued.request)) {
            // A partially published message cannot be skipped without corrupting
            // the peer's strict, noninterleaved logical stream.
            if (queued.offset) {
                retire_channel();
                return;
            }
            m_byte_queue.remove(0);
            continue;
        }
        auto packet = ApplicationIpc::FragmentEncoder::encode_next(queued.kind, queued.success,
            m_byte_epoch, queued.request, queued.wire.bytes(), queued.offset);
        if (packet.is_error()) {
            retire_channel();
            return;
        }
        queued.offset = packet.value().next_offset;
        if (packet.value().complete)
            m_byte_queue.remove(0);
        if (m_byte_channel->send(packet.value().bytes.bytes()).is_error()) {
            retire_channel();
            return;
        }
    }
}

void IpcEndpoint::clear_packets()
{
    m_byte_queue.clear();
    m_reassembler.reset();
    m_fragment_deadline.clear();
    m_receive_reservation = nullptr;
}

void IpcEndpoint::retire_channel()
{
    if (m_byte_failed)
        return;
    m_byte_failed = true;
    clear_packets();
    if (m_byte_channel)
        m_byte_channel->close();
    m_byte_channel = nullptr;
    Vector<u32> pending;
    for (auto const& entry : m_pending)
        pending.append(entry.key);
    auto error = MUST(BridgeData::from_string(u"IPC document channel closed"sv).encode_wire({}));
    for (auto id : pending)
        settle(id, false, error);
}

void IpcEndpoint::receive_packet(ReadonlyBytes bytes)
{
    if (m_byte_failed || !m_owner || m_owner->is_detached() || !m_document->is_fully_active())
        return;
    // Validate declared length/epoch before reserving or allocating reassembly.
    auto required = ApplicationIpc::FragmentReassembler::required_bytes(bytes, m_byte_epoch);
    if (required.is_error()) {
        retire_channel();
        return;
    }
    if (!m_reassembler.active()) {
        m_receive_reservation = m_wire_budget->reserve(required.value());
        if (!m_receive_reservation) {
            retire_channel();
            return;
        }
    }
    auto decoded = m_reassembler.accept(bytes, m_byte_epoch);
    if (decoded.is_error()) {
        retire_channel();
        return;
    }
    if (!decoded.value().has_value()) {
        if (!m_fragment_deadline.has_value())
            m_fragment_deadline = MonotonicTime::now() + AK::Duration::from_seconds(30);
        return;
    }
    m_fragment_deadline.clear();
    auto reservation = move(m_receive_reservation);
    auto packet = decoded.release_value().release_value();
    if (packet.kind != ApplicationIpc::Kind::Reply && packet.kind != ApplicationIpc::Kind::Event) {
        retire_channel();
        return;
    }

    auto wire = move(packet.payload);
    if (BridgeData::decode_wire(wire.view(), application_ipc_limits()).is_error()) {
        retire_channel();
        return;
    }

    if (packet.kind == ApplicationIpc::Kind::Reply)
        settle(packet.request, packet.success, wire, move(reservation));
    else {
        if (packet.request <= m_incoming_last || m_incoming_pending >= 16) {
            retire_channel();
            return;
        }

        m_incoming_last = packet.request;
        ++m_incoming_pending;
        receive_event(packet.request, move(wire), move(reservation));
    }
}

}
