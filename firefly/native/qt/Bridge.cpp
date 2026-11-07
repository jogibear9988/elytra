/** Public bridge facade; implementation ownership and subsystems are private to bridge/. */
#include "Bridge.h"
#include "bridge/Impl.h"
#include "core/EventLoop.h"

namespace Firefly {
Bridge::Bridge(JS::VM& vm, JS::Realm& realm, Mite::EventLoop& jobs, Core::EventLoop& gui,
    std::optional<int>& exit_code, bool electron_compat)
    : m_impl(std::make_unique<Impl>(vm, realm, jobs, gui, exit_code, electron_compat))
{
}

Bridge::~Bridge() = default;

GC::Ref<JS::Object> Bridge::exports()
{
    return m_impl->install();
}

int Bridge::pump_jobs()
{
    Tracing::Recorder::Scope scope(m_impl->tracing, "firefly.main", "event-loop-turn");
    return m_impl->jobs.pump_once();
}

void Bridge::ready()
{
    m_impl->sandbox.ready();
    m_impl->enqueue(0, "ready");
}
}
