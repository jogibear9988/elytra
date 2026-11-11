#include "FunctionBridgeTest.h"
#include "CaptureSlot.h"
#include "context/FunctionBridge.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_function_bridge(JS::VM& vm, Web::DOM::Document& document)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    auto& target = document.window()->principal_realm();
    size_t stage = 0;
    auto check = [&](Utf16View text) -> ErrorOr<void> {
        ++stage;
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), target);
        if (parsed.is_error())
            return Error::from_string_literal("Invalid function bridge fixture");
        GC::Root script { parsed.release_value() };
        auto value = vm.run(*script);
        if (value.is_error() || !value.value().is_boolean() || !value.value().as_bool()) {
            if (value.is_error() && value.error_value().is_object())
                warnln("Fixture error message: {}",
                    value.error_value()
                        .as_object()
                        .get_without_side_effects("message"_utf16)
                        .to_utf16_string_without_side_effects());
            warnln("Function bridge stage {} failed: {}", stage,
                value.is_error() ? value.error_value().to_utf16_string_without_side_effects()
                                 : value.value().to_utf16_string_without_side_effects());
            return Error::from_string_literal("Function bridge JS assertion failed");
        }

        return {};
    };

    auto function = source->evaluate(uR"JS(
        globalThis.sourceSecret='private'; globalThis.calls=0; globalThis.throwTraps=0;
        globalThis.endpoint=function(arg,other) {
            'use strict'; calls++;
            if(arg?.mode==='reenter') return {denied:nativeReenter()};
            if(arg?.mode==='throw') throw new Proxy({}, {get(){throwTraps++;throw 1}});
            if(arg?.mode==='function') return ()=>1;
            if(arg?.mode==='promise') return Promise.resolve(1);
            if(arg?.mode==='big') return new Array(65537);
            return {arg,other,alias:arg,ownThis:this===undefined,
                safe:arg && Object.getPrototypeOf(arg)===Object.prototype && Object.isFrozen(arg)};
        };
        endpoint;
    )JS"sv);
    if (function.is_error())
        return Error::from_string_literal("Cannot create source endpoint");
    auto exported = TRY(
        Firefly::FunctionBridge::create(*source, function.release_value().as_function(), document));
    GC::Root source_realm { *source->realm() };
    ScopeGuard cleanup([&] {
        source_realm->global_object().define_direct_property(
            "nativeReenter"_utf16, JS::js_undefined(), JS::default_attributes);
        source_realm->global_object().define_direct_property(
            "revokeInside"_utf16, JS::js_undefined(), JS::default_attributes);
        exported.owner->revoke();
        source->detach();
    });
    source_realm->global_object().define_native_function(*source_realm, "nativeReenter"_utf16,
        GC::create_function(vm.heap(), GC::captures(exported.function),
            [](auto& function, JS::VM& inner_vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto nested = JS::call(inner_vm, *function, JS::js_undefined());
                return JS::Value(nested.is_error());
            }),
        0, JS::default_attributes);
    for (auto code :
        { u"new Proxy(endpoint,{get(){throwTraps++;throw 1}})"sv, u"endpoint.bind(null)"sv }) {
        auto rejected = source->evaluate(code);
        if (rejected.is_error())
            return Error::from_string_literal("Invalid source rejection fixture");
        if (!Firefly::FunctionBridge::create(
                *source, rejected.release_value().as_function(), document)
                .is_error())
            return Error::from_string_literal("Proxy/bound source function was exported");
    }

    target.global_object().define_direct_property(
        "bridgeCall"_utf16, exported.function, JS::default_attributes);
    auto depth = vm.execution_context_stack().size();
    TRY(check(uR"JS(
        (()=>{
            const a={n:5}; a.self=a;
            const x=bridgeCall.call({untrusted:true},a,a);
            return x.arg!==a && x.arg===x.alias && x.arg===x.other && x.arg.self===x.arg &&
                x.arg.n===5 && x.ownThis && x.safe && !Object.isFrozen(x) && !Object.isFrozen(x.arg) &&
                Object.getPrototypeOf(x)===Object.prototype && Object.isFrozen(bridgeCall) &&
                bridgeCall.constructor('return typeof sourceSecret')()==='undefined';
        })()
    )JS"sv));
    TRY(check(uR"JS(
        (()=>{
            let traps=0, rejected=0;
            const bad=[new Proxy({}, {ownKeys(){traps++;throw 1}}),
                {get x(){traps++;throw 1}}, new Proxy(()=>1,{get(){traps++;throw 1}}), document];
            for(const x of bad)try{bridgeCall(x)}catch(e){
                if(e instanceof TypeError && e.message==='Firefly bridge call rejected')rejected++;
            }
            try{bridgeCall(...Array(65).fill(null))}catch(e){if(e instanceof TypeError)rejected++}
            return traps===0 && rejected===5;
        })()
    )JS"sv));
    auto calls = source->evaluate(u"calls===1"sv);
    if (calls.is_error() || !calls.value().as_bool())
        return Error::from_string_literal("Rejected arguments reached source");
    TRY(check(uR"JS(
        (()=>{
            let rejected=0;
            for(const mode of ['throw','function','big'])try{bridgeCall({mode})}catch(e){
                if(e instanceof TypeError && Object.getPrototypeOf(e)===TypeError.prototype &&
                    e.message==='Firefly bridge call rejected')rejected++;
            }
            return rejected===3 && bridgeCall({n:7}).arg.n===7;
        })()
    )JS"sv));
    auto traps = source->evaluate(u"throwTraps===0"sv);
    if (traps.is_error() || !traps.value().as_bool())
        return Error::from_string_literal("Exception sanitation invoked a trap");
    TRY(check(u"bridgeCall({mode:'reenter'}).denied===true"sv));
    vm.heap().collect_garbage();
    TRY(check(u"bridgeCall({n:8}).arg.n===8"sv));
    exported.owner->revoke();
    exported.owner->revoke();
    TRY(check(
        u"(()=>{try{bridgeCall({})}catch(e){return e instanceof TypeError}return false})()"sv));
    if (!exported.owner->is_revoked() || vm.execution_context_stack().size() != depth)
        return Error::from_string_literal("Revocation/context restoration failed");
    outln("PASS function bridge copies arguments/results, ignores receiver, sanitizes errors and "
          "revokes retained proxies");

    // Revoke and detach during the source frame. The in-flight call must not
    // publish its otherwise valid result or destroy the still-running context.
    RefPtr<Firefly::FunctionBridge> active;
    Firefly::Tests::CaptureSlot active_slot { active };
    Firefly::Tests::CaptureSlot source_slot { source };
    source->realm()->global_object().define_native_function(*source->realm(), "revokeInside"_utf16,
        GC::create_function(vm.heap(),
            GC::captures(Mite::native_reference(active_slot), Mite::native_reference(source_slot)),
            [](auto& active_ref, auto& source_ref, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto& active = active_ref.get().value;
                auto& source = source_ref.get().value;

                active->revoke();
                source->detach();
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
    auto reentrant = source->evaluate(u"(()=>{revokeInside();return {secret:'must not cross'}})"sv);
    if (reentrant.is_error())
        return Error::from_string_literal("Cannot create reentrant fixture");
    auto second = TRY(Firefly::FunctionBridge::create(
        *source, reentrant.release_value().as_function(), document));
    active = second.owner;
    target.global_object().define_direct_property(
        "revokingCall"_utf16, second.function, JS::default_attributes);
    TRY(check(
        u"(()=>{try{revokingCall()}catch(e){return e instanceof TypeError}return false})()"sv));
    if (!active->is_revoked() || !source->is_detached()
        || vm.execution_context_stack().size() != depth)
        return Error::from_string_literal("In-flight revocation did not complete");
    active.clear();
    vm.heap().collect_garbage();
    TRY(check(
        u"(()=>{try{revokingCall()}catch(e){return e instanceof TypeError}return false})()"sv));
    outln("PASS in-flight revoke/detach denies output and safely unwinds source context");
    return {};
}
