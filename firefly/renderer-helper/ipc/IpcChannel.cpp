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
    if (wire.length() > ApplicationIpc::wire_limit
        || m_byte_queue.size() >= ApplicationIpc::queue_limit) {
        retire_channel();
        return false;
    }

    m_byte_queue.append({ kind, id, success, wire });
    flush_packets();
    return !m_byte_failed;
}

void IpcEndpoint::flush_packets()
{
    if (m_byte_failed || !m_owner || m_owner->is_detached() || !m_document->is_fully_active()
        || !m_byte_channel || !m_byte_channel->is_open())
        return;
    while (!m_byte_queue.is_empty() && m_byte_channel->pending_send_count() < Bytes::max_pending) {
        auto queued = m_byte_queue.take_first();
        if ((queued.kind == ApplicationIpc::Kind::Invoke
                || queued.kind == ApplicationIpc::Kind::Send)
            && !m_pending.contains(queued.request))
            continue;
        auto packet = MUST(ApplicationIpc::encode_message(
            queued.kind, queued.success, m_byte_epoch, queued.request, queued.wire.bytes()));
        if (m_byte_channel->send(packet.bytes()).is_error()) {
            retire_channel();
            return;
        }
    }
}

void IpcEndpoint::retire_channel()
{
    if (m_byte_failed)
        return;
    m_byte_failed = true;
    m_byte_queue.clear();
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
    auto decoded = ApplicationIpc::decode_message(bytes);
    if (decoded.is_error() || decoded.value().epoch != m_byte_epoch) {
        retire_channel();
        return;
    }

    auto const& packet = decoded.value();
    if (packet.kind != ApplicationIpc::Kind::Reply && packet.kind != ApplicationIpc::Kind::Event) {
        retire_channel();
        return;
    }

    auto wire = ByteString(
        StringView(reinterpret_cast<char const*>(packet.payload.data()), packet.payload.size()));
    if (BridgeData::decode_wire(wire.view(), public_binary_limits()).is_error()) {
        retire_channel();
        return;
    }

    if (packet.kind == ApplicationIpc::Kind::Reply)
        settle(packet.request, packet.success, wire);
    else {
        if (packet.request <= m_incoming_last || m_incoming_pending >= 16) {
            retire_channel();
            return;
        }

        m_incoming_last = packet.request;
        ++m_incoming_pending;
        receive_event(packet.request, move(wire));
    }
}

}
