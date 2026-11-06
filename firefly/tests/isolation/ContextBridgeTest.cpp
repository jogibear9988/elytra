#include "ContextBridgeTest.h"
#include "context/ContextBridge.h"
#include <AK/ScopeGuard.h>
#include <AK/Utf16StringBuilder.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_context_bridge(JS::VM& vm, Web::DOM::Document& document)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    auto owner = TRY(Firefly::ContextBridge::create(*source, document));
    auto& target = document.window()->principal_realm();
    GC::Root exports { owner->exports() };
    source->realm()->global_object().define_direct_property(
        "contextBridge"_utf16, exports, JS::default_attributes);
    ScopeGuard cleanup([&] {
        owner->revoke();
        source->detach();
    });
    size_t stage = 0;
    auto check = [&](Utf16View code, bool isolated = false) -> ErrorOr<void> {
        ++stage;
        Utf16StringBuilder wrapped;
        wrapped.append(u"{"sv);
        wrapped.append(code);
        wrapped.append(u"}"sv);
        auto text = wrapped.to_string();
        auto result = [&]() -> JS::ThrowCompletionOr<JS::Value> {
            if (isolated)
                return source->evaluate(text);
            auto parsed = JS::Script::parse(text, target);
            if (parsed.is_error())
                return JS::js_undefined();
            GC::Root script { parsed.release_value() };
            return vm.run(*script);
        }();
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool()) {
            warnln("Context bridge assertion failed at stage {}", stage);
            return Error::from_string_literal("Context bridge assertion failed");
        }

        return {};
    };

    TRY(check(uR"JS(
        globalThis.apiSecret=41;
        const fn=value=>({n:apiSecret+value});
        const api={version:1,nested:{fn,alias:fn},array:[fn],pending:()=>new Promise(()=>{}),fail:()=>{throw Error('private-source-error')}};
        api.self=api;
        Object.defineProperty(api,'__proto__',{value:{safe:true},enumerable:true});
        contextBridge.exposeInMainWorld('__ffContextApi',api);
        api.version=99;
        contextBridge.exposeInMainWorld('__ffContextScalar',42);
        Object.isFrozen(contextBridge) && contextBridge.exposeInMainWorld.length===2;
    )JS"sv,
        true));
    vm.heap().collect_garbage();
    TRY(check(uR"JS(
        const api=__ffContextApi,descriptor=Object.getOwnPropertyDescriptor(globalThis,'__ffContextApi');
        api.version===1 && api.self===api && api.nested.fn===api.nested.alias && api.array[0]===api.nested.fn &&
        Object.isFrozen(api) && Object.isFrozen(api.nested) && Object.isFrozen(api.array) && Object.isFrozen(api.nested.fn) &&
        Object.getPrototypeOf(api)===Object.prototype && Object.getPrototypeOf(api.array)===Array.prototype &&
        Object.getPrototypeOf(api.nested.fn)===Function.prototype && api.__proto__.safe===true &&
        api.nested.fn(1).n===42 && Object.isFrozen(api.nested.fn(1)) &&
        descriptor.enumerable && !descriptor.writable && !descriptor.configurable && __ffContextScalar===42 &&
        typeof apiSecret==='undefined' && typeof contextBridge==='undefined';
    )JS"sv));
    auto count = owner->function_count();
    if (count != 3)
        return Error::from_string_literal("Duplicate API functions were not shared");
    TRY(check(uR"JS(
        globalThis.apiTraps=0;
        const failures=[
          {good:()=>1,get bad(){apiTraps++;throw 1}},
          {good:()=>1,bad:new Proxy({}, {ownKeys(){apiTraps++;throw 1},get(){apiTraps++;throw 1}})},
          {good:()=>1,bad:new Proxy(()=>1,{get(){apiTraps++;throw 1}})},
          {bad:(()=>1).bind(null)}, {bad:Object.create({})}, {bad:Symbol()}, {bad:1n}, {bad:Promise.resolve(1)},
          Object.fromEntries(Array.from({length:65},(_,i)=>['f'+i,()=>i])),
          {good:()=>1,bad:new Array(4097)}
        ];
        for(const api of failures){let rejected=false;try{contextBridge.exposeInMainWorld('__ffFailedApi',api)}catch(e){rejected=e instanceof TypeError}if(!rejected)throw Error('Unsafe API accepted');}
        let duplicate=false;try{contextBridge.exposeInMainWorld('__ffContextApi',{n:1})}catch(e){duplicate=e instanceof TypeError}
        apiTraps===0 && duplicate;
    )JS"sv,
        true));
    TRY(check(uR"JS(
        for(const key of [undefined,null,1,Symbol(),'', 'x'.repeat(257)]){
            let rejected=false;try{contextBridge.exposeInMainWorld(key,1)}catch(e){rejected=e instanceof TypeError}
            if(!rejected)throw Error('Invalid namespace accepted');
        }
        true;
    )JS"sv,
        true));
    if (owner->function_count() != count)
        return Error::from_string_literal("Failed publication leaked function endpoints");
    TRY(check(u"!Object.hasOwn(globalThis,'__ffFailedApi') && __ffContextApi.version===1"sv));
    TRY(check(uR"JS(
        globalThis.pageApiTraps=0;
        Object.defineProperty(globalThis,'__ffOccupiedApi',{get(){pageApiTraps++;throw 1},configurable:true});
        Object.defineProperty(Object.prototype,'__ffInheritedApi',{set(){pageApiTraps++;throw 1},configurable:true});
        true;
    )JS"sv));
    ScopeGuard restore([&] {
        (void)check(
            u"delete globalThis.__ffOccupiedApi; delete Object.prototype.__ffInheritedApi; true"sv);
    });
    TRY(check(uR"JS(
        let rejected=false;try{contextBridge.exposeInMainWorld('__ffOccupiedApi',{n:1})}catch(e){rejected=e instanceof TypeError}
        contextBridge.exposeInMainWorld('__ffInheritedApi',{n:42});
        rejected;
    )JS"sv,
        true));
    TRY(check(
        u"pageApiTraps===0 && Object.hasOwn(globalThis,'__ffInheritedApi') && __ffInheritedApi.n===42"sv));
    // Even a deliberately leaked publisher cannot enter a live source owner
    // from the page: native function realm metadata alone is not authority.
    auto publisher = exports->get_without_side_effects("exposeInMainWorld"_utf16);
    JS::Value arguments[] { JS::PrimitiveString::create(vm, "__ffForeignApi"_utf16), JS::Value(1) };
    if (!JS::call(vm, publisher, exports, ReadonlySpan<JS::Value> { arguments }).is_error())
        return Error::from_string_literal(
            "Publisher accepted a call outside its source execution scope");
    TRY(check(u"!Object.hasOwn(globalThis,'__ffForeignApi')"sv));
    target.global_object().define_direct_property(
        "__ffLeakedPublisher"_utf16, publisher, JS::default_attributes);
    source->realm()->global_object().define_native_function(
        *source->realm(), "principalPublicationProbe"_utf16,
        [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            auto parsed = JS::Script::parse(uR"JS(
                (()=>{let rejected=0;
                    for(const run of [()=>__ffLeakedPublisher('__ffBadReentry',1),
                        ()=>__ffLeakedPublisher.call(null,'__ffBadReentry',1),
                        ()=>Reflect.apply(__ffLeakedPublisher,null,['__ffBadReentry',1])]){
                        try{run()}catch(e){if(e.name==='TypeError')rejected++}
                    }
                    return rejected===3 && !Object.hasOwn(globalThis,'__ffBadReentry');
                })()
            )JS"_utf16,
                target);
            if (parsed.is_error())
                return JS::js_undefined();
            GC::Root script { parsed.release_value() };
            return vm.run(*script);
        },
        0, JS::default_attributes);
    TRY(check(u"principalPublicationProbe()"sv, true));
    source->realm()->global_object().define_direct_property(
        "principalPublicationProbe"_utf16, JS::js_undefined(), JS::default_attributes);
    TRY(check(u"delete globalThis.__ffLeakedPublisher;true"sv));
    TRY(check(
        u"globalThis.contextRevoked=false;__ffContextApi.pending().catch(e=>contextRevoked=e instanceof TypeError);true"sv));
    source->detach(); // native detach hook must revoke the whole publication owner.
    owner->revoke();
    owner->revoke();
    if (owner->function_count())
        return Error::from_string_literal("Detached publication retained endpoint roots");
    vm.save_execution_context_stack();
    Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
    vm.restore_execution_context_stack();
    vm.heap().collect_garbage();
    TRY(check(uR"JS(
        let denied=false;try{__ffContextApi.nested.fn(1)}catch(e){denied=e instanceof TypeError && !e.stack.includes('private-source-error')}
        denied && contextRevoked && __ffContextApi.version===1;
    )JS"sv));
    outln("PASS atomic nested context APIs: frozen copies/cycles/function aliases, "
          "traps/rollback/budgets and detach revocation");
    return {};
}
