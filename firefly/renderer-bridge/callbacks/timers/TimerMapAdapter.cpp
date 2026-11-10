/**
 * Releases source-owned native timers during world revocation. Timer cleanup must not cancel
 * document/page timers owned by a different world.
 */

// Preserve the original class declaration and every timer algorithm. The generated
// composition rewrites complete expressions, including explicit lambda owners;
// an identifier macro would mis-expand self.m_timers and captureless callbacks.
#include "callbacks/timers/TimerMapAdapter.h"
#include <WindowOrWorkerGlobalScope.inc>
