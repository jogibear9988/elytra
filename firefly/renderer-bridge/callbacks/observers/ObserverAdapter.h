/**
 * Owns observer callback wrappers and source-world disconnect. Copied delivery and teardown
 * must not expose page prototypes or retain revoked realms.
 */

#pragma once
#include "callbacks/CallbackGate.h"
#include <LibWeb/DOM/MutationObserver.h>

namespace Firefly::Observers {
class NodesView {
public:
    NodesView(Vector<GC::Weak<Web::DOM::Node>>& nodes, Web::DOM::MutationObserver& observer)
        : m_nodes(nodes)
        , m_observer(observer)
    {
    }

    auto begin()
    {
        return m_nodes.begin();
    }

    auto end()
    {
        return m_nodes.end();
    }

    void append(Web::DOM::Node& node)
    {
        m_nodes.append(node);

        Callbacks::observer_added(m_observer);
    }

    void clear()
    {
        m_nodes.clear();

        Callbacks::observer_removed(m_observer);
    }

private:
    Vector<GC::Weak<Web::DOM::Node>>& m_nodes;
    Web::DOM::MutationObserver& m_observer;
};

inline NodesView nodes_view(
    Vector<GC::Weak<Web::DOM::Node>>& nodes, Web::DOM::MutationObserver& observer)
{
    return { nodes, observer };
}
}
