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
    if (m_impl->dialogs && m_impl->dialogs->is_synchronous())
        return m_impl->jobs.status();
    Tracing::Recorder::Scope scope(m_impl->tracing, "firefly.main", "event-loop-turn");
    return m_impl->jobs.pump_once();
}

void Bridge::gpu_info_update()
{
    m_impl->enqueue(0, "gpu-info-update");
}

void Bridge::gpu_process_gone(ByteString details)
{
    m_impl->enqueue(0, "gpu-process-gone", move(details));
}

void Bridge::ready()
{
    m_impl->sandbox.ready();
    m_impl->enqueue(0, "ready");
}
}
