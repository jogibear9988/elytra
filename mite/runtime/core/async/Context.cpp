#include "core/async/Context.h"
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Mite {
GC_DEFINE_ALLOCATOR(AsyncStorage);
GC_DEFINE_ALLOCATOR(AsyncFrame);
GC_DEFINE_ALLOCATOR(AsyncCarrier);

class MiteAsyncJobContext final : public GC::Cell {
    GC_CELL(MiteAsyncJobContext, GC::Cell);
    GC_DECLARE_ALLOCATOR(MiteAsyncJobContext);

public:
    MiteAsyncJobContext(JS::JobCallback& original, AsyncCarrier& carrier)
        : original(original)
        , carrier(carrier)
    {
    }

    GC::Ref<JS::JobCallback> original;
    GC::Ref<AsyncCarrier> carrier;

    void visit_edges(Visitor& visitor) override
    {
        Base::visit_edges(visitor);
        visitor.visit(original);
        visitor.visit(carrier);
    }
};

GC_DEFINE_ALLOCATOR(MiteAsyncJobContext);

void AsyncFrame::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    for (auto const& binding : bindings) {
        visitor.visit(binding.storage);
        visitor.visit(binding.value);
    }
}

void AsyncCarrier::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(frame);
}

AsyncContext::Scope::Scope(AsyncContext& owner, GC::Ptr<AsyncCarrier> carrier)
    : m_owner(owner)
    , m_previous(owner.m_current.ptr())
{
    owner.m_current = carrier;
}

AsyncContext::Scope::~Scope()
{
    m_owner.m_current = m_previous.ptr();
}

AsyncContext::AsyncContext(JS::VM& vm)
    : m_vm(vm)
    , m_make(move(vm.host_make_job_callback))
    , m_call(move(vm.host_call_job_callback))
{
    vm.host_make_job_callback = [this](JS::FunctionObject& function) {
        GC::Root original { m_make(function) };
        GC::Root carrier { capture() };
        GC::Root metadata { m_vm.heap().allocate<MiteAsyncJobContext>(*original, *carrier) };
        return JS::JobCallback::create(m_vm, function, metadata.ptr());
    };

    vm.host_call_job_callback
        = [this](JS::JobCallback& callback, JS::Value receiver,
              ReadonlySpan<JS::Value> args) -> JS::ThrowCompletionOr<JS::Value> {
        auto metadata = callback.custom_data();
        if (metadata && GC::class_name_of(*metadata) == "MiteAsyncJobContext"sv) {
            auto& record = static_cast<MiteAsyncJobContext&>(*metadata);
            Scope scope(*this, record.carrier);
            return m_call(record.original, receiver, args);
        }

        return m_call(callback, receiver, args);
    };
}

AsyncContext::~AsyncContext()
{
    m_vm.host_make_job_callback = move(m_make);
    m_vm.host_call_job_callback = move(m_call);
    revoke_native_captures();
    m_current = nullptr;
}

void AsyncContext::visit_capture_edges(GC::Cell::Visitor& visitor) const
{
    visitor.visit(m_current.ptr());
    for (auto const& storage : m_restoring)
        visitor.visit(storage.ptr());
}

GC::Ref<AsyncCarrier> AsyncContext::capture()
{
    // IDs are safe integers; exhaustion is practically unreachable, never wrap/reuse.
    VERIFY(m_next < 9007199254740992ULL);
    return m_vm.heap().allocate<AsyncCarrier>(
        m_current ? m_current->frame : nullptr, m_next++, m_current ? m_current->id : 0);
}

JS::ThrowCompletionOr<GC::Ref<AsyncCarrier>> AsyncContext::with_storage(
    AsyncStorage& storage, JS::Value value, bool remove)
{
    Vector<AsyncFrame::Binding> bindings;
    if (m_current && m_current->frame) {
        for (auto const& item : m_current->frame->bindings) {
            if (item.storage.ptr() != &storage)
                bindings.append(item);
        }
    }

    if (!remove) {
        bindings.append({ &storage, value });
    }

    // Reserve absent bindings required by nested run() restoration. Refuse the
    // new admission before mutation; a callback cannot consume its finally slot.
    Vector<AsyncStorage*> identities;
    for (auto const& item : bindings)
        identities.append(item.storage.ptr());
    for (auto const& item : m_restoring)
        if (!identities.contains_slow(item.ptr()))
            identities.append(item.ptr());
    if (identities.size() > 256)
        return m_vm.throw_completion<JS::RangeError>("Async context storage limit exceeded"_utf16);

    GC::Root frame { m_vm.heap().allocate<AsyncFrame>(move(bindings)) };
    return m_vm.heap().allocate<AsyncCarrier>(
        frame.ptr(), m_current ? m_current->id : 0, m_current ? m_current->trigger : 0);
}

