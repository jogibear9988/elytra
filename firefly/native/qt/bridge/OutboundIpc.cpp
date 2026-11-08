/** Main-to-preload messages bind to the current native document, with bounded private ACK credit.
 */
#include "Impl.h"
#include "serialization/BridgeData.h"
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
    if (!vm.argument(1).is_string())
        return preload_error(vm, "ERR_INVALID_ARG_TYPE", "IPC wire must be a string"sv);
    auto view = vm.argument(1).as_string().utf16_string_view();
    if (view.length_in_code_units() > 128 * 1024)
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "IPC message too large"sv);
    auto bytes = view.to_utf8();
    if (bytes.is_error() || bytes.value().byte_count() > 128 * 1024
        || BridgeData::decode_wire(bytes.value().bytes_as_string_view(), {}).is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid IPC graph"sv);
    if (window->ipc_outgoing.size() >= 16 || ipc_outgoing_count >= 64
        || window->ipc_outgoing_next == 0xffffffffu)
        return preload_error(
            vm, "ERR_FIREFLY_IPC_LIMIT", "Main-to-renderer event budget exceeded"sv);
    auto request = ++window->ipc_outgoing_next;
    window->ipc_outgoing.emplace(request, MonotonicTime::now() + AK::Duration::from_seconds(30));
    ++ipc_outgoing_count;
    JsonObject envelope;
    envelope.set("document"sv, MUST(String::from_utf8(window->ipc_document.view())));
    envelope.set("realm"sv, MUST(String::from_utf8(window->ipc_realm.view())));
    envelope.set("request"sv, request);
    envelope.set("wire"sv, bytes.release_value());
    auto& page = window->view->page();
    page.async_run_webdriver_bidi_command(
        0, page.traversable().id(), "firefly.ipc.event.v1"sv, move(envelope));
    return JS::js_undefined();
}

void Bridge::Impl::expire_ipc_events()
{
    auto now = MonotonicTime::now();
    for (auto& [id, window] : windows) {
        (void)id;
        if (window.ipc_byte_deadline.has_value() && *window.ipc_byte_deadline <= now) {
            window.ipc_byte_deadline.clear();
            window.ipc_byte_ready = false;
            if (window.ipc_byte_channel) {
                window.ipc_byte_channel->close();
                window.ipc_byte_channel = nullptr;
            }

            warnln("ERR_FIREFLY_IPC_BYTES: document channel handshake timed out");
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
