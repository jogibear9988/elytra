/**
 * Electron's user-gesture-required is an element lock, not document sticky activation.
 * State contains no GC references: actual element destruction removes its address before reuse.
 */
#include "Media.h"
#include "Policy.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <AK/ThreadID.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/HTMLMediaElement.h>
#include <LibWeb/HTML/HTMLVideoElement.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Navigable.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly::Autoplay {
namespace {
    struct ElementState {
        bool locked { true };
        bool opener_play { false };
        Optional<Policy> previous_policy;
    };

    struct State {
        AK::ThreadID owner { AK::ThreadID::current() };
        HashMap<Web::HTML::HTMLMediaElement const*, ElementState> elements;
    };

    State& state()
    {
        static NeverDestroyed<State> value;
        return *value;
    }

    ElementState& element_state(Web::HTML::HTMLMediaElement const& element, Optional<Policy> policy)
    {
        auto& values = state().elements;
        auto found = values.find(&element);
        if (found == values.end()) {
            values.set(&element, ElementState {});
            found = values.find(&element);
        }

        auto& value = found->value;
        // Newly configured elements and transitions from an unrestricted policy
        // must start locked. A repeated snapshot does not revoke an element unlock.
        if (policy == Policy::UserGestureRequired
            && value.previous_policy != Policy::UserGestureRequired)
            value.locked = true;
        value.previous_policy = policy;
        return value;
    }

    bool transient(Web::DOM::Document const& document)
    {
        auto window = document.window();
        return window && window->has_transient_activation();
    }

    void try_unlock(Web::HTML::HTMLMediaElement& element)
    {
        if (!state().owner.is_current_thread())
            return;
        auto policy = policy_for(element.document());
        if (!policy.has_value())
            return;
        auto& value = element_state(element, policy);
        if (transient(element.document()))
            value.locked = false;
    }
}

void initialize(Web::HTML::HTMLMediaElement& element)
{
    if (state().owner.is_current_thread()) {
        // Initialization is also an address-reuse barrier; never inherit a stale
        // unlock, even if a future SDK lifecycle regression missed destruction.
        state().elements.remove(&element);
        element_state(element, policy_for(element.document()));
    }
}

void destroyed(Web::HTML::HTMLMediaElement& element)
{
    if (state().owner.is_current_thread())
        state().elements.remove(&element);
}

void adopted(Web::HTML::HTMLMediaElement& element, Web::DOM::Document const& old_document)
{
    if (!state().owner.is_current_thread())
        return;
    auto old = policy_for(old_document);
    auto current = policy_for(element.document());
    auto& value = element_state(element, old);
    value.opener_play = false;
    if (current == Policy::UserGestureRequired && old != Policy::UserGestureRequired)
        value.locked = true;
    value.previous_policy = current;
}

void request_play(Web::HTML::HTMLMediaElement& element)
{
    try_unlock(element);
    auto policy = policy_for(element.document());
    if (!state().owner.is_current_thread() || !policy.has_value())
        return;
    auto& value = element_state(element, policy);
    value.opener_play = false;
    // Blink permits this one call when a local opener has transient activation,
    // but only the element's own frame activation permanently unlocks it.
    auto navigable = element.document().navigable();
    auto opener = navigable ? navigable->active_browsing_context_opener_navigable() : nullptr;
    if (opener && !opener->has_been_destroyed() && opener->is_local_navigable()) {
        auto document = static_cast<Web::HTML::LocalNavigable&>(*opener).active_document();
        value.opener_play = document && transient(*document);
    }
}

void request_load(Web::HTML::HTMLMediaElement& element)
{
    try_unlock(element);
}

void request_audible(Web::HTML::HTMLMediaElement& element)
{
    if (element.effective_media_volume() != 0.0)
        try_unlock(element);
}

void native_play(Web::HTML::HTMLMediaElement& element)
{
    if (!state().owner.is_current_thread())
        return;
    auto policy = policy_for(element.document());
    if (policy.has_value())
        element_state(element, policy).locked = false;
}

Optional<bool> allowed(Web::HTML::HTMLMediaElement const& element)
{
    if (!state().owner.is_current_thread())
        return {};
    auto policy = policy_for(element.document());
    if (!policy.has_value() || *policy == Policy::UpstreamDefault)
        return {};
    if (*policy == Policy::Blocked)
        return false;
    auto& value = element_state(element, policy);
    bool opener_play = value.opener_play;
    value.opener_play = false; // Consumed by the immediate play() admission check only.
    if (*policy == Policy::NoUserGestureRequired || !value.locked || opener_play)
        return true;
    // Muted audio is not Blink's muted-video autoplay exception.
    return is<Web::HTML::HTMLVideoElement>(element) && element.effective_media_volume() == 0.0;
}
}
