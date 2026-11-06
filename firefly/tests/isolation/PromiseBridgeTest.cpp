#include "PromiseBridgeTest.h"
#include "context/FunctionBridge.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_promise_bridge(JS::VM& vm, Web::DOM::Document& document)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    auto& target = document.window()->principal_realm();
    size_t stage = 0;
    auto check = [&](Utf16View text) -> ErrorOr<void> {
        ++stage;
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), target);
        if (parsed.is_error())
            return Error::from_string_literal("Invalid promise fixture");
        GC::Root script { parsed.release_value() };
        auto answer = vm.run(*script);
        if (answer.is_error() || !answer.value().is_boolean() || !answer.value().as_bool()) {
            warnln("Promise bridge stage {} failed", stage);
            return Error::from_string_literal("Promise bridge assertion failed");
        }

        return {};
    };

    auto function = source->evaluate(uR"JS(
        globalThis.promiseCalls=0; globalThis.promiseTraps=0; globalThis.resolvers=[];
        (function(mode){
            promiseCalls++;
            if(mode==='pending')return new Promise(resolve=>resolvers.push(resolve));
            if(mode==='reject')return Promise.reject(new Proxy({}, {get(){promiseTraps++;throw 1}}));
            if(mode==='bad')return Promise.resolve(()=>1);
            if(mode==='big')return Promise.resolve(new Array(4097));
            if(mode==='thenable')return {get then(){promiseTraps++;throw 1}};
            const p=Promise.resolve({n:42,nested:[true]});
            Object.defineProperty(p,'then',{get(){promiseTraps++;throw 1}});
            Object.defineProperty(p,'constructor',{get(){promiseTraps++;throw 1}});
            return p;
        })
    )JS"sv);
    if (function.is_error())
        return Error::from_string_literal("Cannot create promise endpoint");
    auto endpoint = TRY(
        Firefly::FunctionBridge::create(*source, function.release_value().as_function(), document));
    ScopeGuard cleanup([&] {
        endpoint.owner->revoke();
        source->detach();
    });
    target.global_object().define_direct_property(
        "promiseCall"_utf16, endpoint.function, JS::default_attributes);
    auto depth = vm.execution_context_stack().size();
    auto checkpoint = [&] {
        // Only the SDK harness's initial context is active here, no JS frame.
        // The actual renderer performs checkpoints naturally between tasks.
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
    };

    TRY(check(uR"JS(
        globalThis.promiseOrder=[]; globalThis.promiseResult=null; globalThis.promiseErrors=[];
        const p=promiseCall('ready');
        p.then(value=>{promiseOrder.push('fulfilled');promiseResult=value});
        for(const mode of ['reject','bad','big'])promiseCall(mode).catch(e=>promiseErrors.push(e));
        promiseOrder.push('sync');
        p instanceof Promise && Object.getPrototypeOf(p)===Promise.prototype && promiseOrder.join(',')==='sync';
    )JS"sv));
    checkpoint();
    TRY(check(uR"JS(
        promiseOrder.join(',')==='sync,fulfilled' && promiseResult.n===42 &&
        Object.isFrozen(promiseResult) && Object.isFrozen(promiseResult.nested) &&
        Object.getPrototypeOf(promiseResult)===Object.prototype &&
        Object.getPrototypeOf(promiseResult.nested)===Array.prototype &&
        promiseErrors.length===3 && promiseErrors.every(e=>e instanceof TypeError &&
            e.message==='Firefly bridge promise rejected' && !e.stack.includes('firefly:isolated-world'));
    )JS"sv));
    auto traps = source->evaluate(u"promiseTraps===0"sv);
    if (traps.is_error() || !traps.value().as_bool() || endpoint.owner->pending_count() != 0)
        return Error::from_string_literal(
            "Promise observer invoked getters or leaked pending entries");
    TRY(check(
        u"(()=>{try{promiseCall('thenable')}catch(e){return e instanceof TypeError}return false})()"sv));
    traps = source->evaluate(u"promiseTraps===0"sv);
    if (traps.is_error() || !traps.value().as_bool())
        return Error::from_string_literal("Thenable result getter ran");
    TRY(check(uR"JS(
        globalThis.targetThenTraps=0;globalThis.poisonRejected=false;
        Object.defineProperty(Object.prototype,'then',{get(){targetThenTraps++;throw 1},configurable:true});
        promiseCall('ready').catch(e=>{poisonRejected=e instanceof TypeError});
        true;
    )JS"sv));
    checkpoint();
    TRY(check(u"delete Object.prototype.then; poisonRejected && targetThenTraps===0"sv));
    outln("PASS promise results copy/freeze data asynchronously and bypass source and target then "
          "traps");

    TRY(check(uR"JS(
        globalThis.lateFulfilled=0;globalThis.lateRejected=0;
        for(let i=0;i<64;i++)promiseCall('pending').then(()=>lateFulfilled++,e=>{
            if(e instanceof TypeError && e.message==='Firefly bridge promise rejected')lateRejected++;
        });
        let denied=false;try{promiseCall('pending')}catch(e){denied=e instanceof TypeError}
        denied;
    )JS"sv));
    if (endpoint.owner->pending_count() != 64)
        return Error::from_string_literal("Missing pending promise budget");
    auto budget = source->evaluate(u"resolvers.length===64"sv);
    if (budget.is_error() || !budget.value().as_bool())
        return Error::from_string_literal("Over-budget call reached source");
    vm.heap().collect_garbage();
    endpoint.owner->revoke();
    endpoint.owner->revoke();
    if (endpoint.owner->pending_count() != 0)
        return Error::from_string_literal("Revoke did not release pending records");
    checkpoint();
    TRY(check(u"lateRejected===64 && lateFulfilled===0"sv));
    // Settling the source after revocation must neither resurrect roots nor
    // forward data. The realm is still live here to exercise queued reactions.
    auto late
        = source->evaluate(u"for(const resolve of resolvers)resolve({secret:'late'}); true"sv);
    if (late.is_error())
        return Error::from_string_literal("Late source fixture failed");
    vm.heap().collect_garbage();
    checkpoint();
    TRY(check(u"lateRejected===64 && lateFulfilled===0"sv));
    if (endpoint.owner->pending_count() != 0 || vm.execution_context_stack().size() != depth)
        return Error::from_string_literal("Late promise reaction changed state/context");
    outln("PASS pending promise budget, GC, revoke rejection and ignored late fulfillment");
    GC::Root source_realm { *source->realm() };
    RefPtr<Firefly::FunctionBridge> revoking;
    source_realm->global_object().define_native_function(
        *source_realm, "revokePromiseInside"_utf16,
        [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            revoking->revoke();
            source->detach();
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    ScopeGuard clear_native([&] {
        source_realm->global_object().define_direct_property(
            "revokePromiseInside"_utf16, JS::js_undefined(), JS::default_attributes);
    });
    auto revoker = source->evaluate(
        u"(mode=>{if(mode==='pending')return new Promise(()=>{});revokePromiseInside();return 7})"sv);
    if (revoker.is_error())
        return Error::from_string_literal("Invalid promise revoker fixture");
    auto second = TRY(
        Firefly::FunctionBridge::create(*source, revoker.release_value().as_function(), document));
    revoking = second.owner;
    target.global_object().define_direct_property(
        "revokingPromiseCall"_utf16, second.function, JS::default_attributes);
    TRY(check(uR"JS(
        globalThis.inFlightPromiseRejected=false;
        revokingPromiseCall('pending').catch(e=>{
            inFlightPromiseRejected=e instanceof TypeError &&
                e.message==='Firefly bridge promise rejected' && !e.stack.includes('firefly:isolated-world');
        });
        let inFlightDenied=false;try{revokingPromiseCall('revoke')}catch(e){
            inFlightDenied=e instanceof TypeError && !e.stack.includes('firefly:isolated-world');
        }
        inFlightDenied;
    )JS"sv));
    checkpoint();
    TRY(check(u"inFlightPromiseRejected"sv));
    if (!source->is_detached() || !revoking->is_revoked() || revoking->pending_count() != 0
        || vm.execution_context_stack().size() != depth)
        return Error::from_string_literal("In-flight promise revocation leaked state/context");
    outln("PASS in-source revoke/detach rejects pending promises without source stack frames");
    return {};
}
