/** Forward only real SDK port dequeue across the authenticated document channel. */
#include "PortEndpoint.h"
#include "ipc/IpcEndpoint.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/BridgeData.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/Bindings/RustInterface.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/HTML/MessageEvent.h>
#include <LibWeb/HTML/MessagePort.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly {
namespace {
    namespace P = Utility::Ports;

    class NativePortTarget final : public Web::DOM::EventTarget {
        GC_CELL(NativePortTarget, Web::DOM::EventTarget);
        GC_DECLARE_ALLOCATOR(NativePortTarget);

    public:
        NativePortTarget(WeakPtr<IpcEndpoint> endpoint, P::Identity identity)
            : m_endpoint(move(endpoint))
            , m_identity(identity)
        {
        }

        virtual bool dispatch_event(Web::DOM::Event& event) override;

    private:
        WeakPtr<IpcEndpoint> m_endpoint;
        P::Identity m_identity;
    };

    GC_DEFINE_ALLOCATOR(NativePortTarget);
}

bool NativePortTarget::dispatch_event(Web::DOM::Event& event)
{
    if (auto endpoint = m_endpoint.strong_ref())
        PortEndpoint::dispatch_to(*endpoint, m_identity, event);
    return true;
}

void PortEndpoint::dispatch_to(IpcEndpoint& endpoint, P::Identity identity, Web::DOM::Event& event)
{
    if (endpoint.m_ports)
        endpoint.m_ports->dispatch(identity, event);
}

PortEndpoint::PortEndpoint(IpcEndpoint& endpoint)
    : m_endpoint(endpoint)
{
}

PortEndpoint::~PortEndpoint()
{
    revoke();
}

bool PortEndpoint::live() const
{
    return !m_stopped && !m_endpoint.m_byte_failed && m_endpoint.m_byte_epoch && m_endpoint.m_owner
        && !m_endpoint.m_owner->is_detached() && m_endpoint.m_document
        && m_endpoint.m_document->is_fully_active();
}

bool PortEndpoint::validate(void* context, Web::HTML::MessagePort&, JS::Realm& realm)
{
    auto& self = *static_cast<PortEndpoint*>(context);
    if (!self.live())
        return false;
    auto& document = *self.m_endpoint.m_document;
    if (&realm != self.m_endpoint.m_owner->realm()
        && &realm != &document.window()->principal_realm())
        return false;
    return Web::Bindings::principal_host_defined_environment_settings_object(realm)
               .responsible_document()
               .ptr()
        == &document;
}

void PortEndpoint::visit_edges(GC::Cell::Visitor& visitor) const
{
    for (auto const& item : m_routes) {
        visitor.visit(item.value.forwarder.ptr());
        visitor.visit(item.value.target.ptr());
    }
}

bool PortEndpoint::send(P::Kind kind, P::Identity route, ReadonlyBytes payload)
{
    if (!live() || m_controls.size() >= 8192)
        return false;
    auto frame = P::encode(
        kind, route, {}, { reinterpret_cast<char const*>(payload.data()), payload.size() });
    if (frame.empty() || frame.size() > ApplicationIpc::wire_limit
        || frame.size() + m_control_bytes + m_outgoing_bytes > 4 * 1024 * 1024)
        return false;
    m_control_bytes += frame.size();
    m_controls.append(ByteString(frame.data(), frame.size()));
    flush();
    return live();
}

