/** GC-owned, non-forgeable CallSite records backed exclusively by captured LibJS metadata. */
#include "CallSites.h"
#include "core/GCCaptures.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Mite {
namespace {
    JS::ThrowCompletionOr<GC::Ref<JS::Object>> receiver_record(
        JS::VM& vm, JS::Object& records, JS::FunctionObject& lookup)
    {
        auto receiver = vm.this_value();
        // Reject Proxies before intrinsic WeakMap lookup, without invoking traps
        // or following a forged prototype chain to a different real CallSite.
        if (!receiver.is_object() || receiver.as_object().is_proxy())
            return vm.throw_completion<JS::TypeError>("Invalid CallSite receiver"_utf16);
        auto record = TRY(JS::call(vm, lookup, &records, receiver));
        if (!record.is_object())
            return vm.throw_completion<JS::TypeError>("Invalid CallSite receiver"_utf16);
        return record.as_object();
    }

    JS::Completion unsupported(JS::VM& vm)
    {
        GC::Root error { JS::Error::create(
            vm.realm(), "This CallSite method requires metadata not exposed by Mite"_utf16) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, "ERR_MITE_UNSUPPORTED_API"_utf16),
            JS::default_attributes);
        return JS::throw_completion(error);
    }
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> install_call_sites(JS::Realm& realm)
{
    auto& vm = realm.vm();
    // Capture the real intrinsics during bootstrap; application changes to
    // WeakMap.prototype must not alter private record ownership or lookup.
    GC::Root weak_constructor { TRY(realm.global_object().get("WeakMap"_utf16)).as_function() };
    GC::Root records { TRY(JS::construct(vm, *weak_constructor)) };
    GC::Root weak_prototype { TRY(weak_constructor->get("prototype"_utf16)).as_object() };
    GC::Root lookup { TRY(weak_prototype->get("get"_utf16)).as_function() };
    GC::Root store { TRY(weak_prototype->get("set"_utf16)).as_function() };
    GC::Root prototype { JS::Object::create(realm, realm.intrinsics().object_prototype()) };

    struct Method {
        Utf16FlyString name;
        Utf16FlyString field;
    };

    Method const supported[] {
        { "getFunctionName"_utf16, "name"_utf16 },
        { "getFileName"_utf16, "filename"_utf16 },
        { "getScriptNameOrSourceURL"_utf16, "filename"_utf16 },
        { "getLineNumber"_utf16, "line"_utf16 },
        { "getColumnNumber"_utf16, "column"_utf16 },
        { "isNative"_utf16, "native"_utf16 },
        { "toString"_utf16, "display"_utf16 },
    };

    for (auto const& method : supported) {
        prototype->define_native_function(realm, method.name,
            GC::create_function(realm.vm().heap(),
                GC::captures(GC::Ptr<JS::Object>(records.ptr()),
                    GC::Ptr<JS::FunctionObject>(lookup.ptr()), method.field),
                [](auto& map, auto& get, auto& field,
                    JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    GC::Root record { TRY(receiver_record(vm, *map, *get)) };
                    return record->get(field);
                }),
            0, JS::Attribute::Writable | JS::Attribute::Configurable);
    }

    // These V8 concepts cannot be reconstructed from a source location. Keep
    // them explicit rather than fabricating false/null implementation claims.
    Utf16FlyString const unavailable[] { "isConstructor"_utf16, "getThis"_utf16,
        "getFunction"_utf16, "getTypeName"_utf16, "getMethodName"_utf16, "getEvalOrigin"_utf16,
        "isEval"_utf16, "isAsync"_utf16, "isToplevel"_utf16, "isPromiseAll"_utf16,
        "getPromiseIndex"_utf16, "getPosition"_utf16, "getEnclosingLineNumber"_utf16,
        "getEnclosingColumnNumber"_utf16, "getScriptHash"_utf16 };
    for (auto name : unavailable) {
        prototype->define_native_function(realm, name,
            GC::create_function(realm.vm().heap(),
                GC::captures(
                    GC::Ptr<JS::Object>(records.ptr()), GC::Ptr<JS::FunctionObject>(lookup.ptr())),
                [](auto& map, auto& get, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    GC::Root record { TRY(receiver_record(vm, *map, *get)) };
                    return unsupported(vm);
                }),
            0, JS::Attribute::Writable | JS::Attribute::Configurable);
    }

    GC::Root factory { JS::Object::create(realm, nullptr) };
    factory->define_direct_property("records"_utf16, records.ptr(), 0);
    factory->define_direct_property("store"_utf16, store.ptr(), 0);
    factory->define_direct_property("prototype"_utf16, prototype.ptr(), 0);
    return *factory;
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> make_call_site(
    JS::VM& vm, JS::Object& factory, CapturedSite const& captured)
{
    GC::Root prototype { TRY(factory.get("prototype"_utf16)).as_object() };
    GC::Root site { JS::Object::create(vm.realm(), prototype.ptr()) };
    GC::Root record { JS::Object::create(vm.realm(), nullptr) };
    auto nullable_string = [&](Utf16String const& value) -> JS::Value {
        if (value.is_empty())
            return JS::js_null();
        return JS::PrimitiveString::create(vm, value);
    };

    record->define_direct_property("name"_utf16, nullable_string(captured.name), 0);
    record->define_direct_property("filename"_utf16,
        captured.source.has_value() ? nullable_string(captured.source->filename()) : JS::js_null(),
        0);
    record->define_direct_property("line"_utf16,
        captured.source.has_value() ? JS::Value(captured.source->start.line) : JS::js_null(), 0);
    record->define_direct_property("column"_utf16,
        captured.source.has_value() ? JS::Value(captured.source->start.column) : JS::js_null(), 0);
    record->define_direct_property("native"_utf16, JS::Value(captured.native), 0);
    record->define_direct_property(
        "display"_utf16, JS::PrimitiveString::create(vm, captured.display), 0);
    GC::Root records { TRY(factory.get("records"_utf16)).as_object() };
    GC::Root store { TRY(factory.get("store"_utf16)).as_function() };
    TRY(JS::call(vm, *store, records.ptr(), site.ptr(), record.ptr()));
    return *site;
}
}
