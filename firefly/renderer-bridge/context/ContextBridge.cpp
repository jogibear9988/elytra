/**
 * Publishes bounded immutable application APIs from an isolated preload. Validation and
 * callback registration must succeed atomically before the page sees an API.
 */

#include "context/ContextBridge.h"
#include "context/CallbackBridge.h"
#include "context/Snapshot.h"
#include "serialization/BridgeData.h"
#include <AK/ScopeGuard.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly {
ContextBridge::ContextBridge(
    IsolatedWorld& source, Web::DOM::Document& document, CallbackRegistry& callbacks)
    : m_source(source)
    , m_callbacks(callbacks)
    , m_document(document)
{
}

ErrorOr<NonnullRefPtr<ContextBridge>> ContextBridge::create(
    IsolatedWorld& source, Web::DOM::Document& document)
{
    if (source.is_detached() || !document.is_fully_active()
        || Web::Bindings::host_defined_of(*source.realm()).principal_realm.ptr()
            != &document.window()->principal_realm())
        return Error::from_string_literal("Context bridge requires its own live document");
    auto callbacks = TRY(CallbackRegistry::create(source, document));
    auto owner = adopt_ref(*new ContextBridge(source, document, *callbacks));
    source.add_detach_hook([weak = owner->make_weak_ptr()] {
        if (auto owner = weak.strong_ref())
            owner->revoke();
    });
    return owner;
}

ContextBridge::~ContextBridge()
{
    revoke();
}

void ContextBridge::revoke()
{
    if (m_revoked)
        return;
    m_revoked = true;
    for (auto& function : m_functions)
        function->revoke();
    m_functions.clear();
    m_callbacks->revoke();
    m_callbacks.clear();
    m_document = nullptr;
    m_source.clear();
}

size_t ContextBridge::callback_count()
{
    return m_callbacks ? m_callbacks->size() : 0;
}

size_t ContextBridge::callback_proxy_count()
{
    return m_callbacks ? m_callbacks->proxy_count() : 0;
}

GC::Ref<JS::Object> ContextBridge::exports()
{
    VERIFY(!m_revoked && !m_source->is_detached());
    auto& realm = *m_source->realm();
    GC::Root object { JS::Object::create(realm, nullptr) };
    auto function = JS::NativeFunction::create(
        realm,
        [weak = make_weak_ptr()](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto owner = weak.strong_ref();
            if (!owner)
                return vm.throw_completion<JS::TypeError>("Context bridge is revoked"sv);
            return owner->expose(vm);
        },
        2, "exposeInMainWorld"_utf16, GC::Ptr<JS::Realm> { realm });
    object->define_direct_property("exposeInMainWorld"_utf16, function, {});
    VERIFY(MUST(object->set_integrity_level(JS::Object::IntegrityLevel::Frozen)));
    return *object;
}

