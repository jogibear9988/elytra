#pragma once
#include "CallbackGate.h"
#include <LibWeb/HTML/WindowOrWorkerGlobalScope.h>

namespace Firefly::Timers {
// Reference view over the original member, never a replacement container. GC
// visitation/iteration still reach the original map and all its callback roots.
class MapView {
public:
    MapView(Callbacks::TimerMap& map, Web::HTML::WindowOrWorkerGlobalScopeMixin& global) : m_map(map), m_global(global) { }
    auto get(int id) { return m_map.get(id); }
    bool contains(int id) const { return m_map.contains(id); }
    auto begin() { return m_map.begin(); }
    auto end() { return m_map.end(); }
    void visit_edges(GC::Cell::Visitor& visitor) { visitor.visit(m_map); }
    auto set(int id, GC::Ref<Web::HTML::Timer> timer)
    {
        auto result = m_map.set(id, timer);
        Callbacks::timer_added(m_global, m_map, id, *timer);
        return result;
    }
    bool remove(int id)
    {
        bool result = m_map.remove(id);
        if (result) Callbacks::timer_removed(m_global, m_map, id);
        return result;
    }
    void clear()
    {
        Vector<int> ids;
        for (auto const& entry : m_map) ids.append(entry.key);
        m_map.clear();
        for (auto id : ids) Callbacks::timer_removed(m_global, m_map, id);
    }
private:
    Callbacks::TimerMap& m_map;
    Web::HTML::WindowOrWorkerGlobalScopeMixin& m_global;
};
inline MapView map_view(Callbacks::TimerMap& map, Web::HTML::WindowOrWorkerGlobalScopeMixin& global) { return {map, global}; }
}
