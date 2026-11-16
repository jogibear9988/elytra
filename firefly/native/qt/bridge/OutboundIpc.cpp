/** Main-to-preload messages bind to the current native document, with bounded private ACK credit.
 */
#include "Impl.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include "serialization/WireValue.h"
#include <AK/JsonObject.h>
#include <AK/QuickSort.h>
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentPage.h>
#include <QPointer>
#include <QTimer>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
JS::ThrowCompletionOr<JS::Value> Bridge::Impl::send_ipc_event(JS::VM& vm)
{
    auto* window = TRY(state(vm));
    auto id = vm.argument(0).as_i32();
    if (window->ipc_document.is_empty()
        || !ipc_current(id, window->ipc_document, window->ipc_realm))
        return preload_error(
            vm, "ERR_FIREFLY_IPC_NOT_READY", "No active bound top-level preload"sv);
    if (window->ipc_outgoing_failed)
        return preload_error(vm, "ERR_FIREFLY_IPC_REVOKED",
            "Event peer timed out; reload for a new document endpoint"sv);
    auto bytes = wire_bytes(vm.argument(1), realm, WireValuePolicy::ApplicationMessage);
    if (bytes.is_error()
        || BridgeData::decode_wire(bytes.value().view(), application_ipc_limits()).is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC graph"sv);
    if (window->ipc_waiting_events.size() >= 256 || ipc_waiting_event_count >= 1024
        || window->ipc_outgoing_next == 0xffffffffu)
        return preload_error(
            vm, "ERR_FIREFLY_IPC_LIMIT", "Main-to-renderer waiting event budget exceeded"sv);
    auto reservation = window->ipc_wire_budget->reserve(bytes.value().length());
    if (!reservation)
        return preload_error(
            vm, "ERR_FIREFLY_IPC_LIMIT", "Main-to-renderer wire budget exceeded"sv);
    window->ipc_waiting_events.append({ bytes.release_value(),
        MonotonicTime::now() + AK::Duration::from_seconds(30), move(reservation) });
    ++ipc_waiting_event_count;
    drain_ipc_events();
    auto current = windows.find(id);
    if (current == windows.end() || current->second.ipc_outgoing_failed)
        return preload_error(vm, "ERR_FIREFLY_IPC_REVOKED", "IPC document channel is retired"sv);

    return JS::js_undefined();
}

void Bridge::Impl::schedule_ipc_event_drain()
{
    if (stopping || ipc_event_drain_scheduled)
        return;
    ipc_event_drain_scheduled = true;
    QTimer::singleShot(0, context.get(), [this, lifetime = QPointer<QObject>(context.get())] {
        if (!lifetime)
            return;
        ipc_event_drain_scheduled = false;
        drain_ipc_events();
    });
}

void Bridge::Impl::drain_ipc_events()
{
    if (stopping || ipc_draining_events)
        return;
    ipc_draining_events = true;
    ScopeGuard done([&] { ipc_draining_events = false; });
    // Snapshot identities, never hold an unordered-map iterator across channel
    // activity. Global credit is shared with other windows and port transfers.
    Vector<int> ids;
    for (auto const& [id, window] : windows) {
        if (!window.ipc_waiting_events.is_empty())
            ids.append(id);
    }

    quick_sort(ids);
    bool progress = true;
    while (progress && ipc_outgoing_count < 64) {
        progress = false;
        size_t start = 0;
        while (start < ids.size() && ids[start] <= ipc_last_event_window)
            ++start;
        if (start == ids.size())
            start = 0;
        for (size_t step = 0; step < ids.size() && ipc_outgoing_count < 64; ++step) {
            auto id = ids[(start + step) % ids.size()];
            auto found = windows.find(id);
            if (found == windows.end())
                continue;
            auto& window = found->second;
            if (window.ipc_waiting_events.is_empty() || window.closing || window.ipc_outgoing_failed
                || !window.ipc_byte_ready || !window.ipc_byte_channel
                || !window.ipc_byte_channel->is_open())
                continue;
            if (!ipc_current(id, window.ipc_document, window.ipc_realm)
                || window.ipc_waiting_events.first().deadline <= MonotonicTime::now()
                || window.ipc_outgoing_next == 0xffffffffu) {
                retire_ipc_channel(id);
                continue;
            }
            // Waiting events never enter the physical control/reply queue.
            // Admit one only when it can start; a large fragmented message may
            // subsequently retain one in-progress slot until its contiguous end.
            if (window.ipc_outgoing.size() + window.port_event_pending.size() >= 16
                || !window.ipc_byte_queue.is_empty()
                || window.ipc_byte_channel->pending_send_count() >= Bytes::max_pending)
                continue;
            auto event = window.ipc_waiting_events.take_first();
            --ipc_waiting_event_count;
            auto request = ++window.ipc_outgoing_next;
            // Until the first fragment is accepted this is the waiting deadline;
            // flush_ipc_packets starts a fresh ACK deadline at that exact point.
            window.ipc_outgoing.emplace(request, event.deadline);
            ++ipc_outgoing_count;
            auto epoch = window.ipc_byte_epoch;
            window.ipc_byte_queue.append({ ApplicationIpc::Kind::Event, request, false,
                move(event.wire), 0, move(event.reservation) });
            ipc_last_event_window = id;
            progress = true;
            flush_ipc_packets(id, epoch);
        }
    }
}

void Bridge::Impl::expire_ipc_events()
{
    auto now = MonotonicTime::now();
    for (auto& [id, window] : windows) {
        (void)id;
        if (window.ipc_byte_deadline.has_value() && *window.ipc_byte_deadline <= now) {
            retire_ipc_channel(id);
            warnln("ERR_FIREFLY_IPC_BYTES: document channel handshake timed out");
        }

        if (window.ipc_fragment_deadline.has_value() && *window.ipc_fragment_deadline <= now) {
            retire_ipc_channel(id);
            warnln("ERR_FIREFLY_IPC_BYTES: incomplete logical message timed out");
        }

        Vector<u32> expired_halves;
        for (auto const& [request, packet] : window.ipc_packets)
            if (packet.deadline <= now)
                expired_halves.append(request);
        for (auto request : expired_halves) {
            auto found = window.ipc_packets.find(request);
            if (found == window.ipc_packets.end())
                continue;
            if (window.ipc_refused.size() >= 16) {
                retire_ipc_channel(id);
                break;
            }

            window.ipc_refused.emplace(request, found->second.marker);
            window.ipc_packets.erase(found);
            if (window.ipc_pending.erase(request))
                --ipc_pending_count;
            auto error = MUST(
                BridgeData::from_string(u"IPC packet admission timed out"sv).encode_wire({}));
            send_ipc_reply(id, window.ipc_document, window.ipc_realm, request, false, error);
        }

        bool expired = !window.ipc_waiting_events.is_empty()
            && window.ipc_waiting_events.first().deadline <= now;
        for (auto const& [request, deadline] : window.port_event_pending) {
            (void)request;
            if (deadline <= now) {
                expired = true;
                break;
            }
        }

        for (auto const& [request, deadline] : window.ipc_outgoing) {
            (void)request;
            if (deadline <= now) {
                expired = true;
                break;
            }
        }

        if (expired) {
            // Replenishing credit to a stalled peer would permit an unbounded
            // native transport queue. Retire this direction until document reload.
            retire_ipc_channel(id);
            warnln("ERR_FIREFLY_IPC_SEND: main-to-renderer peer timed out; endpoint retired");
        }
    }

    drain_ipc_events();
}
}
