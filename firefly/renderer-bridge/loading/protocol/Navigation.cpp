/** Native intent fields cannot be supplied by page fetch, links, workers or subframes. */
#include "Navigation.h"
#include "native/protocol/FilePolicy.h"
#include "native/protocol/OriginPolicy.h"
#include <LibWeb/DOM/Document.h>
#include <LibWeb/Fetch/Infrastructure/ClientContextSnapshot.h>
#include <LibWeb/HTML/Scripting/Environments.h>

namespace Firefly::ProtocolFetch {
Navigation::Navigation(Web::HTML::LocalNavigable& navigable, URL::URL url, Utf16String id)
    : m_navigable(navigable)
    , m_source(*navigable.active_document())
    , m_url(move(url))
    , m_id(move(id))
{
}

ErrorOr<NonnullOwnPtr<Navigation>> Navigation::create(
    Web::HTML::LocalNavigable& navigable, URL::URL url, Utf16String id)
{
    bool file = url.scheme() == "file"sv && Protocols::file_policy_snapshot().active;
    if (navigable.has_been_destroyed() || !navigable.is_top_level_traversable()
        || !navigable.active_document() || !navigable.active_document()->is_fully_active()
        || (!file && !Protocols::is_standard_scheme(url.scheme()))
        || (!file && url.origin().is_opaque()) || id.is_empty() || id.length_in_code_units() > 1024)
        return Error::from_string_literal("Invalid native navigation intent");
    auto owner = TRY(
        adopt_nonnull_own_or_enomem(new (nothrow) Navigation(navigable, move(url), move(id))));
    if (!owner->active())
        return Error::from_string_literal("Native navigation is not current");
    return owner;
}

bool Navigation::active() const
{
    if (m_navigable->has_been_destroyed() || !m_navigable->is_top_level_traversable())
        return false;
    auto document = m_navigable->active_document();
    if (!document || !document->is_fully_active())
        return false;
    if (document.ptr() != m_source.ptr()) {
        if (!m_used || !document->url().equals(m_url, URL::ExcludeFragment::Yes)
            || (m_url.scheme() == "file"sv ? !document->origin().is_file_origin()
                                           : !document->origin().is_same_origin(m_url.origin()))
            || (m_destination && document.ptr() != m_destination.ptr()))
            return false;
        m_destination = *document;
    } else if (m_destination)
        return false;
    auto ongoing = m_navigable->ongoing_navigation();
    if (auto const* id = ongoing.get_pointer<Utf16String>())
        return *id == m_id;
    // A committed navigation may clear its ongoing ID while its body still
    // belongs to the destination. Do not require the source document to survive.
    return m_used && ongoing.has<Empty>() && m_destination;
}

bool Navigation::admit(Web::Fetch::Infrastructure::FetchParams const& params)
{
    if (m_used || !active() || !m_source->is_fully_active()
        || m_navigable->active_document().ptr() != m_source.ptr()
        || !allows_navigation_request(params))
        return false;
    auto const& request = *params.request();
    auto const& reserved = *request.reserved_client_snapshot();
    auto const* traversable
        = request.traversable_for_user_prompts().get_pointer<Web::HTML::CrossProcessId>();
    if (!traversable || *traversable != m_navigable->id()
        || request.replaces_client_id() != m_source->relevant_settings_object().id
        || !request.current_url().equals(m_url, URL::ExcludeFragment::Yes)
        || !reserved.creation_url.equals(m_url, URL::ExcludeFragment::Yes)
        || !reserved.top_level_creation_url.has_value()
        || !reserved.top_level_creation_url->equals(m_url, URL::ExcludeFragment::Yes))
        return false;
    m_used = true;
    return true;
}
}
