/** Main-to-preload messages bind to the current native document, with bounded private ACK credit.
 */
#include "Impl.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include "serialization/WireValue.h"
#include <AK/JsonObject.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentPage.h>
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
    auto bytes = wire_bytes(vm.argument(1), realm);
    if (bytes.is_error()
        || BridgeData::decode_wire(bytes.value().view(), public_binary_limits()).is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC graph"sv);
    if (window->ipc_outgoing.size() >= 16 || ipc_outgoing_count >= 64
        || window->ipc_outgoing_next == 0xffffffffu)
        return preload_error(
            vm, "ERR_FIREFLY_IPC_LIMIT", "Main-to-renderer event budget exceeded"sv);
    auto request = ++window->ipc_outgoing_next;
    window->ipc_outgoing.emplace(request, MonotonicTime::now() + AK::Duration::from_seconds(30));
    ++ipc_outgoing_count;
    if (!queue_ipc_packet(id, ApplicationIpc::Kind::Event, request, false, bytes.value())) {
        if (window->ipc_outgoing.erase(request))
            --ipc_outgoing_count;
        return preload_error(vm, "ERR_FIREFLY_IPC_REVOKED", "IPC document channel is retired"sv);
    }

    return JS::js_undefined();
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

        bool expired = false;
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
            window.ipc_outgoing_failed = true;
            ipc_outgoing_count -= window.ipc_outgoing.size();
            window.ipc_outgoing.clear();
            warnln("ERR_FIREFLY_IPC_SEND: main-to-renderer peer timed out; endpoint retired");
        }
    }
}
}
