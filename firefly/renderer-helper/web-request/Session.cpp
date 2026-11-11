/** Typed bounded control messages; never expose a setter or byte endpoint to a page. */
#include "Session.h"
#include "loading/web-request/Gate.h"
#include "native/ipc/bytes/Channel.h"
#include "native/platform/QueueSnapshots.h"
#include "native/web-request/Limits.h"
#include "native/web-request/Packet.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <LibCore/System.h>
#include <LibCore/Timer.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/PageClient.h>

namespace Firefly::HelperWebRequest {
namespace {
    using Packet = WebRequest::Packet;

    struct Pending {
        Packet identity;
        WebRequestGate::Completion completion;
        MonotonicTime deadline;
        GC::Root<Web::DOM::Document> document;
        size_t bytes;
    };

    struct Session {
        AK_ALLOC_WITH_KMALLOC;
        RefPtr<Bytes::Channel> channel;
        RefPtr<Core::Timer> timer;
        HashMap<u32, Pending> pending;
        HashMap<u32, Packet> logical;
        u32 next {};
        size_t bytes {};
        Vector<ByteBuffer> outgoing;
        size_t outgoing_bytes {};
        QueueSnapshotClock diagnostics;
        bool admitted {};
        bool stopped {};
    };

    NeverDestroyed<OwnPtr<Session>> session;

    bool same(Packet const& a, Packet const& b)
    {
        return a.epoch == b.epoch && a.request == b.request && a.flags == b.flags
            && a.page == b.page && a.navigable_namespace == b.navigable_namespace
            && a.navigable_local == b.navigable_local
            && a.document_namespace == b.document_namespace && a.document_local == b.document_local;
    }

    void flush()
    {
        if (!*session || !session.get()->channel || !session.get()->channel->is_open())
            return;
        while (!session.get()->outgoing.is_empty()
            && session.get()->channel->pending_send_count() < Bytes::max_pending) {
            auto sent = session.get()->channel->send(session.get()->outgoing.first());
            if (sent.is_error()) {
                session.get()->channel->close();
                return;
            }

            session.get()->outgoing_bytes -= session.get()->outgoing.first().size();
            session.get()->outgoing.remove(0);
        }
    }

    ErrorOr<void> queue(ByteBuffer bytes)
    {
        if (!*session || !session.get()->channel || !session.get()->channel->is_open()
            || session.get()->outgoing.size() >= 2048
            || session.get()->bytes + session.get()->outgoing_bytes > 2 * 1024 * 1024
            || bytes.size()
                > 2 * 1024 * 1024 - session.get()->bytes - session.get()->outgoing_bytes)
            return Error::from_string_literal("webRequest egress budget exhausted");
        session.get()->outgoing_bytes += bytes.size();
        session.get()->outgoing.append(move(bytes));
        flush();
        return {};
    }

    void send(Packet const& packet)
    {
        if (!*session || !session.get()->channel)
            return;
        auto encoded = WebRequest::encode_packet(packet);
        if (encoded.is_error() || queue(encoded.release_value()).is_error())
            session.get()->channel->close();
    }

    void retire(u32 id, bool notify)
    {
        auto found = session.get()->pending.find(id);
        if (found == session.get()->pending.end())
            return;
        auto pending = session.get()->pending.take(id).release_value();
        session.get()->bytes -= pending.bytes;
        if (notify) {
            pending.identity.kind = WebRequest::Kind::Cancel;
            pending.identity.payload = {};
            send(pending.identity);
        }

        pending.completion({});
    }

    void fail_all()
    {
        if (!*session)
            return;
        Vector<u32> ids;
        for (auto const& entry : session.get()->pending)
            ids.append(entry.key);
        for (auto id : ids)
            retire(id, false);
        session.get()->logical.clear();
        session.get()->outgoing.clear();
        session.get()->outgoing_bytes = 0;
    }

    ErrorOr<u32> submit(WebRequestGate::Stage stage, Web::DOM::Document& document,
        JsonObject metadata, WebRequestGate::Completion completion)
    {
        if (!*session || session.get()->stopped || !session.get()->channel
            || !session.get()->channel->is_open() || session.get()->pending.size() >= 16
            || session.get()->next == 2147483647)
            return Error::from_string_literal(
                "webRequest control channel unavailable or exhausted");
        auto* page = as_if<WebContent::PageClient>(document.page().client());
        auto nav = document.navigable();
        if (!page || !nav || !document.is_fully_active())
            return Error::from_string_literal("webRequest source is not an owned active document");
        auto policy = WebRequestGate::policy();
        auto doc = document.id();
        Packet packet { .kind = stage == WebRequestGate::Stage::Before ? WebRequest::Kind::Before
                                                                       : WebRequest::Kind::Headers,
            .flags = static_cast<u8>(stage == WebRequestGate::Stage::Headers ? 1 : 0),
            .epoch = policy.epoch,
            .request = session.get()->next + 1,
            .page = page->id().value(),
            .navigable_namespace = nav->id().namespace_id,
            .navigable_local = nav->id().local_id,
            .document_namespace = doc.namespace_id,
            .document_local = doc.local_id,
            .payload = move(metadata) };
        auto logical = packet.payload.get_u32("logicalId"sv);
        if (!logical.has_value() || !*logical)
            return Error::from_string_literal("webRequest logical identity budget exceeded");
        if (session.get()->logical.size() >= WebRequest::logical_request_limit
            && !session.get()->logical.contains(*logical)) {
            WebRequest::logical_limit_diagnostic(
                WebRequest::LogicalLimitStage::Session, session.get()->logical.size());
            return Error::from_string_literal("webRequest logical identity budget exceeded");
        }
        auto encoded = TRY(WebRequest::encode_packet(packet));
        if (encoded.size() > 2 * 1024 * 1024 - session.get()->bytes)
            return Error::from_string_literal("webRequest queued-byte budget exceeded");
        auto id = packet.request;
        session.get()->next = id;
        Packet identity = packet;
        identity.payload = {};
        session.get()->logical.set(*logical, identity);
        WebRequest::logical_retention_diagnostic(
            WebRequest::LogicalLimitStage::Session, session.get()->logical.size());
        session.get()->bytes += encoded.size();
        session.get()->pending.set(id,
            Pending { identity, move(completion),
                MonotonicTime::now() + AK::Duration::from_seconds(30), document, encoded.size() });
        auto sent = queue(move(encoded));
        if (sent.is_error())
            retire(id, false);
        return id;
    }

