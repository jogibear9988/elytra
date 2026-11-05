#include "FunctionBridge.h"
#include "BridgeData.h"
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

namespace Firefly {
FunctionBridge::FunctionBridge(IsolatedWorld& source, JS::FunctionObject& function, Web::DOM::Document& document)
    : m_source(source), m_function(function), m_document(document)
{
}

ErrorOr<FunctionExport> FunctionBridge::create(IsolatedWorld& source, JS::FunctionObject& function, Web::DOM::Document& document)
{
    // FunctionObject::realm() is a native field read. Callable proxies/bound
    // functions have no own realm and are rejected without invoking their hooks.
    if (source.is_detached() || function.realm() != source.realm() || !document.is_fully_active() ||
        Web::Bindings::host_defined_of(*source.realm()).principal_realm.ptr() != &document.window()->principal_realm())
        return Error::from_string_literal("Function export requires its live source world and exact document");
    auto owner = adopt_ref(*new FunctionBridge(source, function, document));
    auto& target = document.window()->principal_realm();
    // The first create parameter selects a VM for allocation, not [[Realm]].
    // Set the optional realm explicitly to keep constructor/error paths in target.
    GC::Root proxy { JS::NativeFunction::create(target, [owner](JS::VM& vm) { return owner->invoke(vm); }, 0, "fireflyBridgeFunction"_utf16, GC::Ptr<JS::Realm> { target }) };
    if (proxy->realm() != &target) {
        owner->revoke();
        return Error::from_string_literal("Exported function has the wrong realm");
    }
    auto frozen = proxy->set_integrity_level(JS::Object::IntegrityLevel::Frozen);
    if (frozen.is_error() || !frozen.value()) {
        owner->revoke();
        return Error::from_string_literal("Cannot freeze exported function");
    }
    return FunctionExport { owner, *proxy };
}

void FunctionBridge::revoke()
{
    m_revoked = true;
    m_function = nullptr;
    m_document = nullptr;
    m_source.clear();
}

JS::ThrowCompletionOr<JS::Value> FunctionBridge::invoke(JS::VM& vm)
{
    auto error = [&]() { return vm.throw_completion<JS::TypeError>("Firefly bridge call rejected"sv); };
    if (m_revoked) return error();
    if (!m_document->is_fully_active() || m_source->is_detached()) {
        revoke();
        return error();
    }
    if (m_calling || vm.argument_count() > 64 || vm.current_realm() != &m_document->window()->principal_realm()) return error();
    NonnullRefPtr protect { *this };
    NonnullRefPtr source { *m_source };
    GC::Root function { *m_function };
    GC::Root target { *vm.current_realm() };
    GC::Root source_realm { *source->realm() };
    GC::Root document { *m_document };
    m_calling = true;
    ScopeGuard done([&] { m_calling = false; });
    GC::RootVector<JS::Value> arguments;
    for (size_t i = 0; i < vm.argument_count(); ++i) arguments.append(vm.argument(i));
    GC::Root array { JS::Array::create_from(*target, arguments.span()) };
    auto data = BridgeData::capture(JS::Value(array.ptr()), *target, {});
    if (data.is_error()) return error();
    auto copied = data.value().materialize(*source_realm);
    if (copied.is_error()) return error();
    auto copied_root = GC::Root<JS::Value>::create(copied.value(), SourceLocation::current());
    GC::RootVector<JS::Value> source_arguments;
    for (size_t i = 0; i < arguments.size(); ++i)
        source_arguments.append(copied.value().as_object().get_without_side_effects(i));
    auto result = source->call(*function, source_arguments.span());
    // Never inspect or stringify an exception supplied by the source world.
    // Recheck revocation after the source call, including native reentrant hooks.
    if (m_revoked || source->is_detached() || !document->is_fully_active()) {
        revoke();
        return error();
    }
    if (result.is_error()) return error();
    auto output = BridgeData::capture(result.value(), *source_realm, {});
    if (output.is_error()) return error();
    auto answer = output.value().materialize(*target);
    if (answer.is_error()) return error();
    return answer.release_value();
}
}
