/** Native admission for a real Rust AudioContext; no DOM/script activation surrogate. */
#pragma once
#include <AK/Types.h>
#include <LibWeb/Export.h>

extern "C" WEB_API bool firefly_web_audio_may_start(void* global_event_target, u64* authorization);

// Revalidate queued work against current native ownership/policy, without sampling activation.
extern "C" WEB_API bool firefly_web_audio_still_authorized(
    void* global_event_target, u64 authorization);