bool PortEndpoint::receive(u32 sequence, ReadonlyBytes bytes)
{
    if (!live() || !sequence || sequence <= m_incoming_last)
        return false;
    P::Packet packet;
    if (!P::decode({ reinterpret_cast<char const*>(bytes.data()), bytes.size() }, packet))
        return false;
    m_incoming_last = sequence;
    if (packet.kind == P::Kind::Transfer) {
        if (m_endpoint.m_incoming_pending >= 16 || !m_ledger.admit_transfer(packet.ports))
            return false;
        auto wire = ByteString(packet.payload.data(), packet.payload.size());
        if (BridgeData::decode_wire(wire.view(), public_binary_limits()).is_error())
            return false;
        Vector<P::Identity> ports;
        for (auto identity : packet.ports) {
            Route route;
            route.identity = identity;
            m_routes.set(identity.id, move(route));
            ports.append(identity);
        }
        auto reservation = m_endpoint.m_wire_budget->reserve(wire.length());
        if (!reservation)
            return false;
        ++m_endpoint.m_incoming_pending;
        m_endpoint.receive_event(sequence, move(wire), move(reservation), move(ports));
        return true;
    }
    auto state = m_ledger.state(packet.route);
    if (state == P::Ledger::State::Retired)
        return true;
    if (state != P::Ledger::State::Live)
        return false;
    auto found = m_routes.find(packet.route.id);
    if (found == m_routes.end())
        return false;
    if (packet.kind == P::Kind::Close) {
        found->value.peer_closed = true;
        if (found->value.forwarder && found->value.incoming_sizes.empty())
            close(packet.route, false);
        return true;
    }
    if (packet.kind == P::Kind::Ack) {
        if (!m_ledger.acknowledge(packet.route, P::read_size(packet.payload)))
            return false;
        flush();
        return true;
    }
    if (found->value.peer_closed || packet.kind != P::Kind::Data
        || !m_ledger.received(packet.route, packet.payload.size()))
        return false;
    auto wire = ByteString(packet.payload.data(), packet.payload.size());
    if (BridgeData::decode_wire(wire.view(), public_binary_limits()).is_error())
        return false;
    found->value.incoming_sizes.push_back(wire.length());
    found->value.incoming.append(move(wire));
    deliver(found->value);
    return true;
}

JS::ThrowCompletionOr<JS::Value> PortEndpoint::materialize(
    JS::Realm& realm, Vector<P::Identity> const& identities)
{
    auto& vm = realm.vm();
    GC::Root result { TRY(JS::Array::create(realm, 0)) };
    bool accepted = false;
    ScopeGuard rollback([&] {
        if (!accepted)
            for (auto identity : identities)
                close(identity, true);
    });
    for (size_t index = 0; index < identities.size(); ++index) {
        auto found = m_routes.find(identities[index].id);
        if (!live() || found == m_routes.end() || found->value.forwarder)
            return vm.throw_completion<JS::TypeError>("Port transfer route is not live"_utf16);
        auto& route = found->value;
        GC::Root forwarder { Web::HTML::MessagePort::create(realm) };
        GC::Root receiver { Web::HTML::MessagePort::create(realm) };
        GC::Root target { GC::Heap::the().allocate<NativePortTarget>(
            m_endpoint.make_weak_ptr(), route.identity) };
        forwarder->entangle_with(*receiver);
        auto observation = PortObservation::create(vm, this, validate);
        auto closing = PortObservation::create(vm, this, validate);
        if (!observation || !closing || !PortObservation::observe(*receiver, observation, true)
            || !PortObservation::observe(*forwarder, closing, false)) {
            if (observation)
                PortObservation::revoke(observation);
            if (closing)
                PortObservation::revoke(closing);
            forwarder->close();
            receiver->close();
            close(route.identity, true);
            return vm.throw_completion<JS::TypeError>("Port observation admission failed"_utf16);
        }
        forwarder->set_message_event_target(*target);
        route.observation = move(observation);
        route.closing_observation = move(closing);
        route.forwarder = *forwarder;
        route.target = *target;
        forwarder->enable();
        auto wrapper = Web::Bindings::wrap(
            Web::Bindings::host_defined_wrapper_world(realm), realm, GC::Ref { *receiver });
        result->define_direct_property(index, wrapper, JS::default_attributes);
    }
    accepted = true;
    return result.ptr();
}

