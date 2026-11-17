/** Shared physical admission: payloads cannot consume reply or port-credit reserves. */
#pragma once
#include "Message.h"
#include <AK/Vector.h>

namespace Firefly::ApplicationIpc {
enum class Traffic { Bulk, SequencedControl, Critical, Event };

// The 48 existing control slots contain three nested 16-slot windows. Ordinary
// requests/port DATA use the first; port ACK/CLOSE can also use the second;
// unsequenced application replies/ACKs can use all three. Both directions have
// at most 16 genuine outstanding application acknowledgements per document.
// Packet owns its existing wire lease and exposes its logical fragment offset.
template <typename Packet> class OutgoingQueue {
public:
    bool can_append(Traffic traffic, size_t count = 1) const
    {
        size_t bulk = 0, sequenced = 0, total = 0, events = 0;
        for (auto const& entry : m_entries) {
            events += entry.traffic == Traffic::Event;
            bulk += entry.traffic == Traffic::Bulk;
            sequenced += entry.traffic == Traffic::SequencedControl;
            total += entry.traffic != Traffic::Event;
        }

        if (traffic == Traffic::Event)
            return count <= 1 && events == 0;
        if (count > queue_limit - total)
            return false;
        if (traffic != Traffic::Critical && count > 32 - bulk - sequenced)
            return false;
        return traffic != Traffic::Bulk || count <= 16 - bulk;
    }

    void append(Packet packet, Traffic traffic)
    {
        VERIFY(can_append(traffic));
        // Preserve the existing rule that an unstarted ordinary main Event
        // yields to controls. A started Event remains pinned by first().
        size_t insertion = m_entries.size();

        if (traffic != Traffic::Event) {
            for (size_t i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].traffic == Traffic::Event && !m_entries[i].packet.offset) {
                    insertion = i;
                    break;
                }
            }
        }

        m_entries.insert(insertion, Entry { move(packet), traffic });
    }

    Packet& first()
    {
        VERIFY(!m_entries.is_empty());
        // A started FIM2 message must finish without interleaving. Only
        // unsequenced critical traffic may pass other unstarted messages;
        // relative order of every PortControl remains unchanged. After 16
        // critical completions, the oldest noncritical packet gets a turn.
        if (!m_entries.first().packet.offset) {
            size_t selected = 0;
            bool prefer_critical = m_critical_completions < 16;
            for (size_t i = 0; i < m_entries.size(); ++i) {
                bool critical = m_entries[i].traffic == Traffic::Critical;
                if (critical == prefer_critical) {
                    selected = i;
                    break;
                }
            }

            if (selected)
                m_entries.prepend(m_entries.take(selected));
        }

        return m_entries.first().packet;
    }

    Packet take_first()
    {
        auto entry = m_entries.take_first();

        if (entry.traffic == Traffic::Critical) {
            if (m_critical_completions < 16)
                ++m_critical_completions;
        } else
            m_critical_completions = 0;
        return move(entry.packet);
    }

    bool is_empty() const
    {
        return m_entries.is_empty();
    }

    size_t size() const
    {
        return m_entries.size();
    }

    void clear()
    {
        m_entries.clear();

        m_critical_completions = 0;
    }

private:
    struct Entry {
        Packet packet;
        Traffic traffic;
    };

    Vector<Entry> m_entries;
    size_t m_critical_completions { 0 };
};
}
