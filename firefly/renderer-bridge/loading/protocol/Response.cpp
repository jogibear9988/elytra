/** Keep transport authority native; let upstream FetchController and FetchByteChannel own
 * abort/flow control. */
#include "Response.h"
#include "loading/web-request/Gate.h"
#include "native/protocol/files/FilePacket.h"
#include "native/protocol/files/ResponseHead.h"
#include <AK/WeakPtr.h>
#include <LibHTTP/HeaderList.h>
#include <LibWeb/Fetch/Fetching/Checks.h>
#include <LibWeb/Fetch/Infrastructure/HTTP/Responses.h>

namespace Firefly::ProtocolFetch {
namespace Engine = Web::Fetch::Engine;
namespace Infrastructure = Web::Fetch::Infrastructure;

// The gate owns a transport proxy, not the response owner. No gate/channel/native
// owner cycle can retain the VM after its owning document/client drops it.
class Response::Transport final : public Engine::FetchBodyDeliveryGate::Transport {
public:
    explicit Transport(Response& owner)
        : m_owner(owner)
    {
    }

    virtual void resume_body_delivery_up_to(u64 count) override
    {
        if (auto owner = m_owner.strong_ref())
            owner->resume(count);
    }

    virtual void pause_body_delivery() override
    {
        if (auto owner = m_owner.strong_ref())
            owner->pause();
    }

    virtual void stop() override
    {
        if (auto owner = m_owner.strong_ref())
            owner->stop();
    }

private:
    WeakPtr<Response> m_owner;
};

ErrorOr<NonnullRefPtr<Response>> Response::create(Infrastructure::FetchParams const& params,
    u64 epoch, u32 request, Callbacks callbacks, u64 maximum_length)
{
    if (!epoch || !request || !callbacks.credit || !callbacks.cancel || params.is_canceled()
        || params.controller()->task_destination().has<Empty>()
        || maximum_length > Protocols::file_size_limit)
        return Error::from_string_literal("Invalid or cancelled protocol fetch admission");
    auto callback_owner
        = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) CallbackOwner(move(callbacks))));
    auto response = TRY(adopt_nonnull_ref_or_enomem(
        new (nothrow) Response(params, epoch, request, move(callback_owner), maximum_length)));
    response->m_transport = TRY(adopt_nonnull_ref_or_enomem(new (nothrow) Transport(*response)));
    params.controller()->set_body_transport(*response->m_transport);
    return response;
}

Response::Response(Infrastructure::FetchParams const& params, u64 epoch, u32 request,
    NonnullRefPtr<CallbackOwner> callbacks, u64 maximum_length)
    : m_controller(params.controller())
    , m_pending(Web::Fetch::Fetching::PendingResponse::create(params))
    , m_params(params)
    , m_gate(Engine::FetchBodyDeliveryGate::create(
          Engine::FetchBodyDeliveryGate::NavigationHold::None))
    , m_callbacks(move(callbacks))
    , m_epoch(epoch)
    , m_request(request)
    , m_maximum_length(maximum_length)
{
}

Response::~Response()
{
    fail();
}

GC::Ref<Web::Fetch::Fetching::PendingResponse> Response::pending() const
{
    VERIFY(m_pending);
    return *m_pending;
}

void Response::when_loaded(Web::Fetch::Fetching::PendingResponse::Callback callback)
{
    VERIFY(m_pending);
    m_pending->when_loaded(move(callback));
}

void Response::release_vm_state()
{
    m_pending = nullptr;
    m_controller = nullptr;
    m_params = nullptr;
    m_transport = nullptr;
}

void Response::fail()
{
    VERIFY(m_thread == AK::ThreadID::current());
    if (m_finished)
        return;
    m_finished = true;
    WebRequestGate::cancel(*m_params);
    m_gate->detach_transport();
    if (m_channel)
        (void)m_channel->error({ MUST(ByteBuffer::copy("Protocol resource failed"sv.bytes())) });
    if (m_pending && !m_pending->is_resolved())
        m_pending->resolve(m_params->is_canceled()
                ? Infrastructure::Response::appropriate_network_error(*m_params)
                : Infrastructure::Response::network_error(
                      "Protocol resource request failed"_string));
    auto callbacks = exchange(m_callbacks, nullptr);
    release_vm_state();
    if (callbacks && callbacks->callbacks.cancel)
        callbacks->callbacks.cancel();
}

void Response::stop()
{
    NonnullRefPtr protect { *this };
    fail();
}

ErrorOr<void> Response::reject()
{
    fail();
    return Error::from_string_literal("Invalid protocol response frame/state/credit");
}

void Response::resume(u64 count)
{
    VERIFY(m_thread == AK::ThreadID::current());
    NonnullRefPtr protect { *this };
    if (m_finished)
        return;
    if (count > Protocols::file_chunk_limit) {
        fail();
        return;
    }

    m_allowance = count;
    // The gate reports current remaining allowance, not a new additive grant.
    if (count > m_requested) {
        auto additional = count - m_requested;
        m_requested = count;
        auto callbacks = m_callbacks;
        if (callbacks && callbacks->callbacks.credit)
            callbacks->callbacks.credit(additional);
    }
}

