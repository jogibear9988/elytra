/** Two opt-in owner-thread observations, driven only by existing native sweep timers. */
#pragma once
#include "Diagnostics.h"
#include <AK/Time.h>
#include <AK/Types.h>
#include <cstdio>

namespace Firefly {
class QueueSnapshotClock {
public:
    unsigned due()
    {
        if (!ipc_bytes_diagnostics_enabled())
            return 0;
        auto elapsed = MonotonicTime::now() - m_started;
        if (!(m_emitted & 1) && elapsed >= AK::Duration::from_seconds(5)) {
            m_emitted |= 1;
            return 5;
        }
        if (!(m_emitted & 2) && elapsed >= AK::Duration::from_seconds(30)) {
            m_emitted |= 2;
            return 30;
        }
        return 0;
    }

private:
    MonotonicTime m_started { MonotonicTime::now() };
    u8 m_emitted {};
};
}
