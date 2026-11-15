/**
 * Releases source-owned native timers during world revocation. Timer cleanup must not cancel
 * document/page timers owned by a different world.
 */

#pragma once
#include "callbacks/CallbackGate.h"
#include <LibGC/Root.h>
#include <LibWeb/HTML/WindowOrWorkerGlobalScope.h>

namespace Firefly::Timers {
// Reference view over the original member, never a replacement container. GC
// visitation/iteration still reach the original map and all its callback roots.
class MapView {
public:
    MapView(Callbacks::TimerMap& map, Web::HTML::WindowOrWorkerGlobalScopeMixin& global)
        : m_map(map)
        , m_global(global)
    {
    }

    class Rearm {
    public:
        Rearm(Callbacks::TimerMap& map, Web::HTML::WindowOrWorkerGlobalScopeMixin& global, int id)
            : m_map(map)
            , m_global(global)
            , m_id(id)
            , m_realm(Callbacks::timer_registration_realm(global, map, id))
            , m_previous(s_active)
        {
            if (auto timer = m_map.get(id); timer.has_value())
                m_timer = timer->ptr();
            // Root the native registration identity independently of metadata:
            // GC may revoke the isolated owner while the replacement allocates.
            s_active = this;
            m_map.remove(id);
        }

        Rearm(Rearm const&) = delete;
        Rearm& operator=(Rearm const&) = delete;

        ~Rearm()
        {
            VERIFY(s_active == this);

            s_active = m_previous;
            if (!m_map.contains(m_id))
                Callbacks::timer_removed(m_global, m_map, m_id);
        }

        static JS::Realm* owner(
            Callbacks::TimerMap& map, Web::HTML::WindowOrWorkerGlobalScopeMixin& global, int id)
        {
            for (auto* scope = s_active; scope; scope = scope->m_previous) {
                if (&scope->m_map == &map && &scope->m_global == &global && scope->m_id == id)
                    return scope->m_realm.ptr();
            }

            return nullptr;
        }

    private:
        inline static thread_local Rearm* s_active {};
        Callbacks::TimerMap& m_map;
        Web::HTML::WindowOrWorkerGlobalScopeMixin& m_global;
        int m_id;
        GC::Root<Web::HTML::Timer> m_timer;
        GC::Root<JS::Realm> m_realm;
        Rearm* m_previous;
    };

    Rearm rearm(int id)
    {
        return Rearm { m_map, m_global, id };
    }

    auto get(int id)
    {
        return m_map.get(id);
    }

    bool contains(int id) const
    {
        return m_map.contains(id);
    }

    auto begin()
    {
        return m_map.begin();
    }

    auto end()
    {
        return m_map.end();
    }

    void visit_edges(GC::Cell::Visitor& visitor)
    {
        visitor.visit(m_map);
    }

    auto set(int id, GC::Ref<Web::HTML::Timer> timer)
    {
        auto result = m_map.set(id, timer);

        Callbacks::timer_added(m_global, m_map, id, *timer, Rearm::owner(m_map, m_global, id));

        return result;
    }

    bool remove(int id)
    {
        bool result = m_map.remove(id);

        if (result)
            Callbacks::timer_removed(m_global, m_map, id);

        return result;
    }

    void clear()
    {
        Vector<int> ids;
        for (auto const& entry : m_map)
            ids.append(entry.key);

        m_map.clear();

        for (auto id : ids)
            Callbacks::timer_removed(m_global, m_map, id);
    }

private:
    Callbacks::TimerMap& m_map;
    Web::HTML::WindowOrWorkerGlobalScopeMixin& m_global;
};

inline MapView map_view(Callbacks::TimerMap& map, Web::HTML::WindowOrWorkerGlobalScopeMixin& global)
{
    return { map, global };
}
}
