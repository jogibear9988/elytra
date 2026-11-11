/** Document-bound native port routing. Capability identities never enter application payloads. */
#include "Impl.h"
#include "Value.h"
#include "core/EventLoop.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include "serialization/WireValue.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/VM.h>
#include <QTimer>
#include <cmath>

namespace Firefly {
namespace {
    namespace Ports = Utility::Ports;
    constexpr int success = 0, blocked = 1, retired = 2, invalid = 3;

    Optional<u32> positive(JS::Value value, u32 maximum = Ports::identity_limit)
    {
        if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
            || value.as_double() > maximum || std::trunc(value.as_double()) != value.as_double())
            return {};
        return static_cast<u32>(value.as_double());
    }

    ByteString packet_bytes(std::vector<char> const& bytes)
    {
        return ByteString(StringView(bytes.data(), bytes.size()));
    }

    std::span<char const> span(ByteString const& bytes)
    {
        return { bytes.characters(), bytes.length() };
    }
}

Bridge::Impl::State* Bridge::Impl::renderer_port_state(JS::VM& vm)
{
    auto id = positive(vm.argument(0));
    if (!id.has_value() || stopping || !vm.argument(1).is_string())
        return nullptr;
    auto found = windows.find(static_cast<int>(*id));
    if (found == windows.end())
        return nullptr;
    auto& state = found->second;
    auto epoch = ByteString::number(state.ipc_byte_epoch);
    auto expected = Utf16String::from_utf8_with_replacement_character(epoch.view());
    if (!state.ipc_byte_epoch || state.ipc_outgoing_failed || !state.ipc_byte_channel
        || !state.ipc_byte_channel->is_open()
        || vm.argument(1).as_string().utf16_string_view() != expected.utf16_view()
        || !ipc_current(static_cast<int>(*id), state.ipc_document, state.ipc_realm))
        return nullptr;
    return &state;
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_port_epoch(JS::VM& vm)
{
    auto id = positive(vm.argument(0));
    if (!id.has_value() || stopping)
        return JS::js_null();
    auto found = windows.find(static_cast<int>(*id));
    if (found == windows.end())
        return JS::js_null();
    auto& state = found->second;
    if (!state.ipc_byte_epoch || state.ipc_outgoing_failed || !state.ipc_byte_channel
        || !state.ipc_byte_channel->is_open()
        || !ipc_current(static_cast<int>(*id), state.ipc_document, state.ipc_realm))
        return JS::js_null();
    auto epoch = ByteString::number(state.ipc_byte_epoch);
    return BridgeDetail::text(vm, epoch);
}

void Bridge::Impl::reset_renderer_ports(int id)
{
    auto found = windows.find(id);
    if (found == windows.end())
        return;
    auto& state = found->second;
    auto epoch = state.ipc_byte_epoch;
    // Route identities are scoped to the authenticated document epoch. A fresh
    // helper endpoint starts its ledger at zero after reload; old frames still
    // cannot cross the non-reused outer epoch.
    state.renderer_ports = Ports::Ledger {};
    ipc_outgoing_count -= state.port_event_pending.size();
    state.port_event_pending.clear();
    state.renderer_port_next = 0;
    state.renderer_port_incoming_last = 0;
    state.renderer_port_writable_queued = false;
    if (epoch)
        emit_renderer_port(id, epoch, "renderer-port-revoked");
}

void Bridge::Impl::emit_renderer_port(int id, u64 epoch, ByteString type, ByteString wire)
{
    if (stopping || !context)
        return;
    bool writable = type == "renderer-port-writable"sv;
    if (writable) {
        auto found = windows.find(id);
        if (found == windows.end() || found->second.ipc_byte_epoch != epoch
            || found->second.renderer_port_writable_queued)
            return;
        found->second.renderer_port_writable_queued = true;
    }

    QTimer::singleShot(0, context.get(), [this, id, epoch, type = move(type), wire = move(wire)] {
        if (stopping || !handler)
            return;
        bool revoked = type == "renderer-port-revoked"sv;
        auto found = windows.find(id);
        if (!revoked
            && (found == windows.end() || found->second.ipc_byte_epoch != epoch
                || found->second.ipc_outgoing_failed || !found->second.ipc_byte_channel
                || !found->second.ipc_byte_channel->is_open()
                || !ipc_current(id, found->second.ipc_document, found->second.ipc_realm)))
            return;
        if (type == "renderer-port-writable"sv)
            found->second.renderer_port_writable_queued = false;
        if (jobs.is_busy() || (dialogs && dialogs->is_synchronous())) {
            emit_renderer_port(id, epoch, type, wire);
            return;
        }
        // No state/map references survive this application callback.
        jobs.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
            GC::Root data { JS::Object::create(realm, nullptr) };
            auto text = ByteString::number(epoch);
            data->define_direct_property(
                "epoch"_utf16, BridgeDetail::text(vm, text), JS::default_attributes);
            if (!wire.is_empty())
                data->define_direct_property(
                    "wire"_utf16, TRY(wire_value(realm, wire)), JS::default_attributes);
            return JS::call(vm, *handler, JS::js_undefined(), JS::Value(id),
                BridgeDetail::text(vm, type), data.ptr());
        });
        if (jobs.status())
            gui.quit(jobs.status());
    });
}

