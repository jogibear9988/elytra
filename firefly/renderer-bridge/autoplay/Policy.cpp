/** Resolve policies from native traversable identities, including remote top-level roots. */
#include "Policy.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <AK/NumericLimits.h>
#include <AK/ThreadID.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>

namespace Firefly::Autoplay {
namespace {
    struct Setting {
        u32 generation;
        Policy policy;
        u64 revision;
    };

    struct State {
        AK::ThreadID owner { AK::ThreadID::current() };
        HashMap<String, Setting> values;
        bool owned { false };
        u64 last_revision { 0 };
    };

    State& state()
    {
        static NeverDestroyed<State> value;
        return *value;
    }
}

void initialize_owned()
{
    VERIFY(state().owner.is_current_thread());
    state().values.clear();
    state().owned = true;
}

ErrorOr<void> apply(Vector<Configuration> configurations)
{
    auto& registry = state();
    if (!registry.owner.is_current_thread() || configurations.size() > 256)
        return Error::from_string_literal("Autoplay configuration outside owner/budget");
    HashMap<String, Setting> next;
    u64 next_revision = registry.last_revision;

    for (auto& configuration : configurations) {
        if (configuration.context.is_empty() || configuration.context.byte_count() > 1024
            || configuration.context.contains('\0') || !configuration.generation
            || configuration.generation > 2147483647 || next.contains(configuration.context)
            || (configuration.policy != Policy::NoUserGestureRequired
                && configuration.policy != Policy::UserGestureRequired
                && configuration.policy != Policy::UpstreamDefault))
            return Error::from_string_literal("Invalid/duplicate native autoplay configuration");
        auto parts = configuration.context.bytes_as_string_view().split_view('-');
        if (parts.size() != 2)
            return Error::from_string_literal("Invalid native autoplay context ID");
        auto namespace_id = parts[0].to_number<u64>();
        auto local_id = parts[1].to_number<u64>();
        if (!namespace_id.has_value() || !local_id.has_value() || !*namespace_id || !*local_id
            || configuration.context != TRY(String::formatted("{}-{}", *namespace_id, *local_id)))
            return Error::from_string_literal("Noncanonical native autoplay context ID");
        auto old = registry.values.find(configuration.context);
        if (old != registry.values.end()
            && (old->value.generation > configuration.generation
                || (old->value.generation == configuration.generation
                    && old->value.policy != configuration.policy)))
            return Error::from_string_literal("Stale/conflicting native autoplay generation");
        u64 revision;
        if (old != registry.values.end() && old->value.policy == configuration.policy)
            revision = old->value.revision;
        else {
            if (next_revision == NumericLimits<u64>::max())
                return Error::from_string_literal("Native autoplay revision exhausted");
            revision = ++next_revision;
        }
        TRY(next.try_set(move(configuration.context),
            Setting { configuration.generation, configuration.policy, revision }));
    }

    registry.values = move(next);
    registry.last_revision = next_revision;
    return {};
}

Optional<String> context_for(Web::DOM::Document const& document)
{
    if (!state().owner.is_current_thread())
        return {};
    auto navigable = document.navigable();
    if (!navigable || navigable->has_been_destroyed())
        return {};
    auto top = navigable->top_level_traversable();
    if (top->has_been_destroyed())
        return {};
    // Unlike BiDi's UUID for local top-level views, this actual native ID is
    // replicated unchanged into RemoteNavigable and identifies both cases.
    auto id = top->id();
    if (!id.namespace_id || !id.local_id)
        return {};
    return MUST(String::formatted("{}-{}", id.namespace_id, id.local_id));
}

Optional<Policy> policy_for(Web::DOM::Document const& document)
{
    auto context = context_for(document);
    if (!context.has_value())
        return state().owned ? Optional<Policy> { Policy::Blocked } : Optional<Policy> {};
    auto& registry = state();
    auto found = registry.values.find(*context);
    if (found == registry.values.end())
        return registry.owned ? Optional<Policy> { Policy::Blocked } : Optional<Policy> {};
    return found->value.policy;
}

u64 revision_for(Web::DOM::Document const& document)
{
    auto context = context_for(document);
    if (!context.has_value())
        return 0;
    auto found = state().values.find(*context);
    return found == state().values.end() ? 0 : found->value.revision;
}

void shutdown()
{
    if (state().owner.is_current_thread()) {
        state().values.clear();
        state().owned = false;
    }
}
}
