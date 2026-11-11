/** Authenticate canonical IPC senders and reserve/release native request budgets. */
#include "Impl.h"
#include "Value.h"
#include "core/EventLoop.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include "serialization/WireValue.h"
#include <AK/JsonObject.h>
#include <LibCore/Promise.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentPage.h>
#include <QPointer>
#include <QTimer>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
// Navigation/crash invalidates both queued replies and native budget slots.
void Bridge::Impl::reset_ipc(int id)
{
    reset_permissions(id);
    auto found = windows.find(id);
    if (found == windows.end())
        return;
    auto& window = found->second;
    // Frame identity belongs to the native canonical document, not an IPC
    // realm. First preload binding must preserve a frame read before IPC.
    // Registry validation retires replaced documents/hosts independently.
    if (window.closing)
        frames.invalidate(id, true);
    if (window.ipc_byte_channel) {
        window.ipc_byte_channel->close();
        window.ipc_byte_channel = nullptr;
    }

    for (auto& probe : window.ipc_admission_probes)
        probe->close();
    window.ipc_admission_probes.clear();
    window.ipc_byte_queue.clear();
    window.ipc_reassembler.reset();
    window.ipc_receive_reservation = nullptr;
    window.ipc_fragment_deadline.clear();
    window.ipc_byte_epoch = 0;
    window.ipc_byte_ready = false;
    window.ipc_byte_deadline.clear();
    ipc_pending_count -= window.ipc_pending.size();
    window.ipc_pending.clear();
    ipc_outgoing_count -= window.ipc_outgoing.size();
    window.ipc_outgoing.clear();
    window.ipc_outgoing_next = 0;
    window.ipc_outgoing_failed = false;
    window.ipc_document = {};
    window.ipc_realm = {};
    window.ipc_last = 0;
    window.ipc_control_last = 0;
    window.ipc_packets.clear();
    window.ipc_refused.clear();
}

ByteString Bridge::Impl::active_ipc_document(State const& window)
{
    auto* view = window.native_view();
    if (!view || !view->has_traversable())
        return {};
    auto const& doc = view->traversable().active_document();
    return ByteString::formatted("{}:{}", doc.id().namespace_id, doc.id().local_id);
}

bool Bridge::Impl::ipc_current(int id, ByteString const& document, ByteString const& realm_id)
{
    auto found = windows.find(id);
    return found != windows.end() && !found->second.closing && found->second.native_view()
        && found->second.native_view()->has_traversable()
        && found->second.native_view()->page().routed_connection()
        && found->second.ipc_document == document && found->second.ipc_realm == realm_id
        && active_ipc_document(found->second) == document;
}

void Bridge::Impl::send_ipc_reply(int id, ByteString const& document, ByteString const& realm_id,
    u32 request, bool success, ByteString const& wire)
{
    if (ipc_current(id, document, realm_id))
        queue_ipc_packet(id, ApplicationIpc::Kind::Reply, request, success, wire);
}

