/** Bind trusted control handoffs to the actual receiving connection/page/navigation. */
#include "Session.h"
#include "loading/protocol/Client.h"
#include <LibCore/System.h>
#include <LibIPC/FireflyBootstrap.h>
#include <LibURL/Parser.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/ConnectionFromClient.h>
#include <WebContent/PageClient.h>
#include <WebContent/PageHost.h>

namespace Firefly::Resources {
namespace {
    struct Record {
        GC::Root<Web::HTML::LocalNavigable> navigable;
        Utf16String navigation;
        RefPtr<ProtocolFetch::Client> client;
        MonotonicTime admitted;
        bool started { false };
    };

    struct Session {
        HashMap<Web::HTML::CrossProcessId, Record> records;
        OwnPtr<ProtocolFetch::Registration> provider;
        RefPtr<Core::Timer> timer;
        AK::ThreadID thread { AK::ThreadID::current() };
        u64 last_epoch { 0 };
    };

    Session* session;

    void sweep()
    {
        Vector<Web::HTML::CrossProcessId> destroyed;
        for (auto& entry : session->records) {
            auto& record = entry.value;
            if (record.navigable->has_been_destroyed()) {
                if (record.client)
                    record.client->revoke();
                destroyed.append(entry.key);
                continue;
            }

            if (record.client
                && (record.client->is_revoked() || (record.started && !record.client->pending())
                    || (!record.started
                        && MonotonicTime::now() - record.admitted
                            >= AK::Duration::from_seconds(5)))) {
                record.client->revoke();
                record.client = nullptr;
            }
        }

        for (auto id : destroyed)
            session->records.remove(id);
    }

    ErrorOr<NonnullRefPtr<ProtocolFetch::Response>> fetch(
        Web::Fetch::Infrastructure::FetchParams const& params)
    {
        if (!session || !session->thread.is_current_thread())
            return Error::from_string_literal("Native navigation service is unavailable");
        auto const* id = params.request()
                             ->traversable_for_user_prompts()
                             .get_pointer<Web::HTML::CrossProcessId>();
        auto found = id ? session->records.find(*id) : session->records.end();
        if (found == session->records.end() || !found->value.client)
            return Error::from_string_literal("No native channel for this navigation");
        found->value.started = true;
        auto client = found->value.client;
        return client->fetch(params);
    }

    void receive(IPC::Stub& stub, IPC::TransportHandle handle, Bytes::NavigationBinding binding)
    {
        if (!session || !session->thread.is_current_thread()
            || binding.epoch <= session->last_epoch)
            return;
        // Installed only in the owned WebContent entry. Its NotRouted server
        // stub is the real ConnectionFromClient, never a page-provided object.
        auto& connection = static_cast<WebContent::ConnectionFromClient&>(
            static_cast<WebContentServerStub&>(stub));
        auto page = connection.page_host().page(Web::PageId { binding.page });
        if (!page.has_value())
            return;
        Web::HTML::CrossProcessId id { binding.navigable_namespace, binding.navigable_local };
        auto* navigable
            = as_if<Web::HTML::LocalNavigable>(page->page().navigable_with_id(id).ptr());
        if (!navigable)
            return;
        sweep();
        auto previous = session->records.find(id);
        if ((previous != session->records.end() && previous->value.navigation == binding.navigation)
            || (previous == session->records.end() && session->records.size() >= 32))
            return;
        auto url = URL::Parser::basic_parse(binding.url);
        if (!url.has_value())
            return;
        auto intent = ProtocolFetch::Navigation::create(*navigable, *url, binding.navigation);
        if (intent.is_error())
            return;
        auto transport = handle.create_transport();
        if (transport.is_error())
            return;
        auto client = ProtocolFetch::Client::create_navigation(transport.release_value(),
            binding.epoch, *navigable, url.release_value(), binding.navigation);
        if (client.is_error())
            return;
        if (!session->provider) {
            auto provider = ProtocolFetch::Registration::install({}, fetch);
            if (provider.is_error())
                return;
            session->provider = provider.release_value();
        }

        if (previous != session->records.end() && previous->value.client)
            previous->value.client->revoke();
        auto record = Record { GC::Root { *navigable }, move(binding.navigation),
            client.release_value(), MonotonicTime::now() };
        if (session->records.try_set(id, move(record)).is_error())
            return;
        session->last_epoch = binding.epoch;
        dbgln("FIREFLY_NAVIGATION_CHANNEL_READY pid={} epoch={}", Core::System::getpid(),
            binding.epoch);
    }
}

void initialize()
{
    VERIFY(!session);
    session = new Session;
    Bytes::set_navigation_receiver(receive);
    session->timer = Core::Timer::create_repeating(AK::Duration::from_milliseconds(250), [] {
        if (session)
            sweep();
    });
    session->timer->start();
}

void shutdown()
{
    Bytes::set_navigation_receiver({});
    if (!session)
        return;
    session->timer->stop();
    if (session->provider)
        session->provider->revoke();
    for (auto& entry : session->records)
        if (entry.value.client)
            entry.value.client->revoke();
    delete session;
    session = nullptr;
}
}