void PortEndpoint::deliver(Route& route)
{
    if (!live() || !route.forwarder || m_endpoint.m_owner->is_executing()
        || (route.observation && route.observation->closed.load()))
        return;
    auto identity = route.identity;
    auto owner = m_endpoint.m_owner;
    Web::HTML::TemporaryExecutionContext execution(
        m_endpoint.m_document->relevant_settings_object());
    auto result = owner->run_async_frame([&]() -> JS::ThrowCompletionOr<JS::Value> {
        auto& realm = *owner->realm();
        while (live()) {
            auto found = m_routes.find(identity.id);
            if (found == m_routes.end() || found->value.incoming.is_empty())
                break;
            auto wire = found->value.incoming.take_first();
            auto decoded = BridgeData::decode_wire(wire.view(), public_binary_limits());
            if (decoded.is_error())
                return realm.vm().throw_completion<JS::TypeError>("Invalid port data graph"_utf16);
            auto copied
                = TRY(decoded.value().materialize(realm, {}, BridgeData::Materialization::Mutable));
            auto value = GC::Root<JS::Value>::create(copied, SourceLocation::current());
            GC::RootVector<GC::Ref<JS::Object>> transfer;
            auto posted = found->value.forwarder->post_message(realm, value.value(), transfer);
            if (posted.is_exception())
                return realm.vm().throw_completion<JS::TypeError>("DOM port delivery failed"_utf16);
        }
        return JS::js_undefined();
    });
    if (result.is_error())
        close(identity, true);
}

void PortEndpoint::dispatch(P::Identity identity, Web::DOM::Event& event)
{
    if (!live() || m_ledger.state(identity) != P::Ledger::State::Live)
        return;
    if (!is<Web::HTML::MessageEvent>(event)) {
        close(identity, true);
        return;
    }
    auto& message = static_cast<Web::HTML::MessageEvent&>(event);
    if (event.type() != u"message"_utf16_fly_string || !message.message_ports().is_empty()) {
        for (auto port : message.message_ports())
            port->close();
        warnln("ERR_FIREFLY_PORT_UNSUPPORTED: nested DOM port transfer refused");
        close(identity, true);
        return;
    }
    auto& realm = *m_endpoint.m_owner->realm();
    auto captured = BridgeData::capture(message.data(), realm, public_binary_limits());
    if (captured.is_error()) {
        warnln("ERR_FIREFLY_PORT_SERIALIZATION: DOM port graph refused");
        close(identity, true);
        return;
    }
    auto encoded = captured.value().encode_wire(public_binary_limits());
    auto found = m_routes.find(identity.id);
    if (encoded.is_error() || found == m_routes.end()
        || encoded.value().length() + P::header_size > ApplicationIpc::wire_limit
        || found->value.outgoing.size() >= 64
        || found->value.outgoing_bytes + encoded.value().length() > 1024 * 1024
        || m_outgoing_bytes + m_control_bytes + encoded.value().length() > 4 * 1024 * 1024) {
        warnln("ERR_FIREFLY_PORT_LIMIT: DOM port queue exhausted");
        close(identity, true);
        return;
    }
    auto wire = encoded.release_value();
    found->value.outgoing_bytes += wire.length();
    m_outgoing_bytes += wire.length();
    found->value.outgoing.append(move(wire));
    flush();
}

void PortEndpoint::close(P::Identity identity, bool notify)
{
    auto route = m_routes.take(identity.id);
    if (!route.has_value())
        return;
    if (route->observation)
        PortObservation::revoke(route->observation);
    if (route->closing_observation)
        PortObservation::revoke(route->closing_observation);
    if (route->forwarder) {
        route->forwarder->discard_pending_messages();
        route->forwarder->close();
    }
    m_outgoing_bytes -= route->outgoing_bytes;
    m_ledger.close(identity);
    if (notify && !send(P::Kind::Close, identity))
        m_endpoint.retire_channel();
}