static JS::ThrowCompletionOr<JS::Value> invoke(JS::VM& vm, size_t callback_index)
{
    if (!vm.argument(callback_index).is_function())
        return vm.throw_completion<JS::TypeError>("Callback must be a function"_utf16);
    GC::RootVector<JS::Value> args;
    for (size_t i = callback_index + 2; i < vm.argument_count(); ++i)
        args.append(vm.argument(i));
    return JS::call(vm, vm.argument(callback_index), vm.argument(callback_index + 1), args.span());
}

GC::Ref<JS::Object> AsyncContext::create_storage(JS::Realm& realm)
{
    GC::Root storage { m_vm.heap().allocate<AsyncStorage>() };
    GC::Root object { JS::Object::create(realm, nullptr) };
    object->define_native_function(realm, "get"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this), storage.ptr()),
            [](auto& ref, auto& storage, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (self && self->m_current && self->m_current->frame)
                    for (auto const& item : self->m_current->frame->bindings)
                        if (item.storage.ptr() == storage)
                            return item.value;
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    object->define_native_function(realm, "disable"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this), storage.ptr()),
            [](auto& ref, auto& storage, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (!self)
                    return vm.throw_completion<JS::Error>("Async context closed"_utf16);
                // Node24 frame-mode disable mutates only the current frame. Existing
                // snapshots sharing that frame observe removal; other frames retain it.
                if (self->m_current && self->m_current->frame)
                    self->m_current->frame->bindings.remove_all_matching(
                        [&](auto const& item) { return item.storage.ptr() == storage; });
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    object->define_native_function(realm, "enter"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this), storage.ptr()),
            [](auto& ref, auto& storage, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (!self)
                    return vm.throw_completion<JS::Error>("Async context closed"_utf16);
                self->m_current = TRY(self->with_storage(*storage, vm.argument(0), false));
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    object->define_native_function(realm, "run"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this), storage.ptr()),
            [](auto& ref, auto& storage, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (!self)
                    return vm.throw_completion<JS::Error>("Async context closed"_utf16);
                GC::Root carrier { TRY(
                    self->with_storage(*storage, vm.argument(0), vm.argument(1).to_boolean())) };
                GC::Root<AsyncCarrier> previous { self->m_current.ptr() };
                GC::RootVector<JS::Value> prior;
                bool present = false;
                if (previous && previous->frame)
                    for (auto const& item : previous->frame->bindings)
                        if (item.storage.ptr() == storage) {
                            prior.append(item.value);
                            present = true;
                            break;
                        }

                if (present)
                    self->m_restoring.emplace_back(storage);
                ScopeGuard release_reservation = [&] {
                    if (present)
                        self->m_restoring.pop_back();
                };

                self->m_current = carrier.ptr();
                auto result = invoke(vm, 2);
                GC::RootVector<JS::Value> outcome;
                outcome.append(result.is_error() ? result.error_value() : result.value());
                // Restore this storage only: enterWith on another instance is observable
                // after run returns, as in Node24's frame implementation.
                self->m_current = TRY(self->with_storage(
                    *storage, present ? prior[0] : JS::js_undefined(), !present));
                return result;
            }),
        4, JS::default_attributes);
    return *object;
}

GC::Ref<JS::Object> AsyncContext::create_resource(JS::Realm& realm)
{
    GC::Root carrier { capture() };
    GC::Root object { JS::Object::create(realm, nullptr) };
    object->define_direct_property("id"_utf16, JS::Value(static_cast<double>(carrier->id)), 0);
    object->define_direct_property(
        "trigger"_utf16, JS::Value(static_cast<double>(carrier->trigger)), 0);
    object->define_native_function(realm, "run"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this), carrier.ptr()),
            [](auto& ref, auto& carrier, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (!self)
                    return vm.throw_completion<JS::Error>("Async context closed"_utf16);
                Scope scope(*self, carrier);
                return invoke(vm, 0);
            }),
        2, JS::default_attributes);
    return *object;
}

GC::Ref<JS::Object> AsyncContext::create_host(JS::Realm& realm)
{
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(realm, "storage"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this)),
            [](auto& ref, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (!self)
                    return vm.throw_completion<JS::Error>("Async context closed"_utf16);
                return self->create_storage(vm.realm());
            }),
        0, JS::default_attributes);
    host->define_native_function(realm, "resource"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this)),
            [](auto& ref, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (!self)
                    return vm.throw_completion<JS::Error>("Async context closed"_utf16);
                return self->create_resource(vm.realm());
            }),
        0, JS::default_attributes);
    host->define_native_function(realm, "ids"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this)),
            [](auto& ref, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = ref.try_get();
                if (!self)
                    return vm.throw_completion<JS::Error>("Async context closed"_utf16);
                return JS::Value(static_cast<double>(vm.argument(0).to_boolean()
                        ? (self->m_current ? self->m_current->trigger : 0)
                        : (self->m_current ? self->m_current->id : 0)));
            }),
        1, JS::default_attributes);
    return *host;
}
}
