/** Use the real native context handle, retaining author CSS and ordinary iframe painting. */
#include "Canvas.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <AK/ThreadID.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Element.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/WebDriver/BiDi/BiDi.h>

namespace Firefly::Canvas {
namespace {
    struct Setting {
        u32 generation;
        u32 color;
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

Gfx::Color base(Web::DOM::Document const& document, Gfx::Color fallback)
{
    if (!state().owner.is_current_thread())
        return fallback;
    auto navigable = document.navigable();
    if (!navigable || navigable->has_been_destroyed() || !navigable->is_top_level_traversable()
        || navigable->active_document().ptr() != &document)
        return fallback;
    auto handle = Web::WebDriver::BiDi::navigable_id(*navigable);
    auto found = state().values.find(handle);
    if (found == state().values.end())
        return fallback;
    auto rgb = found->value.color;
    return { static_cast<u8>(rgb >> 16), static_cast<u8>(rgb >> 8), static_cast<u8>(rgb) };
}

ErrorOr<void> apply(Vector<Configuration> configurations)
{
    if (!state().owner.is_current_thread() || configurations.size() > 256)
        return Error::from_string_literal("Canvas configuration outside owner/budget");
    HashMap<String, Setting> next;
    for (auto& configuration : configurations) {
        if (configuration.context.is_empty() || configuration.context.byte_count() > 1024
            || configuration.context.contains('\0') || !configuration.generation
            || configuration.generation > 2147483647 || configuration.color > 0xffffff
            || next.contains(configuration.context))
            return Error::from_string_literal("Invalid/duplicate native canvas configuration");
        auto old = state().values.find(configuration.context);
        if (old != state().values.end()
            && (old->value.generation > configuration.generation
                || (old->value.generation == configuration.generation
                    && old->value.color != configuration.color)))
            return Error::from_string_literal("Stale/conflicting native canvas generation");
        TRY(next.try_set(move(configuration.context),
            Setting { configuration.generation, configuration.color }));
    }

    auto previous = move(state().values);
    state().values
        = move(next); // Commit the complete bounded snapshot before requesting new paint.
    for (auto const& navigable : Web::HTML::all_local_navigables()) {
        if (navigable->has_been_destroyed() || !navigable->is_top_level_traversable()
            || !navigable->active_document())
            continue;
        auto handle = Web::WebDriver::BiDi::navigable_id(*navigable);
        auto old = previous.find(handle), current = state().values.find(handle);
        bool changed = (old == previous.end()) != (current == state().values.end());
        if (old != previous.end() && current != state().values.end())
            changed = old->value.color != current->value.color;
        if (!changed)
            continue;
        auto document = navigable->active_document();
        // Public native repaint seams invalidate paint commands and request a
        // frame. No CSS property, element attribute or principal-world JS changes.
        document->set_needs_to_record_display_list();
        navigable->set_needs_to_record_display_list();
        navigable->set_needs_repaint();
        if (auto* element = document->document_element())
            element->set_needs_repaint();
    }

    return {};
}

void shutdown()
{
    if (state().owner.is_current_thread())
        state().values.clear();
}
}
