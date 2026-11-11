#include "Audio.h"
#include "Policy.h"
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Navigable.h>
#include <LibWeb/HTML/Window.h>

static bool admit(void* global_event_target, u64* authorization, bool sample_activation)
{
    if (!authorization)
        return false;
    auto previous_authorization = *authorization;
    *authorization = 0;
    if (!global_event_target)
        return false;
    auto* target = static_cast<Web::DOM::EventTarget*>(global_event_target);
    if (!is<Web::HTML::Window>(*target))
        return false;
    auto& window = static_cast<Web::HTML::Window&>(*target);
    auto document = window.associated_document_if_any();
    if (!document || !document->is_fully_active())
        return false;
    auto navigable = document->navigable();
    if (!navigable || navigable->has_been_destroyed() || !navigable->active_document_is(*document))
        return false;
    auto policy = Firefly::Autoplay::policy_for(*document);
    if (policy == Firefly::Autoplay::Policy::Blocked)
        return false;
    if (!policy.has_value() || *policy == Firefly::Autoplay::Policy::UpstreamDefault
        || *policy == Firefly::Autoplay::Policy::NoUserGestureRequired)
        return true;
    auto revision = Firefly::Autoplay::revision_for(*document);
    if (!revision)
        return false;
    if (previous_authorization == revision) {
        *authorization = revision;
        return true;
    }
    auto top = navigable->top_level_traversable();
    if (top->has_been_destroyed())
        return false;
    auto origin = top->active_document_origin();
    if (!origin.has_value())
        return false;
    // Chromium 156 AudioContext::UpdateAutoplayRequirement gates only frames
    // cross-origin to the outermost main frame for user-gesture-required.
    // AreAutoplayRequirementsFulfilled uses transient activation, sampled only
    // by constructor/resume/source-start. An unrelated activation never resumes
    // a suspended context; a successful context admission is latched by Rust.
    bool allowed = document->origin().is_same_origin(*origin)
        || (sample_activation && window.has_transient_activation());
    if (allowed)
        *authorization = revision;
    return allowed;
}

bool firefly_web_audio_may_start(void* global_event_target, u64* authorization)
{
    return admit(global_event_target, authorization, true);
}

bool firefly_web_audio_still_authorized(void* global_event_target, u64 authorization)
{
    return admit(global_event_target, &authorization, false);
}