void Bridge::Impl::receive_ipc(WebView::WebContentPage& page, Web::HTML::CrossProcessId navigable,
    JsonValue params, NativeIpcKind kind)
{
    if ((kind != NativeIpcKind::Bind && kind != NativeIpcKind::Invoke
            && kind != NativeIpcKind::Event)
        || stopping || !params.is_object() || params.as_object().size() != 4)
        return;
    auto const& data = params.as_object();
    auto document = data.get_string("document"sv), realm_id = data.get_string("realm"sv),
         wire = data.get_string("wire"sv);
    auto request = data.get_u32("request"sv);
    if (!document.has_value() || document->bytes().size() > 64 || !realm_id.has_value()
        || realm_id->is_empty() || realm_id->bytes().size() > 1024 || !wire.has_value()
        || !wire->is_empty() || !request.has_value()
        || (*request == 0 && kind != NativeIpcKind::Bind))
        return;
    auto canonical = page.hosted_navigable(navigable);
    if (!canonical.has_value() || !canonical->is_top_level_traversable()
        || !page.hosts_active_document_of(*canonical))
        return;
    auto const& actual = canonical->active_document();
    auto actual_key
        = ByteString::formatted("{}:{}", actual.id().namespace_id, actual.id().local_id);
    if (document->to_byte_string() != actual_key)
        return;
    for (auto& [id, window] : windows) {
        if (window.closing || !window.preload.has_value() || !window.owns_page(page))
            continue;
        if (active_ipc_document(window) != actual_key)
            return;
        auto caller = realm_id->to_byte_string();
        if (kind == NativeIpcKind::Bind && (*request != 0 || !wire->is_empty()))
            return;
        if (window.ipc_document != actual_key) {
            reset_ipc(id);
            window.ipc_document = actual_key;
            window.ipc_realm = caller;
            window.ipc_last = 0;
        }

        if (window.ipc_realm != caller)
            return;
        // Bind establishes the endpoint; request markers join raw payloads below.
        if (kind == NativeIpcKind::Bind) {
            bind_ipc_channel(id, page, navigable, actual.id());
            return;
        }

        if (*request <= window.ipc_control_last)
            return;
        window.ipc_control_last = *request;
        bool event = kind == NativeIpcKind::Event;
        if (!admit_ipc_half(id, *request, true, event))
            return;
        window.ipc_packets.at(*request).marker = true;
        auto doc = window.ipc_document, realm_id = window.ipc_realm;
        page.traversable().append_history_queue_steps(
            [this, lifetime = QPointer<QObject>(context.get()), id, doc, realm_id,
                request = *request](NonnullRefPtr<Core::Promise<Empty>> complete) {
                if (lifetime && !stopping && ipc_current(id, doc, realm_id)) {
                    auto& state = windows.at(id);
                    auto found = state.ipc_packets.find(request);
                    if (found != state.ipc_packets.end()) {
                        found->second.history = true;
                        found->second.url = state.native_view()
                                                ->page()
                                                .traversable()
                                                .active_document_url()
                                                .serialize()
                                                .to_byte_string();
                        ready_ipc_packet(id, request);
                    }
                }

                complete->resolve({});
            });
        return;
    }
}

void Bridge::Impl::receive_ipc_packet(int id, u64 epoch, ReadonlyBytes bytes)
{
    auto& window = windows.at(id);
    auto required = ApplicationIpc::FragmentReassembler::required_bytes(bytes, epoch);
    if (required.is_error()) {
        retire_ipc_channel(id);
        return;
    }

    if (!window.ipc_reassembler.active()) {
        window.ipc_receive_reservation = window.ipc_wire_budget->reserve(required.value());
        if (!window.ipc_receive_reservation) {
            retire_ipc_channel(id);
            return;
        }
    }

    auto decoded = window.ipc_reassembler.accept(bytes, epoch);
    if (decoded.is_error()) {
        retire_ipc_channel(id);
        return;
    }

    if (!decoded.value().has_value()) {
        // A slow peer cannot refresh this fixed first-fragment deadline.
        if (!window.ipc_fragment_deadline.has_value())
            window.ipc_fragment_deadline = MonotonicTime::now() + AK::Duration::from_seconds(30);
        return;
    }

    window.ipc_fragment_deadline.clear();
    auto reservation = move(window.ipc_receive_reservation);
    auto packet = decoded.release_value().release_value();
    if (packet.kind == ApplicationIpc::Kind::Ack) {
        if (window.ipc_outgoing.erase(packet.request))
            --ipc_outgoing_count;
        return;
    }

    if ((packet.kind != ApplicationIpc::Kind::Invoke && packet.kind != ApplicationIpc::Kind::Send)
        || packet.request <= window.ipc_last) {
        retire_ipc_channel(id);
        return;
    }

    window.ipc_last = packet.request;
    auto wire = move(packet.payload);
    if (BridgeData::decode_wire(wire.view(), application_ipc_limits()).is_error()) {
        retire_ipc_channel(id);
        return;
    }

    bool event = packet.kind == ApplicationIpc::Kind::Send;
    if (!admit_ipc_half(id, packet.request, false, event))
        return;
    auto& pending = window.ipc_packets.at(packet.request);
    pending.payload = true;
    pending.wire = move(wire);
    pending.reservation = move(reservation);
    ready_ipc_packet(id, packet.request);
}

