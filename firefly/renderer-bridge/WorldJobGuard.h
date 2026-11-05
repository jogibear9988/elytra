#pragma once
#include "IsolatedWorld.h"
#include <LibJS/Runtime/VM.h>

namespace Firefly {
// Install once on the VM owner thread. The VM hook retains the guard state even
// after this registration handle is destroyed, through VM teardown. Registry
// entries are weak; owners must detach worlds before dropping their own roots.
class WorldJobGuard final {
public:
    AK_ALLOC_WITH_KMALLOC;
    explicit WorldJobGuard(JS::VM&);
    ~WorldJobGuard();
    WorldJobGuard(WorldJobGuard const&) = delete;
    WorldJobGuard& operator=(WorldJobGuard const&) = delete;
    ErrorOr<void> protect(IsolatedWorld&);
    size_t blocked_count() const;
    size_t source_rejection_count() const;
    size_t source_job_error_count() const;
private:
    struct State;
    NonnullRefPtr<State> m_state;
};
}
