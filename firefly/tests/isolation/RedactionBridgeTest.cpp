/** Exact native capability redaction, without getter execution or weakening raw IPC capture. */
#include "RedactionBridgeTest.h"
#include "context/ContextBridge.h"
#include "context/Snapshot.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <AK/Utf16StringBuilder.h>
#include <LibGC/Heap.h>
#include <LibGC/WeakInlines.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

// Leave the allocation frame before collection: stale native stack words are
// conservative GC roots and must not masquerade as strong policy ownership.
[[gnu::noinline]] static GC::Weak<JS::Object> register_collectable(Firefly::IsolatedWorld& world)
{
    GC::Root object { JS::Object::create(*world.realm(), nullptr) };
    world.redact_object_export(*object);
    return GC::Weak { *object };
}

ErrorOr<void> check_redaction_bridge(JS::VM& vm, Web::DOM::Document& document)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    Firefly::WorldJobGuard jobs(vm);
    TRY(jobs.protect(*source));
    auto bridge = TRY(Firefly::ContextBridge::create(*source, document));
    auto& principal = document.window()->principal_realm();
    source->realm()->global_object().define_direct_property(
        "contextBridge"_utf16, bridge->exports(), JS::default_attributes);
    ScopeGuard cleanup([&] { source->detach(); });
    size_t stage = 0;
    auto check = [&](Utf16View code, bool isolated = false) -> ErrorOr<void> {
        ++stage;
        Utf16StringBuilder builder;
        builder.append(u"{"sv);
        builder.append(code);
        builder.append(u"}"sv);
        auto wrapped = builder.to_string();
        auto result = [&]() -> JS::ThrowCompletionOr<JS::Value> {
            if (isolated)
                return source->evaluate(wrapped);
            auto parsed = JS::Script::parse(wrapped, principal);
            if (parsed.is_error())
                return JS::js_undefined();
            GC::Root script { parsed.release_value() };
            return vm.run(*script);
        }();
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool()) {
            warnln("Capability redaction assertion failed at stage {}", stage);
            return Error::from_string_literal("Capability redaction assertion failed");
        }

        return {};
    };

    auto checkpoint = [&] {
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
    };

    TRY(check(uR"JS(
        globalThis.redactionTraps=0; globalThis.privateCalls=0;
        globalThis.privateModule=Object.create(null);
        privateModule.send=()=>{++privateCalls};
        privateModule.self=privateModule;
        privateModule[Symbol()]=()=>1;
        Object.defineProperty(privateModule,'secret',{get(){redactionTraps++;throw 1}});
        true;
    )JS"sv,
        true));
    GC::Root module {
        source->realm()->global_object().get_without_side_effects("privateModule"_utf16).as_object()
    };

    auto& method = module->get_without_side_effects("send"_utf16).as_function();
    source->block_function_export(method);
    source->redact_object_export(*module);
    source->redact_object_export(*module); // Duplicate native registration is inert.
    if (!source->object_export_redacted(*module)
        || !Firefly::BridgeData::capture(JS::Value(module.ptr()), *source->realm(), {}).is_error())
        return Error::from_string_literal("Redaction weakened the default data-only capture");
    auto other = TRY(Firefly::IsolatedWorld::create(*document.window()));
    ScopeGuard detach_other([&] { other->detach(); });
    if (!Firefly::capture_to_principal(*other, JS::Value(module.ptr())).is_error())
        return Error::from_string_literal("Foreign module bypassed exact source ownership");

    TRY(check(uR"JS(
        contextBridge.exposeInMainWorld('__ffPrivateRedacted', privateModule);
        contextBridge.exposeInMainWorld('__ffRedaction', {
            a:privateModule,b:privateModule,
            get(){return {a:privateModule,b:privateModule}},
            async promised(){return privateModule},
            emit(cb){return cb({sender:privateModule,alias:privateModule,ports:[]}, {n:7})}
        });
        let blocked=0;
        for(const api of [privateModule.send,{send:privateModule.send},
            new Proxy(privateModule,{get(){redactionTraps++;throw 1}}),
            {get sender(){redactionTraps++;throw 1}}]) {
            try {contextBridge.exposeInMainWorld('__ffRedactionBad',api)}
            catch(e){if(e instanceof TypeError) ++blocked}
        }
        blocked===4 && redactionTraps===0 && privateCalls===0;
    )JS"sv,
        true));
    TRY(check(uR"JS(
        globalThis.redactionEmpty=value=>Object.getPrototypeOf(value)===Object.prototype &&
            Reflect.ownKeys(value).length===0 && Object.isFrozen(value);
        const api=__ffRedaction, result=api.get();
        redactionEmpty(__ffPrivateRedacted) && redactionEmpty(api.a) && api.a===api.b &&
            redactionEmpty(result.a) && result.a===result.b &&
            api.emit((event,value)=>redactionEmpty(event.sender) && event.sender===event.alias &&
                Object.isFrozen(event) && Array.isArray(event.ports) && event.ports.length===0 &&
                value.n===7 && !('send' in event.sender));
    )JS"sv));
    TRY(check(uR"JS(
        globalThis.redactionPromise=false;
        __ffRedaction.promised().then(value=>redactionPromise=redactionEmpty(value));true;
    )JS"sv));
    checkpoint();
    TRY(check(u"redactionPromise===true"sv));
    TRY(check(u"redactionTraps===0 && privateCalls===0 && (privateModule=null,true)"sv, true));
    auto weak_module = register_collectable(*source);
    vm.heap().collect_garbage();
    if (weak_module.ptr())
        return Error::from_string_literal("Native redaction metadata retained a dead module");
    source->detach();
    if (source->object_export_redacted(*module))
        return Error::from_string_literal("Detach retained native redaction identities");
    TRY(check(uR"JS(
        let blocked=false;try {__ffRedaction.get()}catch(e){blocked=e instanceof TypeError}
        blocked && redactionEmpty(__ffPrivateRedacted);
    )JS"sv));
    outln("PASS native exact-object capability redaction: publication/results/callbacks/Promises, "
          "aliases/realm/prototype, raw-method and spoof refusal, no traps, weak GC and detach");
    return {};
}
