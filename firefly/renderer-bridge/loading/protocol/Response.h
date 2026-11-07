/** Adapt framed file responses to real LibWeb pending responses and bounded body streams. */
#pragma once
#include "native/protocol/files/ResponseHead.h"
#include <AK/Function.h>
#include <AK/ThreadID.h>
#include <AK/Weakable.h>
#include <LibGC/Root.h>
#include <LibWeb/Fetch/Engine/FetchBodyDeliveryGate.h>
#include <LibWeb/Fetch/Fetching/PendingResponse.h>
#include <LibWeb/Fetch/Infrastructure/FetchParams.h>

namespace Firefly::ProtocolFetch {
class WEB_API Response final : public RefCounted<Response>, public Weakable<Response> {
public:
    struct Callbacks {
        // Native transport grants; no JS/page setter is installed by this adapter.
        Function<void(u64)> credit;
        Function<void()> cancel;
    };

    static ErrorOr<NonnullRefPtr<Response>> create(Web::Fetch::Infrastructure::FetchParams const&,
        u64 epoch, u32 request, Callbacks, u64 maximum_length = Protocols::file_size_limit);

    ~Response();

    // Obtain/root this before receiving any frames. Terminal paths release our VM roots.
    GC::Ref<Web::Fetch::Fetching::PendingResponse> pending() const;

    void when_loaded(Web::Fetch::Fetching::PendingResponse::Callback);

    ErrorOr<void> receive(ReadonlyBytes);

    void stop();

    bool finished() const
    {
        return m_finished;
    }

    bool headers_received() const
    {
        return m_head.has_value();
    }

private:
    class Transport;

    struct CallbackOwner : RefCounted<CallbackOwner> {
        explicit CallbackOwner(Callbacks value)
            : callbacks(move(value))
        {
        }

        Callbacks callbacks;
    };

    Response(Web::Fetch::Infrastructure::FetchParams const&, u64, u32, NonnullRefPtr<CallbackOwner>,
        u64);

    ErrorOr<void> reject();

    void fail();

    void release_vm_state();

    void resume(u64);

    void pause();

    GC::Root<Web::Fetch::Infrastructure::FetchController> m_controller;
    GC::Root<Web::Fetch::Fetching::PendingResponse> m_pending;
    RefPtr<Web::Fetch::Infrastructure::FetchParams const> m_params;
    NonnullRefPtr<Web::Fetch::Engine::FetchBodyDeliveryGate> m_gate;
    RefPtr<Web::Fetch::Engine::FetchByteChannel> m_channel;
    RefPtr<Transport> m_transport;
    Optional<Protocols::ResponseHead> m_head;
    RefPtr<CallbackOwner> m_callbacks;
    u64 const m_epoch;
    u32 const m_request;
    u64 const m_maximum_length;
    u64 m_received { 0 };
    u64 m_requested { 0 };
    u64 m_allowance { 0 };
    bool m_finished { false };
    AK::ThreadID m_thread { AK::ThreadID::current() };
};
}
