/** Resolve actual local fetch settings; URL/origin equality cannot authorize another document. */
#include "Documents.h"
#include "native/protocol/FilePolicy.h"
#include "native/protocol/OriginPolicy.h"
#include <LibCore/System.h>
#include <LibURL/Parser.h>
#include <LibWeb/Fetch/Fetching/ClientContextSnapshots.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <WebContent/ConnectionFromClient.h>
#include <WebContent/PageClient.h>
#include <WebContent/PageHost.h>

namespace Firefly::Resources {
ErrorOr<void> Documents::receive(WebContent::ConnectionFromClient& connection,
    IPC::TransportHandle handle, Bytes::DocumentBinding binding)
{
    sweep();
    Web::HTML::CrossProcessId id { binding.document_namespace, binding.document_local };
    auto page = connection.page_host().page(Web::PageId { binding.navigation.page });
    auto url = URL::Parser::basic_parse(binding.navigation.url);
    bool intercept = Protocols::file_policy_snapshot().active;
    if (!page.has_value() || !url.has_value()
        || (!intercept && !Protocols::is_standard_scheme(url->scheme()))
        || (url->scheme() != "file"sv && url->origin().is_opaque())
        || (!m_records.contains(id) && m_records.size() >= 32))
        return Error::from_string_literal("Document channel outside admitted page/URL/budget");
    // A grant may precede document creation or refresh that exact committed ID.
    // Neither a same-origin neighbor nor a URL-only match can select its owner.
    auto transport = TRY(handle.create_transport());
    auto previous = m_records.find(id);
    if (previous != m_records.end()) {
        if (previous->value.binding.navigation.epoch >= binding.navigation.epoch)
            return Error::from_string_literal("Native document refresh epoch regressed");
        if (previous->value.client)
            previous->value.client->revoke();
        m_records.remove(id);
    }

    TRY(m_records.try_set(id,
        Record { move(binding), url.release_value(), move(transport), nullptr, {},
            MonotonicTime::now() }));
    return {};
}

ErrorOr<NonnullRefPtr<ProtocolFetch::Response>> Documents::fetch(
    Web::Fetch::Infrastructure::FetchParams const& params)
{
    if (!ProtocolFetch::allows_request(params))
        return Error::from_string_literal("Ineligible native document resource request");
    auto settings = Web::Fetch::Fetching::resolve_client(*params.request());
    auto* window
        = settings ? Web::HTML::window_from_global_object(settings->global_object()) : nullptr;
    if (!window)
        return Error::from_string_literal("Resource client is not a local Window");
    auto& document = window->associated_document();
    auto found = m_records.find(document.id());
    if (found == m_records.end())
        return Error::from_string_literal("No native grant for this document identity");
    auto& record = found->value;
    auto const& binding = record.binding.navigation;
    auto navigable = document.navigable();
    if (!document.is_fully_active() || !navigable || !navigable->is_top_level_traversable()
        || navigable->id()
            != Web::HTML::CrossProcessId { binding.navigable_namespace, binding.navigable_local }
        || document.page().client().id().value() != binding.page
        || !document.url().equals(record.url, URL::ExcludeFragment::Yes)
        || (record.url.scheme() == "file"sv
                ? !document.origin().is_file_origin()
                : !document.origin().is_same_origin(record.url.origin())))
        return Error::from_string_literal("Native document identity is not active/current");
    if (!record.client) {
        if (!record.transport)
            return Error::from_string_literal("Native document channel is retired");
        auto client = TRY(ProtocolFetch::Client::create(
            record.transport.release_nonnull(), binding.epoch, document));
        record.document = document;
        record.client = move(client);
        dbgln("FIREFLY_DOCUMENT_CHANNEL_READY pid={} epoch={}", Core::System::getpid(),
            binding.epoch);
    }

    auto client = record.client; // Delivery can revoke its native owner reentrantly.
    return client->fetch(params);
}

void Documents::sweep()
{
    Vector<Web::HTML::CrossProcessId> retired;
    for (auto& entry : m_records) {
        auto& record = entry.value;
        if ((record.document && !record.document->is_fully_active())
            || (!record.document
                && MonotonicTime::now() - record.admitted >= AK::Duration::from_seconds(5))) {
            if (record.client)
                record.client->revoke();
            retired.append(entry.key);
        }
    }

    for (auto id : retired)
        m_records.remove(id);
}

void Documents::revoke()
{
    for (auto& entry : m_records)
        if (entry.value.client)
            entry.value.client->revoke();
    m_records.clear();
}
}
