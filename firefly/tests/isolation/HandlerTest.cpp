#include "HandlerTest.h"
#include "IsolatedWorld.h"
#include "WorldJobGuard.h"
#include "CallbackGate.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Script.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/WrapperWorld.h>
#include <LibWeb/DOM/BindingsGlue.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_handlers(JS::VM& vm, Web::DOM::Document& document)
{
    Firefly::WorldJobGuard guard(vm);
    auto& page=document.window()->principal_realm();
    auto page_check=[&](Utf16View text)->ErrorOr<void>{
        auto parsed=JS::Script::parse(Utf16String::from_utf16(text),page);
        if(parsed.is_error())return Error::from_string_literal("Invalid handler fixture");
        GC::Root script {parsed.release_value()};
        auto result=vm.run(*script);
        if(result.is_error() || !result.value().is_boolean() || !result.value().as_bool())return Error::from_string_literal("Handler page assertion failed");
        return {};
    };
    // Deliberately create the native event-handler listener in the page first.
    // Replacing its callback with a source function must still enter our guard.
    TRY(page_check(u"document.onclick=()=>{};window.onbeforeunload=()=>{};window.onerror=()=>false;true"sv));
    ScopeGuard clear([&]{MUST(page_check(u"document.onclick=null;window.onbeforeunload=null;window.onerror=null;delete globalThis.beforeUnloadProbe;true"sv));});
    {
        auto source=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*source));
        ScopeGuard detach([&]{source->detach();});
        auto setup=source->evaluate(uR"JS(
            globalThis.handlerRuns=0;globalThis.handlerValid=false;globalThis.returnTraps=0;
            document.onclick=function(event){handlerRuns++;handlerValid=this===document && event.target===document && Object.getPrototypeOf(event)===Event.prototype;return false};true;
        )JS"sv);
        if(setup.is_error())return Error::from_string_literal("Cannot install source handler");
        vm.heap().collect_garbage();
        TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===false"sv));
        auto valid=source->evaluate(u"handlerRuns===1 && handlerValid"sv);
        if(valid.is_error() || !valid.value().as_bool())return Error::from_string_literal("Handler receiver/Event wrapper or return false failed");
        auto object=source->evaluate(u"document.onclick=()=>new Proxy({},{get(){returnTraps++;throw 1}});true"sv);
        if(object.is_error())return Error::from_string_literal("Cannot install object-result handler");
        TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===true"sv));
        auto untouched=source->evaluate(u"returnTraps===0"sv);
        if(untouched.is_error() || !untouched.value().as_bool())return Error::from_string_literal("Ignored handler result invoked source traps");
        auto error_handler=source->evaluate(u"globalThis.errorValid=false;window.onerror=function(message,url,line,column,error){errorValid=arguments.length===5 && this.document===document && message==='safe-message' && error==='safe-reason';return true};true"sv);
        if(error_handler.is_error())return Error::from_string_literal("Cannot install source error handler");
        TRY(page_check(u"window.dispatchEvent(new ErrorEvent('error',{cancelable:true,message:'safe-message',error:'safe-reason'}))===false"sv));
        auto error_valid=source->evaluate(u"errorValid"sv);
        if(error_valid.is_error() || !error_valid.value().as_bool())return Error::from_string_literal("Special error handler arguments/receiver changed");
        auto errors=guard.source_job_error_count();
        auto bad=source->evaluate(u"document.onclick=()=>{throw new Proxy({},{get(){returnTraps++;throw 1}})};true"sv);
        if(bad.is_error())return Error::from_string_literal("Cannot install throwing handler");
        TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===true"sv));
        if(guard.source_job_error_count()!=errors+1)return Error::from_string_literal("Handler exception not redacted");
        auto blocked=guard.blocked_count();
        source->detach();
        TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===true"sv));
        if(guard.blocked_count()!=blocked+1)return Error::from_string_literal("Detached replacement handler not blocked before wrapping");
    }
    outln("PASS owned replacement handlers preserve receiver/Event/strict false and redact errors before page reporting");
    for(auto mode : {0,1,2,3}) {
        RefPtr<Firefly::IsolatedWorld> source=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*source));
        ScopeGuard detach([&]{if(source)source->detach();});
        GC::Root realm {*source->realm()};
        auto weak=source->make_weak_ptr();
        auto* active=source.ptr();
        auto request=JS::NativeFunction::create(*realm,[&](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{
            bool running=active->is_executing();active->detach();
            bool deferred=running && !Web::Bindings::host_defined_wrapper_world(*realm).is_detached();
            source.clear();return JS::Value(deferred);
        },0,"requestHandlerDetach"_utf16,GC::Ptr<JS::Realm>{*realm});
        realm->global_object().define_direct_property("requestHandlerDetach"_utf16,request,JS::default_attributes);
        ScopeGuard clear_request([&]{realm->global_object().define_direct_property("requestHandlerDetach"_utf16,JS::js_undefined(),JS::default_attributes);});
        TRY(page_check(u"document.documentElement.removeAttribute('data-handler-inflight');true"sv));
        auto setup=source->evaluate(mode==0 ? uR"JS(
            document.onclick=()=>{const safe=requestHandlerDetach();document.documentElement.setAttribute('data-handler-inflight',String(safe));return false};true;
        )JS"sv : mode==1 ? uR"JS(
            window.onbeforeunload=()=>({toString(){const safe=requestHandlerDetach();document.documentElement.setAttribute('data-handler-inflight',String(safe));return 'leave'}});true;
        )JS"sv : mode==2 ? uR"JS(
            window.onbeforeunload=()=>({toString(){const safe=requestHandlerDetach();document.documentElement.setAttribute('data-handler-inflight',String(safe));throw {privateCallback(){}}}});true;
        )JS"sv : uR"JS(
            window.onbeforeunload=()=>{const safe=requestHandlerDetach();document.documentElement.setAttribute('data-handler-inflight',String(safe));return {toString(){throw 1}}};true;
        )JS"sv);
        if(setup.is_error())return Error::from_string_literal("Cannot install in-flight handler");
        auto errors=guard.source_job_error_count();
        auto depth=vm.execution_context_stack().size();
        vm.heap().collect_garbage();
        if(mode==0)TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===false"sv));
        else {
            auto event=Firefly::Callbacks::make_beforeunload_probe();
            page.global_object().define_direct_property("beforeUnloadProbe"_utf16,Web::Bindings::event(page,event),JS::default_attributes);
            if(mode==3)TRY(page_check(u"beforeUnloadProbe.returnValue='existing';true"sv));
            TRY(page_check(u"window.dispatchEvent(beforeUnloadProbe)===false"sv));
            if(mode==1)TRY(page_check(u"beforeUnloadProbe.returnValue==='leave'"sv));
            if(mode==2)TRY(page_check(u"beforeUnloadProbe.returnValue===''"sv));
            if(mode==3)TRY(page_check(u"beforeUnloadProbe.returnValue==='existing'"sv));
        }
        TRY(page_check(u"document.documentElement.getAttribute('data-handler-inflight')==='true'"sv));
        if(source || weak.strong_ref() || !Web::Bindings::host_defined_wrapper_world(*realm).is_detached() || vm.execution_context_stack().size()!=depth)
            return Error::from_string_literal("In-flight handler/conversion did not release owner or restore context");
        if(guard.source_job_error_count()!=errors+(mode==2?1:0))return Error::from_string_literal("Beforeunload conversion error/skip behavior changed");
        vm.heap().collect_garbage();
    }
    outln("PASS handler/beforeunload conversion retains last owner through detach, sanitizes errors and preserves cancelation/order");
    return {};
}
