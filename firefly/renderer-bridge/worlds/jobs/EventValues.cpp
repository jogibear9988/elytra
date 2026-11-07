/** Trace and revoke isolated event-value snapshots without exposing source objects. */
#include "serialization/BridgeData.h"
#include "worlds/jobs/State.h"
#include <AK/ScopeGuard.h>
#include <LibGC/WeakInlines.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/CustomEvent.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Scripting/Environments.h>

namespace Firefly {
EventValues::Activity WorldJobGuard::State::event_activity(JS::Realm& realm)
{
    using enum EventValues::Activity;
    if (!realms.contains(realm)) {
        if (previous_gate.event_values.activity)
            return previous_gate.event_values.activity(previous_gate.context, realm);
        return Unregistered;
    }

    auto owner = owner_for(realm);
    auto document = Web::Bindings::principal_host_defined_environment_settings_object(realm)
                        .responsible_document();
    if (!owner || Web::Bindings::host_defined_wrapper_world(realm).is_detached())
        return Revoked;
    if (owner->is_detached() || !document || !document->is_fully_active())
        return owner->is_executing() ? Exiting : Revoked;
    return Live;
}

void WorldJobGuard::State::event_changed(Web::DOM::Event& event, JS::Realm* writer)
{
    event_values.remove_all_matching([&](auto const& record) {
        return !record.event.ptr() || record.event.ptr().ptr() == &event;
    });
    auto activity = writer ? EventValues::activity(*writer) : EventValues::Activity::Unregistered;
    EventRecord record { event, {}, activity != EventValues::Activity::Unregistered,
        activity == EventValues::Activity::Revoked || activity == EventValues::Activity::Exiting,
        {} };
    if (writer)
        record.writer = *writer;
    event_values.append(move(record));
    if (previous_gate.event_values.changed)
        previous_gate.event_values.changed(previous_gate.context, event, writer);
}

void WorldJobGuard::State::visit_event_values(Web::DOM::Event& event, GC::Cell::Visitor& visitor)
{
    for (auto const& record : event_values)
        if (record.event.ptr().ptr() == &event)
            for (auto const& copy : record.copies)
                if (copy.target.ptr())
                    visitor.visit(copy.value);
    if (previous_gate.event_values.visit)
        previous_gate.event_values.visit(previous_gate.context, event, visitor);
}

void WorldJobGuard::State::revoke_event_values(JS::Realm& realm)
{
    for (auto& record : filters)
        if (record.writer.ptr().ptr() == &realm)
            record.revoked = true;
    for (auto& record : event_values) {
        if (record.writer.ptr().ptr() == &realm)
            record.revoked = true;
        record.copies.remove_all_matching([&](auto const& copy) {
            return !copy.target.ptr() || copy.target.ptr().ptr() == &realm;
        });
    }

    if (previous_gate.event_values.revoke)
        previous_gate.event_values.revoke(previous_gate.context, realm);
}

JS::ThrowCompletionOr<JS::Value> WorldJobGuard::State::read_event_value(
    JS::Realm& target, Web::DOM::Event& event, JS::Value value)
{
    using enum EventValues::Activity;
    auto target_activity = EventValues::activity(target);
    auto reject = [&]() -> JS::ThrowCompletionOr<JS::Value> {
        // Reader errors must not capture source dispatch/callback frames.
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        auto context = JS::ExecutionContext::create(0, ReadonlySpan<JS::Value> {}, 0);
        context->realm = &target;
        vm.push_execution_context(*context);
        ScopeGuard pop([&] { VERIFY(vm.pop_execution_context() == context.ptr()); });
        return JS::throw_completion(
            JS::TypeError::create(target, u"Firefly event value cannot cross worlds"sv));
    };

    if (target_activity == Revoked)
        return reject();
    event_values.remove_all_matching([](auto const& record) { return !record.event.ptr(); });
    EventRecord* record = nullptr;
    for (auto& candidate : event_values)
        if (candidate.event.ptr().ptr() == &event) {
            record = &candidate;
            break;
        }

    if (!record) {
        if (previous_gate.event_values.read)
            return previous_gate.event_values.read(previous_gate.context, target, event, value);
        EventRecord unknown { event, {}, false, false, {} };
        if (value.is_object()) {
            auto& writer = value.as_object().shape().realm();
            unknown.writer = writer;
            unknown.writer_owned = EventValues::activity(writer) != Unregistered;
        }

        event_values.append(move(unknown));
        record = &event_values.last();
    }

    auto* source
        = value.is_object() ? &value.as_object().shape().realm() : record->writer.ptr().ptr();
    auto source_activity = source ? EventValues::activity(*source) : Unregistered;
    auto writer_activity
        = record->writer.ptr() ? EventValues::activity(*record->writer.ptr()) : Unregistered;
    bool owned_writer = record->writer_owned || writer_activity != Unregistered || record->revoked;
    // Shape realm can come from a foreign prototype. An owned writer's
    // identity must agree too, or a forged prototype could launder values.
    if (source == &target && (!owned_writer || record->writer.ptr().ptr() == &target))
        return value;
    if (target_activity == Exiting)
        return reject();
    if (target_activity == Unregistered && source_activity == Unregistered && !owned_writer)
        return value;
    record->copies.remove_all_matching([](auto const& copy) { return !copy.target.ptr(); });
    for (auto const& copy : record->copies)
        if (copy.target.ptr().ptr() == &target)
            return copy.value;
    if (record->revoked || source_activity == Revoked || source_activity == Exiting
        || writer_activity == Revoked || writer_activity == Exiting)
        return reject();
    if (record->copies.size() >= 64)
        return reject();
    auto data = BridgeData::capture(value, source ? *source : target, {});
    if (data.is_error())
        return reject();
    auto copied = data.value().materialize(target);
    if (copied.is_error())
        return reject();
    auto output = copied.release_value();
    record->copies.append({ target, output });
    return output;
}

}