JS::ThrowCompletionOr<JS::Value> ContextBridge::expose(JS::VM& vm)
{
    if (m_publishing || m_revoked || !m_source || m_source->is_detached()
        || !m_source->is_executing() || !m_document->is_fully_active()
        || vm.current_realm() != m_source->realm())
        return vm.throw_completion<JS::TypeError>(
            "Context bridge is revoked or called from another realm"sv);
    // Native function [[Realm]] is selected even when a foreign script calls
    // it. Require the first actual script/owner frame, through call/apply, to
    // belong to the source as well. An active source alone is not authority
    // while a principal DOM callback is reentering it.
    bool caller_is_source = false;
    vm.for_each_execution_context_top_to_bottom([&](JS::ExecutionContext const& context) {
        if (context.function && JS::NativeFunction::is_engine_class_of(*context.function))
            return true;
        caller_is_source = context.realm.ptr() == m_source->realm();
        return false;
    });
    if (!caller_is_source)
        return vm.throw_completion<JS::TypeError>("Bridge publication requires a source caller"sv);
    if (!vm.argument(0).is_string() || !vm.argument(0).as_string().length_in_utf16_code_units()
        || vm.argument(0).as_string().length_in_utf16_code_units() > 256 || m_publications >= 64)
        return vm.throw_completion<JS::TypeError>("Invalid or excessive bridge namespace"sv);
    NonnullRefPtr source { *m_source };
    GC::Root document { *m_document };
    GC::Root source_realm { *source->realm() };
    auto callbacks = m_callbacks;
    m_publishing = true;
    ScopeGuard publication([&] { m_publishing = false; });
    auto key = vm.argument(0).as_string().utf16_string();
    auto& target = document->window()->principal_realm();
    auto& global = target.global_object();
    auto existing = global.internal_get_own_property(key);
    if (existing.is_error())
        return vm.throw_completion<JS::TypeError>("Cannot inspect bridge namespace"sv);
    if (existing.value().has_value())
        return vm.throw_completion<JS::TypeError>("Bridge namespace already exists"sv);

    Vector<NonnullRefPtr<FunctionBridge>> pending;
    GC::RootVector<JS::Value> functions;
    bool committed = false;
    ScopeGuard rollback = [&] {
        if (!committed)
            for (auto& function : pending)
                function->revoke();
    };

    auto active
        = [&] { return !m_revoked && !source->is_detached() && document->is_fully_active(); };
    size_t getter_calls = 0;
    auto snapshot = capture_to_principal(
        *source, vm.argument(1),
        [&](JS::FunctionObject& function) -> ErrorOr<size_t> {
            if (!active() || m_functions.size() + pending.size() >= 64)
                return Error::from_string_literal("Bridge function budget exceeded");
            auto exported = TRY(FunctionBridge::create(*source, function, *document, callbacks));
            size_t id = functions.size();
            pending.append(exported.owner);
            functions.append(exported.function);
            return id;
        },
        [&](JS::Object& receiver, JS::FunctionObject& getter) -> ErrorOr<JS::Value> {
            auto id = getter.engine_class_id();
            if (!active() || getter_calls >= 64
                || (id != JS_LAYOUT_CLASS_ID_ECMASCRIPT_FUNCTION_OBJECT
                    && id != JS_LAYOUT_CLASS_ID_ECMASCRIPT_FUNCTION_OBJECT_WITH_RARE_DATA)
                || getter.realm() != source_realm.ptr())
                return Error::from_string_literal("Unsupported or excessive source getter");
            ++getter_calls;
            GC::Root function { getter };
            GC::Root object { receiver };
            auto value = JS::call(vm, *function, JS::Value(object.ptr()));
            if (value.is_error() || !active())
                return Error::from_string_literal("Source getter rejected");
            return value.release_value();
        });
    if (snapshot.is_error() || !active())
        return vm.throw_completion<JS::TypeError>("Unsupported or excessive bridge API"sv);
    // Getter side effects can create a principal property while capture runs.
    // Never overwrite a namespace that appeared after the initial check.
    auto collision = global.internal_get_own_property(key);
    if (collision.is_error() || collision.value().has_value())
        return vm.throw_completion<JS::TypeError>("Bridge namespace changed during publication"sv);
    auto value = snapshot.value().materialize(
        target,
        [&](size_t index) -> JS::ThrowCompletionOr<JS::Value> {
            VERIFY(index < functions.size());
            return functions[index];
        },
        BridgeData::Materialization::FrozenContainers);
    // Do not return a principal-realm exception object to the source world.
    if (value.is_error())
        return vm.throw_completion<JS::TypeError>("Cannot materialize bridge API"sv);
    auto api = GC::Root<JS::Value>::create(value.release_value(), SourceLocation::current());
    JS::PropertyDescriptor descriptor {
        .value = api.value(), .writable = false, .enumerable = true, .configurable = false
    };

    auto defined = global.internal_define_own_property(key, descriptor);
    if (defined.is_error() || !defined.value())
        return vm.throw_completion<JS::TypeError>("Cannot publish bridge API"sv);
    for (auto& function : pending)
        m_functions.append(move(function));
    ++m_publications;
    committed = true;
    return JS::js_undefined();
}
}