void Response::pause()
{
    VERIFY(m_thread == AK::ThreadID::current());
    m_allowance = 0;
}

void Response::finish_head(NonnullRefPtr<Infrastructure::Response> response)
{
    NonnullRefPtr protect { *this };
    if (m_finished || !m_head.has_value())
        return;
    // Reject unauthorized CORS metadata before granting any body credit.
    // The ordinary upstream HTTP fetch path repeats its own CORS/filter checks.
    if (m_params->request()->response_tainting() == Infrastructure::Request::ResponseTainting::CORS
        && !Web::Fetch::Fetching::cors_check(*m_params->request(), *response)) {
        // A policy refusal belongs to this request; the well-formed channel
        // remains usable for other resources from the same document.
        fail();
        return;
    }

    if (!m_head->body_null) {
        m_channel = Engine::FetchByteChannel::create(
            { .low = Protocols::file_chunk_limit / 2, .high = Protocols::file_chunk_limit },
            m_gate->create_channel_sink());
        response->set_body(
            Infrastructure::ResponseBody { NonnullRefPtr { *m_channel }, m_head->content_length });
    }

    m_pending->resolve(response);
    if (m_channel) {
        m_gate->attach_transport(*m_transport);
        m_gate->grant_credit(m_channel->initial_credit());
    }
}

ErrorOr<void> Response::receive(ReadonlyBytes wire)
{
    VERIFY(m_thread == AK::ThreadID::current());
    NonnullRefPtr protect { *this };
    if (m_finished)
        return Error::from_string_literal("Protocol response is already terminal");
    auto decoded = Protocols::decode_file_packet(wire);
    if (decoded.is_error())
        return reject();
    auto const& packet = decoded.value();
    if (packet.epoch != m_epoch || packet.request != m_request)
        return reject();
    using Kind = Protocols::FilePacketKind;
    if (packet.kind == Kind::Cancel) {
        fail();
        return {};
    }

    if (!m_head.has_value()) {
        if (packet.kind != Kind::Metadata)
            return reject();
        StringView text { reinterpret_cast<char const*>(packet.payload.data()),
            packet.payload.size() };
        auto head = Protocols::decode_stream_response_head(text);
        if (head.is_error()) {
            // Legacy descriptor responses keep their original exact wire schema.
            auto file = Protocols::decode_file_response_head(text);
            if (file.is_error())
                return reject();
            auto value = file.release_value();
            head = Protocols::StreamResponseHead { value.request, 200, value.content_length,
                value.head, value.head, move(value.headers) };
        }

        if (head.is_error() || head.value().request != m_request
            || (head.value().content_length.has_value()
                && *head.value().content_length > m_maximum_length)
            || head.value().head != (m_params->request()->method() == "HEAD"sv))
            return reject();
        m_head = head.release_value();
        auto response = Infrastructure::Response::create();
        response->set_status(m_head->status);
        // Custom handlers do not invent a status phrase absent from the wire.
        response->set_status_message(m_head->status == 200 ? "OK" : "");
        response->url_list().append(m_params->request()->current_url());
        for (auto const& header : m_head->headers)
            response->header_list()->append(header);
        auto weak = make_weak_ptr();
        auto continuation = GC::create_function(
            GC::Heap::the(), [weak](NonnullRefPtr<Infrastructure::Response> accepted) {
                auto owner = weak.strong_ref();
                if (!owner || owner->m_finished)
                    return;
                if (accepted->is_network_error()) {
                    owner->fail();
                    return;
                }

                owner->finish_head(move(accepted));
            });
        if (!WebRequestGate::headers(*m_params, response, continuation))
            finish_head(move(response));

        return {};
    }

    if (packet.kind == Kind::Chunk) {
        if (m_head->body_null || !m_channel || packet.offset != m_received
            || packet.payload.size() > m_maximum_length - m_received
            || (m_head->content_length.has_value()
                && packet.payload.size() > *m_head->content_length - m_received)
            || packet.payload.size() > m_requested || packet.payload.size() > m_allowance)
            return reject();
        m_received += packet.payload.size();
        m_requested -= packet.payload.size();
        m_allowance -= packet.payload.size();
        if (m_gate->did_deliver(packet.payload.size())
                != Engine::FetchBodyDeliveryGate::DeliveryResult::Accepted
            || m_channel->write_copy(packet.payload)
                != Engine::FetchByteChannel::WriteResult::Accepted)
            return reject();
        return {};
    }

    if (packet.kind != Kind::Complete || packet.offset != m_received
        || (m_head->body_null && m_received)
        || (!m_head->body_null && m_head->content_length.has_value()
            && m_received != *m_head->content_length))
        return reject();
    m_finished = true;
    m_gate->detach_transport();
    if (m_channel) {
        (void)m_channel->close();
        m_controller->did_complete_network_body(*m_channel);
    }

    m_callbacks = nullptr;
    release_vm_state();
    return {};
}
}
