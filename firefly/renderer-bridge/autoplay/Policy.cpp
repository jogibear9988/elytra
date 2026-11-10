/** Resolve policies from native traversable identities, including remote top-level roots. */
#include "Policy.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <AK/ThreadID.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalNavigable.h>

namespace Firefly::Autoplay {
namespace {
    struct Setting {
        u32 generation;
        Policy policy;
    };

    struct State {
        AK::ThreadID owner { AK::ThreadID::current() };
        HashMap<String, Setting> values;
    };

    State& state()
    {
        static NeverDestroyed<State> value;
        return *value;
    }
}

ErrorOr<void> apply(Vector<Configuration> configurations)
{
    auto& registry = state();
    if (!registry.owner.is_current_thread() || configurations.size() > 256)
        return Error::from_string_literal("Autoplay configuration outside owner/budget");
    HashMap<String, Setting> next;
    for (auto& configuration : configurations) {
        if (configuration.context.is_empty() || configuration.context.byte_count() > 1024
            || configuration.context.contains('\0') || !configuration.generation
            || configuration.generation > 2147483647 || next.contains(configuration.context)
            || (configuration.policy != Policy::NoUserGestureRequired
                && configuration.policy != Policy::UserGestureRequired))
            return Error::from_string_literal("Invalid/duplicate native autoplay configuration");
        auto old = registry.values.find(configuration.context);
        if (old != registry.values.end()
            && (old->value.generation > configuration.generation
                || (old->value.generation == configuration.generation
                    && old->value.policy != configuration.policy)))
            return Error::from_string_literal("Stale/conflicting native autoplay generation");
        TRY(next.try_set(move(configuration.context),
            Setting { configuration.generation, configuration.policy }));
    }

    registry.values = move(next);
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
        return {};
    auto& registry = state();
    auto found = registry.values.find(*context);
    if (found == registry.values.end())
        return {};
    return found->value.policy;
}

void shutdown()
{
    if (state().owner.is_current_thread())
        state().values.clear();
}
}