void PortEndpoint::flush()
{
    if (!live() || m_flushing)
        return;
    m_flushing = true;
    ScopeGuard done([&] { m_flushing = false; });
    while (!m_transfer_acks.is_empty() && live()
        && m_endpoint.m_byte_queue.size() < ApplicationIpc::queue_limit) {
        auto request = m_transfer_acks.take_first();
        if (!m_endpoint.send_packet(ApplicationIpc::Kind::PortAck, request, false, {}))
            return;
    }
    while (!m_controls.is_empty() && live()
        && m_endpoint.m_byte_queue.size() < ApplicationIpc::queue_limit) {
        if (m_next == 0xffffffffu) {
            m_endpoint.retire_channel();
            return;
        }
        auto wire = m_controls.take_first();
        m_control_bytes -= wire.length();
        if (!m_endpoint.send_packet(ApplicationIpc::Kind::PortControl, ++m_next, false, wire))
            return;
    }
    Vector<P::Identity> ids;
    for (auto const& entry : m_routes)
        ids.append(entry.value.identity);
    for (auto identity : ids) {
        while (live() && m_controls.is_empty()
            && m_endpoint.m_byte_queue.size() < ApplicationIpc::queue_limit) {
            auto found = m_routes.find(identity.id);
            if (found == m_routes.end() || found->value.outgoing.is_empty())
                break;
            auto& route = found->value;
            if (!m_ledger.can_send(identity, route.outgoing.first().length()))
                break;
            auto wire = route.outgoing.take_first();
            route.outgoing_bytes -= wire.length();
            m_outgoing_bytes -= wire.length();
            m_ledger.sent(identity, wire.length());
            if (!send(P::Kind::Data, identity, wire.bytes())) {
                m_endpoint.retire_channel();
                return;
            }
            // send() queues while the flush guard is held; publish it in order.
            if (m_next == 0xffffffffu) {
                m_endpoint.retire_channel();
                return;
            }
            auto frame = m_controls.take_first();
            m_control_bytes -= frame.length();
            if (!m_endpoint.send_packet(ApplicationIpc::Kind::PortControl, ++m_next, false, frame))
                return;
        }
    }
    for (auto identity : ids) {
        auto found = m_routes.find(identity.id);
        if (found != m_routes.end() && found->value.close_requested
            && found->value.outgoing.is_empty())
            close(identity, true);
    }
}

void PortEndpoint::expire()
{
    if (!live())
        return;
    Vector<P::Identity> ids;
    for (auto const& entry : m_routes)
        ids.append(entry.value.identity);
    for (auto identity : ids) {
        auto found = m_routes.find(identity.id);
        if (found == m_routes.end())
            continue;
        auto observation = found->value.observation;
        if (observation) {
            auto total = observation->dequeued.load();
            while (found->value.dequeued < total) {
                if (found->value.incoming_sizes.empty()) {
                    m_endpoint.retire_channel();
                    return;
                }
                auto size = found->value.incoming_sizes.front();
                found->value.incoming_sizes.pop_front();
                if (!m_ledger.can_consume(identity, size)) {
                    m_endpoint.retire_channel();
                    return;
                }
                m_ledger.consumed(identity, size);
                ++found->value.dequeued;
                auto payload = P::size_payload(size);
                if (!send(P::Kind::Ack, identity,
                        { reinterpret_cast<u8 const*>(payload.data()), payload.size() })) {
                    m_endpoint.retire_channel();
                    return;
                }
                found = m_routes.find(identity.id);
                if (found == m_routes.end())
                    return;
            }
        }
        if (found->value.closing_observation && found->value.closing_observation->closed.load()) {
            found->value.close_requested = true;
            if (found->value.outgoing.is_empty())
                close(identity, true);
            continue;
        }
        if (found->value.forwarder && found->value.peer_closed
            && found->value.incoming_sizes.empty()) {
            close(identity, false);
            continue;
        }
        deliver(found->value);
    }
    flush();
}

void PortEndpoint::writable()
{
    flush();
}

void PortEndpoint::acknowledge_transfer(u32 request)
{
    if (!live())
        return;
    if (m_transfer_acks.size() >= 16) {
        m_endpoint.retire_channel();
        return;
    }
    m_transfer_acks.append(request);
    flush();
}

void PortEndpoint::revoke()
{
    if (m_stopped)
        return;
    m_stopped = true;
    for (auto& entry : m_routes) {
        if (entry.value.observation)
            PortObservation::revoke(entry.value.observation);
        if (entry.value.closing_observation)
            PortObservation::revoke(entry.value.closing_observation);
        if (entry.value.forwarder) {
            entry.value.forwarder->discard_pending_messages();
            entry.value.forwarder->close();
        }
    }
    m_routes.clear();
    m_ledger.clear();
    m_controls.clear();
    m_transfer_acks.clear();
    m_outgoing_bytes = 0;
    m_control_bytes = 0;
}
}
