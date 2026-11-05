// Preserve the original class declaration and every timer algorithm; only member
// accesses in this translation unit use our map view to observe entry changes.
#include "TimerMapAdapter.h"
#define m_timers Firefly::Timers::map_view(m_timers, *this)
#include <LibWeb/HTML/WindowOrWorkerGlobalScope.cpp>
#undef m_timers