bool Bridge::Impl::admit_ipc_half(int id, u32 request, bool marker, bool event)
{
    auto& window = windows.at(id);
    if (auto refused = window.ipc_refused.find(request); refused != window.ipc_refused.end()) {
        if (refused->second != marker)
            window.ipc_refused.erase(refused);
        return false;
    }

    auto found = window.ipc_packets.find(request);
    if (found != window.ipc_packets.end()) {
        if (found->second.event != event
            || (marker ? found->second.marker : found->second.payload)) {
            retire_ipc_channel(id);
            return false;
        }

        return true;
    }

    if (window.ipc_pending.size() >= 16 || ipc_pending_count >= 64) {
        if (window.ipc_refused.size() >= 16) {
            retire_ipc_channel(id);
            return false;
        }

        window.ipc_refused.emplace(request, marker);
        auto error = MUST(
            BridgeData::from_string(u"IPC pending request budget exceeded"sv).encode_wire({}));
        send_ipc_reply(id, window.ipc_document, window.ipc_realm, request, false, error);
        return false;
    }

    window.ipc_pending.insert(request);
    ++ipc_pending_count;
    window.ipc_packets.emplace(request,
        State::RawRequest {
            .event = event, .deadline = MonotonicTime::now() + AK::Duration::from_seconds(30) });
    return true;
}

void Bridge::Impl::ready_ipc_packet(int id, u32 request)
{
    auto& window = windows.at(id);
    auto found = window.ipc_packets.find(request);
    if (found == window.ipc_packets.end() || !found->second.marker || !found->second.payload
        || !found->second.history)
        return;
    auto packet = move(found->second);
    window.ipc_packets.erase(found);
    QTimer::singleShot(0, context.get(),
        [this, id, doc = window.ipc_document, realm_id = window.ipc_realm, request,
            event = packet.event, wire = move(packet.wire), url = move(packet.url),
            reservation = move(packet.reservation)] {
            dispatch_ipc_packet(id, doc, realm_id, request, event, wire, url, reservation);
        });
}

void Bridge::Impl::dispatch_ipc_packet(int id, ByteString document, ByteString realm_id,
    u32 request, bool event, ByteString wire, ByteString url,
    RefPtr<IpcWireReservation> reservation)
{
    if (stopping || !ipc_current(id, document, realm_id) || !handler)
        return;
    if (jobs.is_busy()) {
        QTimer::singleShot(1, context.get(),
            [this, id, document, realm_id, request, event, wire = move(wire), url = move(url),
                reservation = move(reservation)] {
                dispatch_ipc_packet(id, document, realm_id, request, event, wire, url, reservation);
            });
        return;
    }

    auto& window = windows.at(id);
    if (!window.ipc_pending.contains(request))
        return;
    if (event) {
        window.ipc_pending.erase(request);
        --ipc_pending_count;
    }

    jobs.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
        GC::Root data { JS::Object::create(realm, realm.intrinsics().object_prototype()) };
        data->define_direct_property(
            "document"_utf16, BridgeDetail::text(vm, document), JS::default_attributes);
        data->define_direct_property(
            "realm"_utf16, BridgeDetail::text(vm, realm_id), JS::default_attributes);
        data->define_direct_property("request"_utf16, JS::Value(request), JS::default_attributes);
        data->define_direct_property(
            "url"_utf16, BridgeDetail::text(vm, url), JS::default_attributes);
        data->define_direct_property(
            "wire"_utf16, TRY(wire_value(realm, wire)), JS::default_attributes);
        return JS::call(vm, *handler, JS::js_undefined(), JS::Value(id),
            BridgeDetail::text(vm, event ? "ipc-send"sv : "ipc-invoke"sv), data.ptr());
    });
    if (jobs.status())
        gui.quit(jobs.status());
    if (event && ipc_current(id, document, realm_id)) {
        auto ack = MUST(BridgeData::from_string(u""sv).encode_wire({}));
        send_ipc_reply(id, document, realm_id, request, true, ack);
    }
}
}
