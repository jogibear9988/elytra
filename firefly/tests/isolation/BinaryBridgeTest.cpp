/** Exercise the public binary policy through actual contextBridge calls and text IPC frames. */
#include "BinaryBridgeTest.h"
#include "context/ContextBridge.h"
#include "serialization/BinaryPolicy.h"
#include "serialization/WireValue.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <AK/Utf16StringBuilder.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_binary_bridge(JS::VM& vm, Web::DOM::Document& document)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    Firefly::WorldJobGuard jobs(vm);
    TRY(jobs.protect(*source));
    auto bridge = TRY(Firefly::ContextBridge::create(*source, document));
    auto& target = document.window()->principal_realm();
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
            auto parsed = JS::Script::parse(wrapped, target);
            if (parsed.is_error())
                return JS::js_undefined();
            GC::Root script { parsed.release_value() };
            return vm.run(*script);
        }();
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool()) {
            warnln("Public binary bridge assertion failed at stage {}", stage);
            warnln("Async failures: {} / {}",
                target.global_object()
                    .get_without_side_effects("binaryAsyncError"_utf16)
                    .to_utf16_string_without_side_effects(),
                target.global_object()
                    .get_without_side_effects("binaryCallbackError"_utf16)
                    .to_utf16_string_without_side_effects());
            return Error::from_string_literal("Public binary bridge assertion failed");
        }

        return {};
    };

    auto checkpoint = [&] {
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
    };

    TRY(check(uR"JS(
        globalThis.binaryPrivate=new Uint8Array([0,128,255,7]);
        contextBridge.exposeInMainWorld('__ffBinary',{
            value:binaryPrivate,
            echo(value){return value},
            async echoAsync(value){return value},
            cb(callback){return callback(binaryPrivate)},
            async cbAsync(callback){return await callback(binaryPrivate)},
            original(){return binaryPrivate[0]}
        });true;
    )JS"sv,
        true));
    TRY(check(uR"JS(
        const api=__ffBinary,b=new ArrayBuffer(65536),bytes=new Uint8Array(b);
        bytes[0]=255;bytes[5]=128;bytes[65535]=129;
        const view=new Uint8Array(b,5,17),data=new DataView(b,3,23);
        let traps=0;
        for(const key of ['buffer','byteLength','byteOffset','length','constructor'])
            Object.defineProperty(view,key,{get(){traps++;throw 1}});
        const graph={b,view,alias:view,data}; graph.self=graph;
        const copy=api.echo(graph);
        copy.view[0]=33;
        api.value[0]=42;
        let rejected=false;try{api.echo(new Uint8Array(65537))}catch(e){rejected=e instanceof TypeError}
        globalThis.binaryAsync=false;
        api.echoAsync(new Uint8Array([0,128,255])).then(value=>{
            binaryAsync=value instanceof Uint8Array && value[2]===255;
        },e=>globalThis.binaryAsyncError=e.message);
        globalThis.binaryCallback=false;
        api.cbAsync(async value=>{value[0]=99;return new Uint8Array([9,8])}).then(value=>{
            binaryCallback=value instanceof Uint8Array && value[0]===9 && value[1]===8;
        },e=>globalThis.binaryCallbackError=e.message);
        const answer=api.cb(value=>{value[0]=66;return new Uint8Array([12])});
        copy.self===copy && copy.view===copy.alias && copy.view.buffer===copy.b &&
        copy.data.buffer===copy.b && copy.data.byteOffset===3 && copy.data.byteLength===23 &&
        copy.view.byteOffset===5 && copy.view.length===17 && copy.b.byteLength===65536 &&
        copy.view instanceof Uint8Array && Object.isFrozen(copy) &&
        bytes[5]===128 && api.original()===0 && answer[0]===12 && traps===0 && rejected;
    )JS"sv));
    checkpoint();
    TRY(check(u"binaryAsync && binaryCallback"sv));
    // Promise resolution must not execute a destination prototype then getter.
    TRY(check(uR"JS(
        globalThis.binaryThenTraps=0;globalThis.binaryThenRejected=false;
        __ffBinary.echoAsync(new Uint8Array([1])).then(()=>{},e=>binaryThenRejected=e instanceof TypeError);
        Object.defineProperty(Uint8Array.prototype,'then',{configurable:true,get(){binaryThenTraps++;throw 1}});
        true;
    )JS"sv));
    checkpoint();
    TRY(check(u"delete Uint8Array.prototype.then;binaryThenRejected && binaryThenTraps===0"sv));

    auto graph = TRY(Firefly::BridgeData::capture(
        source->realm()->global_object().get_without_side_effects("binaryPrivate"_utf16),
        *source->realm(), Firefly::public_binary_limits()));
    auto raw = TRY(graph.encode_wire(Firefly::public_binary_limits()));
    auto raw_value = Firefly::wire_value(target, raw);
    if (raw_value.is_error())
        return Error::from_string_literal("Cannot allocate private IPC wire");
    auto root = GC::Root<JS::Value>::create(raw_value.value(), SourceLocation::current());
    target.global_object().define_direct_property(
        "__ffWire"_utf16, raw_value.value(), JS::default_attributes);
    TRY(check(uR"JS(
        globalThis.wireTraps=0;
        for(const key of ['buffer','byteLength','byteOffset','length','constructor'])
            Object.defineProperty(__ffWire,key,{get(){wireTraps++;throw 1}});
        Object.setPrototypeOf(__ffWire,new Proxy({}, {get(){wireTraps++;throw 1}}));
        true;
    )JS"sv));
    if (TRY(Firefly::wire_bytes(raw_value.value(), target)) != raw
        || !Firefly::wire_bytes(raw_value.value(), *source->realm()).is_error())
        return Error::from_string_literal("Private byte wire lost bytes/realm isolation");
    TRY(check(u"wireTraps===0"sv));
    source->detach();
    TRY(check(uR"JS(
        let revoked=false;try{__ffBinary.echo(new Uint8Array([1]))}catch(e){revoked=e instanceof TypeError}
        revoked && __ffBinary.value[1]===128 && __ffBinary.value[2]===255;
    )JS"sv));
    outln("PASS public binary context bridge: publication/sync/Promise/callback copies, "
          "aliases/offsets, "
          "64KiB quota, no hooks/prototype then and independent writable storage");
    return {};
}
