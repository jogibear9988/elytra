// Keep the original observer class/algorithms. Observe only successful node-list
// additions and disconnect clearing through a reference view in this TU.
#include "ObserverAdapter.h"
#define m_node_list Firefly::Observers::nodes_view(m_node_list, *this)
#include <LibWeb/DOM/MutationObserver.cpp>
#undef m_node_list
