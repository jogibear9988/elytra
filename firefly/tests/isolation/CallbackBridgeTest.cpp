/** Actual principal/isolated LibJS calls: identity, no source frames, weak lifetime and
 * cancellation. */
#include "CallbackBridgeTest.h"
#include "context/ContextBridge.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <AK/Utf16StringBuilder.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_callback_bridge(JS::VM& vm, Web::DOM::Document& document)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    Firefly::WorldJobGuard jobs(vm);
    TRY(jobs.protect(*source));
    auto bridge = TRY(Firefly::ContextBridge::create(*source, document));
    auto& principal = document.window()->principal_realm();
    source->realm()->global_object().define_direct_property(
        "contextBridge"_utf16, bridge->exports(), JS::default_attributes);
    ScopeGuard cleanup([&] {
        bridge->revoke();
        source->detach();
    });
    size_t stage = 0;
    auto check = [&](Utf16View code, bool isolated = false) -> ErrorOr<void> {
        ++stage;
        Utf16StringBuilder builder;
        builder.append(u"{"sv);
        builder.append(code);
        builder.append(u"}"sv);
        auto wrapped = builder.to_string();
        auto answer = [&]() -> JS::ThrowCompletionOr<JS::Value> {
            if (isolated)
                return source->evaluate(wrapped);
            auto parsed = JS::Script::parse(wrapped, principal);
            if (parsed.is_error())
                return JS::js_undefined();
            GC::Root script { parsed.release_value() };
            return vm.run(*script);
        }();
        if (answer.is_error() || !answer.value().is_boolean() || !answer.value().as_bool()) {
            warnln("Callback bridge stage {} failed{}", stage,
                isolated ? " in source" : " in principal");
            if (answer.is_error() && answer.error_value().is_object())
                warnln("Callback fixture message: {}",
                    answer.error_value()
                        .as_object()
                        .get_without_side_effects("message"_utf16)
                        .to_utf16_string_without_side_effects());
            return Error::from_string_literal("Callback bridge assertion failed");
        }

        return {};
    };

    auto checkpoint = [&] {
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
    };

    auto depth = vm.execution_context_stack().size();
    TRY(check(uR"JS(
        globalThis.cbPrivateSecret='private'; globalThis.cbCurrent=null; globalThis.cbListeners=[];
        globalThis.markCallbackRejected=e=>{globalThis.callbackRevoked=e instanceof TypeError};
        contextBridge.exposeInMainWorld('__ffCallbacks', {
            set(cb){ cbCurrent=cb; return true; }, same(cb){ return cbCurrent===cb; },
            clear(){ cbCurrent=null; cbListeners=[]; return true; },
            call:function privateSourceCallbackInvoker(value){ return cbCurrent(value,value); },
            alias(value){ return value.a===value.b; },
            on(cb){ cbListeners.push(cb); }, off(cb){ cbListeners=cbListeners.filter(x=>x!==cb); },
            fire(value){ for(const cb of cbListeners) cb(value); return true; },
            immediate(cb,value){ return cb(value); },
            recurse(cb,n){ return n ? cb(n-1) : 'done'; },
            awaitCallback(cb){ return cb({n:4}).then(value=>({n:value.n+1})); },
            stashPromise(cb){ globalThis.heldCallbackPromise=cb(); heldCallbackPromise.catch(markCallbackRejected); return true; }
        }); true;
    )JS"sv,
        true));
    TRY(check(uR"JS(
        globalThis.cbCalls=0; globalThis.callback=function(a,b){
            'use strict'; ++cbCalls;
            return {n:a.n+1,same:a===b,frozen:Object.isFrozen(a),cycle:a.self===a,
                receiver:this===undefined,realm:Object.getPrototypeOf(a)===Object.prototype,
                secret:a.constructor.constructor('return typeof cbPrivateSecret')(),stack:new Error().stack};
        };
        const value={n:4};value.self=value;
        const api=__ffCallbacks;
        api.set(callback);
        const result=api.call(value);
        result.n===5 && result.same && result.frozen && result.cycle && result.receiver && result.realm &&
            result.secret==='undefined' && !result.stack.includes('privateSourceCallbackInvoker') &&
            api.same(callback) && api.alias({a:callback,b:callback}) && cbCalls===1;
    )JS"sv));
    TRY(check(
        u"Object.isFrozen(cbCurrent) && cbCurrent.constructor('return typeof cbPrivateSecret')()==='string'"sv,
        true));
    TRY(check(uR"JS(
        globalThis.frontEvents=0; globalThis.frontListener=()=>{++frontEvents};
        const api=__ffCallbacks;
        api.on(frontListener); api.fire(1); api.off(frontListener); api.fire(2);
        frontEvents===1 && api.same(callback);
    )JS"sv));
    vm.heap().collect_garbage();
    if (bridge->callback_count() != 1)
        return Error::from_string_literal("Weak callback cache retained an unregistered proxy");
    auto count = bridge->callback_count();
    TRY(check(uR"JS(
        let traps=0, rejected=0; const api=__ffCallbacks;
        for(const arg of [new Proxy(()=>1,{get(){traps++;throw 1}}),callback.bind(null),
            {good:()=>1,get bad(){traps++;throw 1}},document]) {
            try { api.alias(arg); } catch(e) { if(e instanceof TypeError) ++rejected; }
        }
        rejected===4 && traps===0 && api.same(callback);
    )JS"sv));
    if (bridge->callback_count() != count)
        return Error::from_string_literal(
            "Failed capture consumed callback slots or revoked an existing one");
    TRY(check(uR"JS(
        globalThis.callbackTraps=0;
        let rejected=0;
        for (const cb of [()=>{throw new Proxy({}, {get(){callbackTraps++;throw 1}})},
            ()=>({get value(){callbackTraps++;throw 1}})]) {
            try { __ffCallbacks.immediate(cb,{}); } catch(e) { if(e instanceof TypeError) ++rejected; }
        }
        rejected===2 && callbackTraps===0;
    )JS"sv));
    TRY(check(uR"JS(
        function again(n){ return __ffCallbacks.recurse(again,n); }
        let rejected=false;
        try { __ffCallbacks.recurse(again,40); } catch(e) { rejected=e instanceof TypeError; }
        rejected && __ffCallbacks.recurse(again,4)==='done' && __ffCallbacks.clear();
    )JS"sv));
    vm.heap().collect_garbage();
    if (bridge->callback_count() != 0 || vm.execution_context_stack().size() != depth)
        return Error::from_string_literal("Callback GC/context restoration failed");
    // Repeated application subscriptions must reclaim weak cache slots after collection.
    for (size_t pass = 0; pass < 3; ++pass) {
        TRY(check(uR"JS(
            const callbacks=Array.from({length:64},()=>()=>1),api=__ffCallbacks;
            for(const cb of callbacks) api.on(cb);
            let rejected=false;try { api.on(()=>1); } catch(e){rejected=e instanceof TypeError}
            api.clear(); rejected;
        )JS"sv));
        vm.heap().collect_garbage();
        if (bridge->callback_count())
            return Error::from_string_literal("Unused callback cache slots were not reclaimed");
    }

    TRY(check(uR"JS(
        globalThis.callbackTurn='before'; globalThis.callbackOuterDone=false;
        __ffCallbacks.immediate(()=>{
            Promise.resolve().then(()=>{callbackTurn=callbackOuterDone?'after':'early'});
            return 1;
        });
        callbackOuterDone=true; callbackTurn==='before';
    )JS"sv));
    checkpoint();
    TRY(check(u"callbackTurn==='after'"sv));

    TRY(check(uR"JS(
        const p=new Promise(resolve=>globalThis.massCallbackResolve=resolve), cb=()=>p;
        for(let i=0;i<64;i++) __ffCallbacks.stashPromise(cb);
        let capped=false;
        try { __ffCallbacks.stashPromise(cb); } catch(e) { capped=e instanceof TypeError; }
        massCallbackResolve(7); capped;
    )JS"sv));
    checkpoint();
    vm.heap().collect_garbage();
    if (bridge->callback_count())
        return Error::from_string_literal("Settled callback budget retained an endpoint");

    TRY(check(uR"JS(
        globalThis.callbackAsync='pending'; globalThis.callbackPromiseTraps=0;
        __ffCallbacks.awaitCallback(value=>{
            const p=Promise.resolve({n:value.n*2});
            Object.defineProperty(p,'then',{get(){callbackPromiseTraps++;throw 1}});
            Object.defineProperty(p,'constructor',{get(){callbackPromiseTraps++;throw 1}});
            return p;
        }).then(value=>callbackAsync=value.n,()=>callbackAsync='failed');
        callbackAsync==='pending';
    )JS"sv));
    checkpoint();
    TRY(check(u"callbackAsync===9 && callbackPromiseTraps===0"sv));
    TRY(check(uR"JS(
        globalThis.callbackReject=false;
        __ffCallbacks.awaitCallback(()=>Promise.reject(new Proxy({}, {get(){callbackPromiseTraps++;throw 1}})))
            .then(()=>{},e=>callbackReject=e instanceof TypeError);
        true;
    )JS"sv));
    checkpoint();
    TRY(check(u"callbackReject && callbackPromiseTraps===0"sv));
    vm.heap().collect_garbage();
    TRY(check(uR"JS(
        globalThis.principalTail=false;
        __ffCallbacks.stashPromise(()=>new Promise(resolve=>globalThis.callbackResolve=resolve)
            .then(value=>{principalTail=true;return value}));
        true;
    )JS"sv));
    vm.heap().collect_garbage();
    // The proxy can die while its principal Promise still owns a reaction. It must
    // remain in the native revocation registry until that asynchronous endpoint dies.
    if (bridge->callback_count() != 1 || bridge->callback_proxy_count() != 0)
        return Error::from_string_literal(
            "Pending callback/proxy weak lifetime was not established");
    bridge->revoke();
    checkpoint();
    TRY(check(u"callbackRevoked===true"sv, true));
    source->detach();
    TRY(check(u"callbackResolve({n:9}); true;"sv));
    checkpoint();
    TRY(check(u"principalTail===true"sv));
    if (bridge->callback_count() || vm.execution_context_stack().size() != depth)
        return Error::from_string_literal("Callback revocation retained state/frames");
    outln("PASS actual callback bridge identity/aliases/sync reentry/weak GC/atomic "
          "budgets/Promise sanitation and detached cancellation");
    return {};
}
