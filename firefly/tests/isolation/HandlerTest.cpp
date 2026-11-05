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
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/HTML/Window.h>

[[gnu::noinline]] static ErrorOr<GC::Weak<Web::DOM::EventTarget>> make_handler_cycle(Firefly::IsolatedWorld& world)
{
    auto result=world.evaluate(u"(()=>{const node=document.createElement('button');node.onclick=()=>node.id;return node})()"sv);
    if(result.is_error())return Error::from_string_literal("Cannot create detached handler cycle");
    auto value=result.release_value();
    auto* target=Web::Bindings::impl_from<Web::DOM::EventTarget>(&value.as_object());
    if(!target)return Error::from_string_literal("Missing native cyclic target");
    return GC::Weak { *target };
}

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
        auto retained=source->evaluate(u"document.onclick"sv);
        if(retained.is_error())return Error::from_string_literal("Cannot retain native handler control");
        auto retained_value=retained.release_value();
        GC::Root retained_function { retained_value.as_object() };
        auto blocked=guard.blocked_count();
        source->detach();
        TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===true"sv));
        if(guard.blocked_count()!=blocked)return Error::from_string_literal("Source handler registration survived teardown");
        auto event=Firefly::Callbacks::make_beforeunload_probe();
        auto denied=Firefly::Callbacks::probe_handler(*retained_function,*event);
        if(denied.is_error() || guard.blocked_count()!=blocked+1)return Error::from_string_literal("Retained handler not blocked before wrapping");
    }
    outln("PASS owned replacement handlers preserve receiver/Event/strict false and redact errors before page reporting");
    {
        auto first=TRY(Firefly::IsolatedWorld::create(*document.window()));
        auto second=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*first));TRY(guard.protect(*second));
        ScopeGuard detach_worlds([&]{first->detach();second->detach();});
        auto baseline=guard.owned_handler_map_count();
        TRY(page_check(uR"JS(
            globalThis.pageHandler=()=>{const e=document.documentElement;e.setAttribute('data-handler-order',e.getAttribute('data-handler-order')+'p')};
            globalThis.middleListener=()=>{const e=document.documentElement;e.setAttribute('data-handler-order',e.getAttribute('data-handler-order')+'m')};
            document.documentElement.onclick=pageHandler;true;
        )JS"sv));
        ScopeGuard clear_element([&]{MUST(page_check(u"document.documentElement.onclick=null;document.documentElement.removeAttribute('onclick');document.documentElement.removeEventListener('click',middleListener);delete globalThis.pageHandler;delete globalThis.middleListener;true"sv));});
        auto a=first->evaluate(uR"JS(
            if(document.documentElement.onclick!==null)throw Error('Page callback leaked');
            globalThis.mine=function(){const e=document.documentElement;e.setAttribute('data-handler-order',e.getAttribute('data-handler-order')+'a');return false};
            document.documentElement.onclick=mine;
            document.documentElement.onclick===mine && Object.getPrototypeOf(document.documentElement.onclick)===Function.prototype;
        )JS"sv);
        if(a.is_error() || !a.value().as_bool())return Error::from_string_literal("First handler map/getter not isolated");
        TRY(page_check(u"document.documentElement.onclick===pageHandler && (document.documentElement.addEventListener('click',middleListener),true)"sv));
        auto b=second->evaluate(uR"JS(
            if(document.documentElement.onclick!==null)throw Error('Other-world callback leaked');
            (()=>{const mine=function handlerB(){const e=document.documentElement;e.setAttribute('data-handler-order',e.getAttribute('data-handler-order')+'b');return 0};document.documentElement.onclick=mine})();
            Object.getPrototypeOf(document.documentElement.onclick)===Function.prototype;
        )JS"sv);
        if(b.is_error() || !b.value().as_bool() || guard.owned_handler_map_count()!=baseline+2)return Error::from_string_literal("Second handler map/getter not isolated");
        auto replaced=first->evaluate(u"mine=function(){const e=document.documentElement;e.setAttribute('data-handler-order',e.getAttribute('data-handler-order')+'a2');return false};document.documentElement.onclick=mine;true"sv);
        if(replaced.is_error())return Error::from_string_literal("Cannot replace private handler");
        vm.heap().collect_garbage();
        TRY(page_check(u"document.documentElement.setAttribute('data-handler-order','');document.documentElement.dispatchEvent(new Event('click',{cancelable:true}))===false && document.documentElement.getAttribute('data-handler-order')==='pa2mb' && document.documentElement.onclick===pageHandler"sv));
        auto identity=first->evaluate(u"document.documentElement.onclick===mine"sv);
        if(identity.is_error() || !identity.value().as_bool())return Error::from_string_literal("Private handler identity lost after GC");
        // DOM text attributes remain principal document behavior, including when
        // the isolated caller changes or removes the attribute.
        auto inline_set=first->evaluate(uR"JS(
            document.documentElement.setAttribute('onclick',"document.documentElement.setAttribute('data-inline-handler',typeof mine);return false");
            document.documentElement.onclick===mine;
        )JS"sv);
        if(inline_set.is_error() || !inline_set.value().as_bool())return Error::from_string_literal("Inline attribute replaced private handler");
        TRY(page_check(u"document.documentElement.dispatchEvent(new Event('click',{cancelable:true}))===false && document.documentElement.getAttribute('data-inline-handler')==='undefined' && document.documentElement.onclick!==pageHandler"sv));
        auto removed=second->evaluate(u"document.documentElement.removeAttribute('onclick');document.documentElement.onclick.name==='handlerB'"sv);
        if(removed.is_error() || !removed.value().as_bool())return Error::from_string_literal("Removing inline attribute cleared another world's handler");
        TRY(page_check(u"document.documentElement.onclick===null"sv));
        auto cleared=second->evaluate(u"document.documentElement.onclick=null;document.documentElement.onclick===null"sv);
        if(cleared.is_error() || !cleared.value().as_bool())return Error::from_string_literal("Cannot clear private handler");
        auto first_still=first->evaluate(u"document.documentElement.onclick===mine"sv);
        if(first_still.is_error() || !first_still.value().as_bool())return Error::from_string_literal("Private clear crossed worlds");
        auto restored=second->evaluate(u"document.documentElement.onclick=function(){const e=document.documentElement;e.setAttribute('data-handler-order',e.getAttribute('data-handler-order')+'b')};true"sv);
        if(restored.is_error())return Error::from_string_literal("Cannot restore second private handler");
        TRY(page_check(u"document.documentElement.onclick=pageHandler;true"sv));
        auto weak_target=TRY(make_handler_cycle(*first));
        vm.heap().collect_garbage();
        if(weak_target.ptr())return Error::from_string_literal("Handler registry retained a detached target/callback cycle");
        auto prune=first->evaluate(u"document.documentElement.onclick===mine"sv);
        if(prune.is_error() || !prune.value().as_bool() || guard.owned_handler_map_count()!=baseline+2)return Error::from_string_literal("Dead handler target metadata not pruned");
        auto canceled=guard.canceled_handler_map_count();
        first->detach();
        if(guard.owned_handler_map_count()!=baseline+1 || guard.canceled_handler_map_count()!=canceled+1)return Error::from_string_literal("First world handler maps did not clear");
        TRY(page_check(u"document.documentElement.setAttribute('data-handler-order','');document.documentElement.dispatchEvent(new Event('click',{cancelable:true}))===true && document.documentElement.getAttribute('data-handler-order')==='mbp' && document.documentElement.onclick===pageHandler"sv));
        second->detach();
        if(guard.owned_handler_map_count()!=baseline)return Error::from_string_literal("Second world handler map remained");
        TRY(page_check(u"document.documentElement.setAttribute('data-handler-order','');document.documentElement.dispatchEvent(new Event('click',{cancelable:true}))===true && document.documentElement.getAttribute('data-handler-order')==='mp' && document.documentElement.onclick===pageHandler"sv));
    }
    outln("PASS private handler maps isolate getters/identity/order across page and two worlds, preserve inline attributes and collect detached cycles");
    {
        auto first=TRY(Firefly::IsolatedWorld::create(*document.window()));
        auto second=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*first));TRY(guard.protect(*second));
        ScopeGuard detach_worlds([&]{first->detach();second->detach();});
        TRY(page_check(u"globalThis.eraseNode=document.createElement('button');eraseNode.id='erase-handler-fixture';document.body.append(eraseNode);eraseNode.onclick=()=>{};true"sv));
        ScopeGuard clear_node([&]{MUST(page_check(u"eraseNode.onclick=null;eraseNode.remove();delete globalThis.eraseNode;true"sv));});
        for(auto* owner : {first.ptr(),second.ptr()}) {
            auto installed=owner->evaluate(u"document.getElementById('erase-handler-fixture').onclick=()=>false;true"sv);
            if(installed.is_error())return Error::from_string_literal("Cannot create handler erase fixture");
        }
        auto baseline=guard.owned_handler_map_count();
        auto parsed=JS::Script::parse(u"eraseNode"_utf16,page);
        if(parsed.is_error())return Error::from_string_literal("Cannot parse native erase target");
        GC::Root script {parsed.release_value()};
        auto result=vm.run(*script);
        if(result.is_error())return Error::from_string_literal("Cannot get native erase target");
        auto value=result.release_value();
        auto* target=Web::Bindings::impl_from<Web::DOM::EventTarget>(&value.as_object());
        if(!target)return Error::from_string_literal("Missing native erase target");
        auto request=JS::NativeFunction::create(*first->realm(),[target](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{
            target->erase_all_event_listeners_and_handlers();return JS::js_undefined();
        },0,"eraseHandlerProbe"_utf16,GC::Ptr<JS::Realm>{*first->realm()});
        first->realm()->global_object().define_direct_property("eraseHandlerProbe"_utf16,request,JS::default_attributes);
        ScopeGuard clear_probe([&]{if(first->realm())first->realm()->global_object().define_direct_property("eraseHandlerProbe"_utf16,JS::js_undefined(),JS::default_attributes);});
        auto erased=first->evaluate(u"eraseHandlerProbe();true"sv);
        if(erased.is_error() || guard.owned_handler_map_count()!=baseline-2)return Error::from_string_literal("Native erase did not clear every owned target map");
        TRY(page_check(u"eraseNode.onclick===null"sv));
        for(auto* owner : {first.ptr(),second.ptr()}) {
            auto empty=owner->evaluate(u"document.getElementById('erase-handler-fixture').onclick===null"sv);
            if(empty.is_error() || !empty.value().as_bool())return Error::from_string_literal("Native erase left a private handler value");
        }
        auto rearmed=first->evaluate(u"document.getElementById('erase-handler-fixture').onclick=()=>{document.documentElement.setAttribute('data-handler-rearmed','yes');return false};true"sv);
        if(rearmed.is_error())return Error::from_string_literal("Cannot rearm erased private handler");
        TRY(page_check(u"eraseNode.onclick=()=>{};eraseNode.dispatchEvent(new Event('click',{cancelable:true}))===false && document.documentElement.getAttribute('data-handler-rearmed')==='yes'"sv));
    }
    outln("PASS native target erase clears principal and all private maps from a source context; handlers can rearm");
    {
        auto owner=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*owner));
        ScopeGuard detach_owner([&]{owner->detach();});
        size_t foreign_calls=0;
        GC::Root callback {JS::NativeFunction::create(page,[&](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{++foreign_calls;return JS::Value(false);},0,"foreignHandlerProbe"_utf16,GC::Ptr<JS::Realm>{page})};
        owner->realm()->global_object().define_direct_property("foreignHandlerProbe"_utf16,callback.ptr(),JS::default_attributes);
        ScopeGuard clear_callback([&]{if(owner->realm())owner->realm()->global_object().define_direct_property("foreignHandlerProbe"_utf16,JS::js_undefined(),JS::default_attributes);});
        auto installed=owner->evaluate(u"document.onclick=foreignHandlerProbe;document.onclick===foreignHandlerProbe"sv);
        if(installed.is_error() || !installed.value().as_bool())return Error::from_string_literal("Cannot install foreign-realm private handler control");
        TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===false"sv));
        if(foreign_calls!=1)return Error::from_string_literal("Private handler dispatch selected callback realm's map instead of owner's map");
        auto parsed=JS::Script::parse(uR"JS(
            globalThis.foreignUnloadCalls=0;globalThis.foreignUnloadLeaks=0;globalThis.foreignUnloadJobs=0;
            globalThis.foreignUnload=()=>({toString(){foreignUnloadCalls++;if(new Error().stack.includes('firefly:isolated-world'))foreignUnloadLeaks++;Promise.resolve().then(()=>foreignUnloadJobs++);return 'page-value'}});
            foreignUnload;
        )JS"_utf16,page);
        if(parsed.is_error())return Error::from_string_literal("Cannot parse foreign beforeunload fixture");
        GC::Root script {parsed.release_value()};auto function=vm.run(*script);
        if(function.is_error())return Error::from_string_literal("Cannot create foreign beforeunload function");
        owner->realm()->global_object().define_direct_property("foreignUnload"_utf16,function.release_value(),JS::default_attributes);
        ScopeGuard clear_foreign([&]{MUST(page_check(u"delete globalThis.foreignUnload;delete globalThis.foreignUnloadProbe;delete globalThis.foreignUnloadCalls;delete globalThis.foreignUnloadLeaks;delete globalThis.foreignUnloadJobs;true"sv));});
        auto registered=owner->evaluate(u"window.onbeforeunload=foreignUnload;true"sv);
        if(registered.is_error())return Error::from_string_literal("Cannot register foreign beforeunload handler");
        auto event=Firefly::Callbacks::make_beforeunload_probe();
        page.global_object().define_direct_property("foreignUnloadProbe"_utf16,Web::Bindings::event(page,event),JS::default_attributes);
        TRY(page_check(u"window.dispatchEvent(foreignUnloadProbe)===false && foreignUnloadProbe.returnValue==='page-value' && foreignUnloadCalls===1 && foreignUnloadLeaks===0"sv));
        owner->detach();
        {
            vm.save_execution_context_stack();ScopeGuard restore([&]{vm.restore_execution_context_stack();});
            Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
        }
        TRY(page_check(u"foreignUnloadJobs===1 && document.dispatchEvent(new Event('click',{cancelable:true}))===true"sv));
        if(foreign_calls!=1)return Error::from_string_literal("Foreign-realm private handler survived source cleanup");
    }
    outln("PASS private native handler listener uses storage owner's realm even for a foreign callback");
    {
        auto owner=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*owner));
        auto late=owner->evaluate(u"(()=>{const doc=document;return ()=>{doc.onclick=()=>false}})()"sv);
        if(late.is_error())return Error::from_string_literal("Cannot create late handler write control");
        auto value=late.release_value();
        GC::Root callback {value.as_function()};
        owner->detach();
        auto before=guard.owned_handler_map_count(),canceled=guard.canceled_handler_map_count();
        auto listeners=guard.owned_listener_count(),removed=guard.canceled_listener_count();
        // Native harness only: ordinary owner entry rejects evaluation after
        // detach. Even a retained raw call must not fall back to the page map.
        auto result=JS::call(vm,*callback,JS::js_undefined());
        if(result.is_error() || guard.owned_handler_map_count()!=before+1 || guard.owned_listener_count()!=listeners || guard.canceled_listener_count()!=removed+1)
            return Error::from_string_literal("Revoked native handler write did not stay private and cancel registration");
        TRY(page_check(u"document.dispatchEvent(new Event('click',{cancelable:true}))===true"sv));
        {
            vm.save_execution_context_stack();
            ScopeGuard restore([&]{vm.restore_execution_context_stack();});
            Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
        }
        if(guard.owned_handler_map_count()!=before || guard.canceled_handler_map_count()!=canceled+1)
            return Error::from_string_literal("Late revoked handler map did not clear at native checkpoint");
        vm.heap().collect_garbage();
    }
    outln("PASS revoked raw native handler writes never reach principal storage and clear after return");
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