    void receive(ReadonlyBytes bytes)
    {
        auto decoded = WebRequest::decode_packet(bytes);
        if (decoded.is_error()) {
            session.get()->channel->close();
            return;
        }

        auto packet = decoded.release_value();
        if (packet.kind == WebRequest::Kind::Policy) {
            auto before = packet.payload.get_bool("before"sv),
                 headers = packet.payload.get_bool("headers"sv);
            if (packet.payload.size() != 2 || !before.has_value() || !headers.has_value()
                || !WebRequestGate::configure({ packet.epoch, *before, *headers }))
                session.get()->channel->close();
            return;
        }

        if (packet.kind != WebRequest::Kind::Decision) {
            session.get()->channel->close();
            return;
        }

        auto found = session.get()->pending.find(packet.request);
        if (found == session.get()->pending.end())
            return; // Retired timeout/generation; cannot revive it.
        if (!same(found->value.identity, packet)) {
            session.get()->channel->close();
            return;
        }

        auto pending = session.get()->pending.take(packet.request).release_value();
        session.get()->bytes -= pending.bytes;
        pending.completion(move(packet.payload));
    }
}

void initialize()
{
    *session = make<Session>();
    WebRequestGate::install(
        submit,
        [](u32 id) {
            if (*session)
                retire(id, true);
        },
        [](u32 id) {
            if (!*session)
                return;
            auto found = session.get()->logical.take(id);
            if (!found.has_value())
                return;
            auto packet = found.release_value();
            packet.kind = WebRequest::Kind::Finish;
            packet.request = id;
            packet.flags = 0;
            packet.payload = {};
            send(packet);
        });
    session.get()->timer = Core::Timer::create_repeating(AK::Duration::from_milliseconds(100), [] {
        if (!*session)
            return;
        if (auto after = session.get()->diagnostics.due())
            std::fprintf(stderr,
                "FIREFLY_QUEUE_SNAPSHOT stage=web-request-session after=%u pending=%zu "
                "logical=%zu pending_bytes=%zu outgoing=%zu outgoing_bytes=%zu open=%u\n",
                after, session.get()->pending.size(), session.get()->logical.size(),
                session.get()->bytes, session.get()->outgoing.size(), session.get()->outgoing_bytes,
                session.get()->channel && session.get()->channel->is_open() ? 1u : 0u);
        WebRequestGate::sweep();
        Vector<u32> expired;
        for (auto const& entry : session.get()->pending)
            if (MonotonicTime::now() >= entry.value.deadline
                || !entry.value.document->is_fully_active())
                expired.append(entry.key);
        for (auto id : expired)
            retire(id, true);
    });
    session.get()->timer->start();
    Bytes::set_bootstrap_receiver([](IPC::TransportHandle handle, Bytes::BootstrapCookie cookie) {
        if (!*session || session.get()->stopped || session.get()->admitted)
            return;
        session.get()->admitted = true;
        auto transport = handle.create_transport();
        if (transport.is_error())
            return;
        auto channel = Bytes::Channel::create(transport.release_value(),
            { .on_data = receive, .on_writable = flush, .on_closed = [](auto) { fail_all(); } });
        if (channel.is_error())
            return;
        session.get()->channel = channel.release_value();
        Packet hello { .kind = WebRequest::Kind::Hello, .payload = {} };
        hello.payload.set("cookieHigh"sv, MUST(String::formatted("{}", cookie.high)));
        hello.payload.set("cookieLow"sv, MUST(String::formatted("{}", cookie.low)));
        hello.payload.set("pid"sv, Core::System::getpid());
        send(hello);
    });
}

void shutdown()
{
    if (!*session)
        return;
    session.get()->stopped = true;
    Bytes::set_bootstrap_receiver({});
    WebRequestGate::shutdown();
    if (session.get()->timer)
        session.get()->timer->stop();
    if (session.get()->channel)
        session.get()->channel->close();
    fail_all();
    session->clear();
}
}
