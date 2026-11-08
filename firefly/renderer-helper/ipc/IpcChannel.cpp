/** Admit one raw endpoint for the exact live configured preload; duplicates cannot revive it. */
#include "IpcEndpoint.h"
#include "native/ipc/application/Handshake.h"
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
    if (transport.is_error())
        return;
    auto weak = make_weak_ptr();
    auto created = Bytes::Channel::create(transport.release_value(),
        { // Application packet routing is a separate step. Refuse unexpected input
          // now rather than install a raw echo service in an application renderer.
            .on_data =
                [weak](ReadonlyBytes) {
                    if (auto owner = weak.strong_ref(); owner && owner->m_byte_channel)
                        owner->m_byte_channel->close();
                },
            .on_writable = {},
            .on_closed =
                [weak](auto) {
                    if (auto owner = weak.strong_ref())
                        owner->m_byte_channel = nullptr;
                } });
    if (created.is_error())
        return;
    m_byte_channel = created.release_value();
    auto hello = ApplicationIpc::encode_hello(
        { binding.epoch, static_cast<u64>(Core::System::getpid()), binding.document_namespace,
            binding.document_local, binding.navigable_namespace, binding.navigable_local });
    if (m_byte_channel->send(hello.span()).is_error()) {
        m_byte_channel->close();
        m_byte_channel = nullptr;
    }
}
}
