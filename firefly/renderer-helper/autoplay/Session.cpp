/** The authenticated main-process session is the only source of autoplay configuration. */
#include "Session.h"
#include "renderer-bridge/autoplay/Policy.h"

namespace Firefly::HelperAutoplay {
ErrorOr<void> extract(Web::WebDriver::BiDiSessionState& state)
{
    // At most 256 policies, 256 canvas records and 16 actual preload scripts.
    if (state.preload_scripts.size() > 528)
        return Error::from_string_literal("Autoplay/preload configuration budget exceeded");
    Vector<Autoplay::Configuration> configurations;
    for (auto const& record : state.preload_scripts) {
        if (record.id != "firefly.autoplay.v1"sv)
            continue;
        if (!record.function_declaration.is_empty() || !record.contexts.has_value()
            || record.contexts->size() != 1 || record.sandbox.has_value()
            || !record.user_contexts.is_empty() || !record.arguments.is_object())
            return Error::from_string_literal("Invalid metadata-only autoplay record");
        auto const& data = record.arguments.as_object();
        auto generation = data.get_u32("generation"sv);
        auto name = data.get_string("policy"sv);
        if (data.size() != 2 || !generation.has_value() || !name.has_value()
            || configurations.size() >= 256)
            return Error::from_string_literal("Invalid autoplay arguments");
        Autoplay::Policy policy;
        if (*name == "upstream-default"sv)
            policy = Autoplay::Policy::UpstreamDefault;
        else if (*name == "no-user-gesture-required"sv)
            policy = Autoplay::Policy::NoUserGestureRequired;
        else if (*name == "user-gesture-required"sv)
            policy = Autoplay::Policy::UserGestureRequired;
        else
            return Error::from_string_literal("Unsupported autoplay policy");
        TRY(configurations.try_append({ record.contexts->first(), *generation, policy }));
    }
    // Validate/apply the entire snapshot before stripping any records.
    TRY(Autoplay::apply(move(configurations)));
    state.preload_scripts.remove_all_matching(
        [](auto const& record) { return record.id == "firefly.autoplay.v1"sv; });
    return {};
}
}
