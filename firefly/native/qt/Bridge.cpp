/** Public bridge facade; implementation ownership and subsystems are private to bridge/. */
#include "Bridge.h"
#include "bridge/Impl.h"

namespace Firefly {
Bridge::Bridge(JS::VM& vm, JS::Realm& realm, Mite::EventLoop& jobs, Core::EventLoop& gui,
    std::optional<int>& exit_code)
    : m_impl(std::make_unique<Impl>(vm, realm, jobs, gui, exit_code))
{
}

Bridge::~Bridge() = default;

GC::Ref<JS::Object> Bridge::exports()
{
    return m_impl->install();
}

void Bridge::ready()
{
    m_impl->enqueue(0, "ready");
}
}
