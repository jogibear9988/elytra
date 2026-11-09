/** This native configuration never reaches a script realm or the upstream preload executor. */
#include "Session.h"
#include "renderer-bridge/canvas/Canvas.h"

namespace Firefly::HelperCanvas {
void initialize()
{
    MUST(Canvas::apply({}));
}

ErrorOr<void> extract(Web::WebDriver::BiDiSessionState& state)
{
    if (state.preload_scripts.size() > 272)
        return Error::from_string_literal("Canvas/preload configuration budget exceeded");
    Vector<Canvas::Configuration> configurations;
    for (auto const& record : state.preload_scripts) {
        if (record.id != "firefly.canvas.v1"sv)
            continue;
        if (!record.function_declaration.is_empty() || !record.contexts.has_value()
            || record.contexts->size() != 1 || record.sandbox.has_value()
            || !record.user_contexts.is_empty() || !record.arguments.is_object())
            return Error::from_string_literal("Invalid metadata-only native canvas record");
        auto const& data = record.arguments.as_object();
        auto generation = data.get_u32("generation"sv), color = data.get_u32("color"sv);
        if (data.size() != 2 || !generation.has_value() || !color.has_value()
            || configurations.size() >= 256)
            return Error::from_string_literal("Invalid native canvas arguments");
        TRY(configurations.try_append(
            Canvas::Configuration { record.contexts->first(), *generation, *color }));
    }

    TRY(Canvas::apply(move(configurations)));
    state.preload_scripts.remove_all_matching(
        [](auto const& record) { return record.id == "firefly.canvas.v1"sv; });
    return {};
}

void shutdown()
{
    Canvas::shutdown();
}
}
