#include "CaptureSlot.h"
/** Trusted source getter snapshots retain atomic publication, ownership and strict other
 * boundaries. */
#include "PublicationGetterTest.h"
#include "context/ContextBridge.h"
#include "serialization/BridgeData.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <AK/Utf16StringBuilder.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_publication_getters(JS::VM& vm, Web::DOM::Document& document)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    Firefly::WorldJobGuard jobs(vm);
    TRY(jobs.protect(*source));
    auto bridge = TRY(Firefly::ContextBridge::create(*source, document));
    auto& target = document.window()->principal_realm();
    source->realm()->global_object().define_direct_property(
        "contextBridge"_utf16, bridge->exports(), JS::default_attributes);
    source->realm()->global_object().define_native_function(
        *source->realm(), "getterCollect"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            vm.heap().collect_garbage();
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    source->realm()->global_object().define_native_function(*source->realm(), "getterClaim"_utf16,
        GC::create_function(vm.heap(), GC::captures(target),
            [](auto& target, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                target.global_object().define_direct_property(
                    "__ffGetterCollision"_utf16, JS::Value(17), JS::default_attributes);
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    ScopeGuard cleanup([&] { source->detach(); });
    size_t stage = 0;
    auto check = [&](Utf16View code, bool isolated = false) -> ErrorOr<void> {
        ++stage;
        Utf16StringBuilder text;
        text.append(u"{"sv);
        text.append(code);
        text.append(u"}"sv);
        auto wrapped = text.to_string();
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
            warnln("Publication getter assertion failed at stage {}", stage);
            return Error::from_string_literal("Publication getter assertion failed");
        }

        return {};
    };

    TRY(check(uR"JS(
        globalThis.publicationReads=0;globalThis.publicationSecret='source';
        globalThis.setterWrites=0;globalThis.pubConfig={env:{LABEL:'original'}};
        const fn=()=>41, api={
            get value(){++publicationReads;return this===api?42:0},
            get env(){++publicationReads;return pubConfig.env},
            get function(){++publicationReads;return fn}, alias:fn,
            get transient(){++publicationReads;return {a:{n:3},get b(){getterCollect();return {n:4}}}},
            get bytes(){++publicationReads;return new Uint8Array([0,128,255])},
            get self(){return this},
            reads(){return publicationReads},
            rejectPage(value){return value},
            resultGetter(){return {get forbidden(){throw 1}}}
        };
        Object.defineProperty(api,'setterOnly',{enumerable:true,set(value){setterWrites++}});
        contextBridge.exposeInMainWorld('__ffGetters',api);
        pubConfig.env.LABEL='later';publicationReads===5 && setterWrites===0;
    )JS"sv,
        true));
    TRY(check(uR"JS(
        const api=__ffGetters, descriptor=Object.getOwnPropertyDescriptor(api,'value');
        api.self===api && api.value===42 && api.value===42 && api.env.LABEL==='original' && api.reads()===5 &&
        api.function===api.alias && api.function()===41 && api.transient.a.n===3 && api.transient.b.n===4 &&
        Object.isFrozen(api.transient) && Object.isFrozen(api.env) && !descriptor.get &&
        !descriptor.writable && !descriptor.configurable && api.setterOnly===undefined &&
        api.bytes instanceof Uint8Array && api.bytes[2]===255 && typeof publicationSecret==='undefined';
    )JS"sv));
    TRY(check(
        u"globalThis.principalGetterCalls=0;globalThis.principalGetter=()=>{principalGetterCalls++;return 1};true"sv));
    source->realm()->global_object().define_direct_property("foreignGetter"_utf16,
        target.global_object().get_without_side_effects("principalGetter"_utf16),
        JS::default_attributes);
    TRY(check(uR"JS(
        let denied=false;try{contextBridge.exposeInMainWorld('__ffGetterForeign',Object.defineProperty({},'value',{get:foreignGetter}))}
        catch(e){denied=e instanceof TypeError}denied;
    )JS"sv,
        true));
    TRY(check(u"principalGetterCalls===0 && !Object.hasOwn(globalThis,'__ffGetterForeign')"sv));
    auto count = bridge->function_count();
    TRY(check(uR"JS(
        globalThis.failedGetterCalls=0;globalThis.failedGetterTraps=0;
        for(const value of [
            {good:()=>1,get bad(){failedGetterCalls++;throw new Proxy({}, {get(){failedGetterTraps++;throw 1}})}},
            {good:()=>1,get bad(){failedGetterCalls++;return new Proxy({}, {ownKeys(){failedGetterTraps++;throw 1}})}},
            Object.defineProperty({good:()=>1},'bad',{enumerable:true,get:new Proxy(()=>1,{apply(){failedGetterTraps++;throw 1}})}),
            Object.defineProperty({good:()=>1},'bad',{enumerable:true,get:(()=>1).bind(null)}),
            Object.defineProperty({good:()=>1},'bad',{enumerable:true,get:getterCollect})
        ]) {
            let rejected=false;try{contextBridge.exposeInMainWorld('__ffGetterFailed',value)}
            catch(e){rejected=e instanceof TypeError}
            if(!rejected)throw Error('Unsupported getter accepted');
        }
        failedGetterCalls===2 && failedGetterTraps===0;
    )JS"sv,
        true));
    if (bridge->function_count() != count)
        return Error::from_string_literal("Failed getter publication leaked function slots");
    TRY(check(u"!Object.hasOwn(globalThis,'__ffGetterFailed')"sv));
    TRY(check(uR"JS(
        let traps=0,rejected=0;
        try{__ffGetters.rejectPage({get x(){traps++;throw 1}})}catch(e){if(e instanceof TypeError)rejected++}
        try{__ffGetters.resultGetter()}catch(e){if(e instanceof TypeError)rejected++}
        traps===0 && rejected===2;
    )JS"sv));
    TRY(check(uR"JS(
        globalThis.reentrantPublication=false;
        contextBridge.exposeInMainWorld('__ffGetterReentry', {get value(){
            try{contextBridge.exposeInMainWorld('__ffGetterNested',{fn(){}})}
            catch(e){reentrantPublication=e instanceof TypeError}
            return 1;
        }});
        let collided=false;
        try{contextBridge.exposeInMainWorld('__ffGetterCollision',{good:()=>1,get value(){getterClaim();return 1}})}
        catch(e){collided=e instanceof TypeError}
        reentrantPublication && collided;
    )JS"sv,
        true));
    TRY(check(
        u"__ffGetterReentry.value===1 && !Object.hasOwn(globalThis,'__ffGetterNested') && __ffGetterCollision===17"sv));
    TRY(check(uR"JS(
        globalThis.getterBudget=0;
        const limited={};for(let i=0;i<64;i++)Object.defineProperty(limited,'g'+i,{enumerable:true,get(){getterBudget++;return i}});
        contextBridge.exposeInMainWorld('__ffGetterLimit',limited);
        const excessive={good:()=>1};for(let i=0;i<65;i++)Object.defineProperty(excessive,'g'+i,{enumerable:true,get(){getterBudget++;return i}});
        let rejected=false;try{contextBridge.exposeInMainWorld('__ffGetterOver',excessive)}catch(e){rejected=e instanceof TypeError}
        rejected && getterBudget===128;
    )JS"sv,
        true));
    TRY(check(u"__ffGetterLimit.g63===63 && !Object.hasOwn(globalThis,'__ffGetterOver')"sv));
    if (bridge->function_count() != count)
        return Error::from_string_literal("Getter quota/collision consumed function slots");
    // Revoke while a getter is running; inert source effects remain, no API may commit.
    source->realm()->global_object().define_native_function(*source->realm(), "getterRevoke"_utf16,
        GC::create_function(vm.heap(), GC::captures(bridge->make_weak_ptr()),
            [](auto& bridge, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                bridge->revoke();
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    TRY(check(uR"JS(
        let rejected=false;try{contextBridge.exposeInMainWorld('__ffGetterRevoked',{get value(){getterRevoke();return 1}})}
        catch(e){rejected=e instanceof TypeError}rejected;
    )JS"sv,
        true));
    TRY(check(
        u"!Object.hasOwn(globalThis,'__ffGetterRevoked') && __ffGetters.env.LABEL==='original'"sv));
    outln("PASS trusted publication getters: copied values/receiver/functions/binary, GC, 64-call "
          "quota, atomic rollback/collision/reentry/revoke and strict other directions");
    return {};
}