bool Bridge::Impl::queue_renderer_port(int id, ByteString const& wire)
{
    auto& state = windows.at(id);
    if (wire.length() > ApplicationIpc::wire_limit || state.renderer_port_next == 0xffffffffu)
        return false;
    return queue_ipc_packet(
        id, ApplicationIpc::Kind::PortControl, ++state.renderer_port_next, false, wire);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_send_port_transfer(JS::VM& vm)
{
    auto* state = renderer_port_state(vm);
    int status = state ? success : retired;
    auto count = positive(vm.argument(3), 16);
    auto wire = wire_bytes(vm.argument(2), realm);
    std::vector<Ports::Identity> identities;
    if (!count.has_value() || wire.is_error())
        status = invalid;
    if (!status) {
        if (wire.value().length() > ApplicationIpc::wire_limit - Ports::header_size - *count * 8)
            status = invalid;
        else if (state->ipc_outgoing.size() + state->port_event_pending.size() >= 16
            || ipc_outgoing_count >= 64
            || state->ipc_byte_queue.size() >= ApplicationIpc::queue_limit)
            status = blocked;
        else if (state->renderer_port_next == 0xffffffffu
            || !state->renderer_ports.prepare_transfer(*count, identities))
            status = blocked;
    }

    if (!status) {
        auto graph = BridgeData::decode_wire(wire.value().view(), application_ipc_limits());
        if (graph.is_error())
            status = invalid;
        else {
            auto value = graph.value().materialize(realm, {}, BridgeData::Materialization::Mutable);
            if (value.is_error())
                status = invalid;
            else {
                auto root = GC::Root<JS::Value>::create(value.value(), SourceLocation::current());
                if (!root.value().is_object()
                    || !JS::Array::is_engine_class_of(root.value().as_object()))
                    status = invalid;
                else {
                    auto& array = root.value().as_object();
                    // Array length is an engine slot, not an ordinary property
                    // in the Rust-backed SDK. Read it without user hooks.
                    auto length = TRY(array.internal_get_own_property("length"_utf16));
                    auto channel = array.get_without_side_effects(0);
                    if (!length.has_value() || !length->value.has_value()
                        || !length->value->is_number() || length->value->as_double() != 2
                        || !channel.is_string()
                        || channel.as_string().length_in_utf16_code_units() > 256)
                        status = invalid;
                }
            }
        }
    }
    // Allocate the complete JS result before native admission; no application code
    // runs between wire queue admission and committing the native capability ledger.
    GC::Root ports { TRY(JS::Array::create(realm, status ? 0 : *count)) };
    if (!status) {
        for (size_t i = 0; i < identities.size(); ++i) {
            GC::Root descriptor { JS::Object::create(realm, nullptr) };
            descriptor->define_direct_property(
                "id"_utf16, JS::Value(identities[i].id), JS::default_attributes);
            descriptor->define_direct_property(
                "generation"_utf16, JS::Value(identities[i].generation), JS::default_attributes);
            ports->define_direct_property(i, descriptor.ptr(), JS::default_attributes);
        }
    }

    GC::Root result { JS::Object::create(realm, nullptr) };
    auto epoch = state ? ByteString::number(state->ipc_byte_epoch) : ByteString("0");
    result->define_direct_property(
        "epoch"_utf16, BridgeDetail::text(vm, epoch), JS::default_attributes);
    result->define_direct_property("ports"_utf16, ports.ptr(), JS::default_attributes);
    result->define_direct_property("status"_utf16, JS::Value(status), JS::default_attributes);
    if (!status) {
        auto id = static_cast<int>(*positive(vm.argument(0)));
        auto frame = packet_bytes(
            Ports::encode(Ports::Kind::Transfer, {}, identities, span(wire.value())));
        // Probe the same shared wire budget before committing event credit.
        // No application code or event pumping runs between this probe and queueing.
        auto reservation = state->ipc_wire_budget->reserve(frame.length());
        if (!reservation) {
            result->define_direct_property(
                "status"_utf16, JS::Value(blocked), JS::default_attributes);
            return JS::Value(result.ptr());
        }

        reservation = nullptr;
        auto request = state->renderer_port_next + 1;
        state->port_event_pending.emplace(
            request, MonotonicTime::now() + AK::Duration::from_seconds(30));
        ++ipc_outgoing_count;
        if (!queue_renderer_port(id, frame)) {
            // Channel retirement refunds its pending event map; never detach the
            // source wrappers on a failed admission.
            retire_ipc_channel(id);
            status = retired;
        } else {
            VERIFY(state->renderer_ports.admit_transfer(identities));
        }
    }

    result->define_direct_property("status"_utf16, JS::Value(status), JS::default_attributes);
    return JS::Value(result.ptr());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::renderer_port_action(JS::VM& vm, int action)
{
    auto* state = renderer_port_state(vm);
    if (!state)
        return JS::Value(retired);
    auto id = static_cast<int>(*positive(vm.argument(0)));
    auto route_id = positive(vm.argument(2)), generation = positive(vm.argument(3));
    if (!route_id.has_value() || !generation.has_value())
        return JS::Value(invalid);
    Ports::Identity route { *route_id, *generation };
    auto live = state->renderer_ports.state(route);
    if (live == Ports::Ledger::State::Retired) {
        // A peer Close can overtake the queued JavaScript consumption callback.
        // This exact known route is already canceled: ignore late consume/close
        // controls without emitting an ACK or refunding any other reservation.
        // Unknown identities and wrong generations remain invalid below; data
        // sends never report successful admission to a retired route.
        if (action == 1)
            return JS::Value(positive(vm.argument(4), ApplicationIpc::wire_limit).has_value()
                    ? success
                    : invalid);
        return JS::Value(action == 2 ? success : retired);
    }

    if (live != Ports::Ledger::State::Live)
        return JS::Value(invalid);
    if (state->ipc_byte_queue.size() >= ApplicationIpc::queue_limit)
        return JS::Value(blocked);
    ByteString wire;
    size_t size = 0;
    auto kind = action == 0 ? Ports::Kind::Data
        : action == 1       ? Ports::Kind::Ack
                            : Ports::Kind::Close;
    if (action == 0) {
        auto bytes = wire_bytes(vm.argument(4), realm);
        if (bytes.is_error()
            || bytes.value().length() > ApplicationIpc::wire_limit - Ports::header_size
            || BridgeData::decode_wire(bytes.value().view(), public_binary_limits()).is_error())
            return JS::Value(invalid);
        size = bytes.value().length();
        if (!state->renderer_ports.can_send(route, size))
            return JS::Value(blocked);
        wire = bytes.release_value();
    } else if (action == 1) {
        auto amount = positive(vm.argument(4), ApplicationIpc::wire_limit);
        if (!amount.has_value() || !state->renderer_ports.can_consume(route, *amount))
            return JS::Value(invalid);
        size = *amount;
        wire = packet_bytes(Ports::size_payload(*amount));
    }

    auto frame = packet_bytes(Ports::encode(kind, route, {}, span(wire)));
    auto reservation = state->ipc_wire_budget->reserve(frame.length());
    if (!reservation)
        return JS::Value(blocked);
    reservation = nullptr;
    if (!queue_renderer_port(id, frame)) {
        retire_ipc_channel(id);
        return JS::Value(retired);
    }

    if (action == 0)
        state->renderer_ports.sent(route, size);
    else if (action == 1)
        state->renderer_ports.consumed(route, size);
    else
        state->renderer_ports.close(route);
    return JS::Value(success);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_send_renderer_port(JS::VM& vm)
{
    return renderer_port_action(vm, 0);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_consume_renderer_port(JS::VM& vm)
{
    return renderer_port_action(vm, 1);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_close_renderer_port(JS::VM& vm)
{
    return renderer_port_action(vm, 2);
}

void Bridge::Impl::receive_renderer_port(
    int id, ApplicationIpc::Kind kind, u32 request, ByteString wire)
{
    auto& state = windows.at(id);
    if (kind == ApplicationIpc::Kind::PortAck) {
        if (!state.port_event_pending.erase(request)) {
            retire_ipc_channel(id);
            return;
        }

        --ipc_outgoing_count;
        emit_renderer_port(id, state.ipc_byte_epoch, "renderer-port-writable");
        return;
    }

    if (request <= state.renderer_port_incoming_last) {
        retire_ipc_channel(id);
        return;
    }

    state.renderer_port_incoming_last = request;
    Ports::Packet packet {};
    if (!Ports::decode(span(wire), packet) || packet.kind == Ports::Kind::Transfer) {
        retire_ipc_channel(id);
        return;
    }

    auto live = state.renderer_ports.state(packet.route);
    if (live == Ports::Ledger::State::Retired)
        return;
    if (live != Ports::Ledger::State::Live) {
        retire_ipc_channel(id);
        return;
    }

    if (packet.kind == Ports::Kind::Ack) {
        if (!state.renderer_ports.acknowledge(packet.route, Ports::read_size(packet.payload))) {
            retire_ipc_channel(id);
            return;
        }

        emit_renderer_port(id, state.ipc_byte_epoch, "renderer-port-writable");
    } else if (packet.kind == Ports::Kind::Close) {
        state.renderer_ports.close(packet.route);
        emit_renderer_port(id, state.ipc_byte_epoch, "renderer-port-close", move(wire));
    } else {
        StringView payload { packet.payload.data(), packet.payload.size() };
        if (BridgeData::decode_wire(payload, public_binary_limits()).is_error()
            || !state.renderer_ports.received(packet.route, packet.payload.size())) {
            retire_ipc_channel(id);
            return;
        }

        emit_renderer_port(id, state.ipc_byte_epoch, "renderer-port-data", move(wire));
    }
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_encode_port_ipc(JS::VM& vm)
{
    auto graph = BridgeData::capture_ipc(vm.argument(0), realm, public_binary_limits());
    if (graph.is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid port data"sv);
    auto wire = graph.value().encode_wire(public_binary_limits());
    if (wire.is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Port data exceeds its budget"sv);
    return wire_value(realm, wire.value());
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_host_decode_port_ipc(JS::VM& vm)
{
    auto wire = wire_bytes(vm.argument(0), realm);
    if (wire.is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid port bytes"sv);
    auto graph = BridgeData::decode_wire(wire.value().view(), public_binary_limits());
    if (graph.is_error())
        return preload_error(vm, "ERR_FIREFLY_IPC_SERIALIZATION", "Invalid port data"sv);
    return graph.value().materialize(realm, {}, BridgeData::Materialization::Mutable);
}

void Bridge::Impl::install_renderer_ports(JS::Object& host)
{
    host.define_direct_property(
        "rendererPortQueueFull"_utf16, JS::Value(blocked), JS::default_attributes);
    host.define_native_function(realm, "portEpoch"_utf16,
        native_binding<&Impl::native_host_port_epoch>(), 1, JS::default_attributes);
    host.define_native_function(realm, "sendPortTransfer"_utf16,
        native_binding<&Impl::native_host_send_port_transfer>(), 4, JS::default_attributes);
    host.define_native_function(realm, "sendRendererPort"_utf16,
        native_binding<&Impl::native_host_send_renderer_port>(), 5, JS::default_attributes);
    host.define_native_function(realm, "consumeRendererPort"_utf16,
        native_binding<&Impl::native_host_consume_renderer_port>(), 5, JS::default_attributes);
    host.define_native_function(realm, "closeRendererPort"_utf16,
        native_binding<&Impl::native_host_close_renderer_port>(), 4, JS::default_attributes);
    host.define_native_function(realm, "encodePortIpc"_utf16,
        native_binding<&Impl::native_host_encode_port_ipc>(), 1, JS::default_attributes);
    host.define_native_function(realm, "decodePortIpc"_utf16,
        native_binding<&Impl::native_host_decode_port_ipc>(), 1, JS::default_attributes);
}
}
