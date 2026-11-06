/**
 * Owns observer callback wrappers and source-world disconnect. Copied delivery and teardown
 * must not expose page prototypes or retain revoked realms.
 */

// Keep the original observer class/algorithms. Observe only successful node-list
// additions and disconnect clearing through a reference view in this TU.
#include "callbacks/observers/ObserverAdapter.h"
#define m_node_list Firefly::Observers::nodes_view(m_node_list, *this)
#include <LibWeb/DOM/MutationObserver.cpp>
#undef m_node_list
