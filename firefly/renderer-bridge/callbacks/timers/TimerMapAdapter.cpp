/**
 * Releases source-owned native timers during world revocation. Timer cleanup must not cancel
 * document/page timers owned by a different world.
 */

// Preserve the original class declaration and every timer algorithm; only member
// accesses in this translation unit use our map view to observe entry changes.
#include "callbacks/timers/TimerMapAdapter.h"
#define m_timers Firefly::Timers::map_view(m_timers, *this)
#include <LibWeb/HTML/WindowOrWorkerGlobalScope.cpp>
#undef m_timers
