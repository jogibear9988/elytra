#include "PrincipalCallbackTest.h"
#include "callbacks/CallbackGate.h"
#include "worlds/IsolatedWorld.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Script.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/WrapperWorld.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_principal_callbacks(JS::VM& vm, Web::DOM::Document& document)
{
    Firefly::WorldJobGuard guard(vm);
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    TRY(guard.protect(*source));
    ScopeGuard detach([&] { source->detach(); });
    auto& page = document.window()->principal_realm();
    auto page_eval = [&](Utf16View text) {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), page);
        VERIFY(!parsed.is_error());
        GC::Root script { parsed.release_value() };
        return vm.run(*script);
    };

    auto page_check = [&](Utf16View text) -> ErrorOr<void> {
        auto result = page_eval(text);
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("Principal callback page assertion failed");
        return {};
    };

    TRY(page_check(uR"JS(
        globalThis.boundaryLeaks=0;globalThis.boundaryJobs=0;globalThis.boundaryCalls=0;globalThis.boundaryErrors=0;
        globalThis.boundaryChecks=[];
        globalThis.noteBoundary=()=>{
            boundaryCalls++;
            if(new Error().stack.includes('firefly:isolated-world'))boundaryLeaks++;
        };
        globalThis.boundaryFunction=function(event){
            noteBoundary();boundaryChecks.push(this===window && event.target===window);
            Promise.resolve().then(()=>boundaryJobs++);queueMicrotask(()=>boundaryJobs++);
            globalThis.boundaryInterval=setInterval(()=>{},1000);
        };
        globalThis.boundaryObject={get handleEvent(){noteBoundary();Promise.resolve().then(()=>boundaryJobs++);return function(event){noteBoundary();boundaryChecks.push(this===boundaryObject && event.currentTarget===window);Promise.resolve().then(()=>boundaryJobs++)}}};
        globalThis.boundaryProxy=new Proxy({},{get(target,key){noteBoundary();return function(){noteBoundary();Promise.resolve().then(()=>boundaryJobs++)}}});
        globalThis.boundaryThrow=()=>{noteBoundary();Promise.resolve().then(()=>boundaryJobs++);throw new Error('boundary-page-error')};
        window.addEventListener('boundary-function',boundaryFunction);
        window.addEventListener('boundary-object',boundaryObject);
        window.addEventListener('boundary-proxy',boundaryProxy);
        window.addEventListener('boundary-throw',boundaryThrow);
        window.onfocus=function(event){noteBoundary();boundaryChecks.push(this.document===document && event.target===window);Promise.resolve().then(()=>boundaryJobs++);return false};
        window.onerror=function(message,url,line,column,error){noteBoundary();boundaryErrors++;boundaryChecks.push(error instanceof Error && error.message==='boundary-page-error');Promise.resolve().then(()=>boundaryJobs++);return true};
        true;
    )JS"sv));
    ScopeGuard clear([&] {
        MUST(page_check(
            u"clearInterval(boundaryInterval);window.removeEventListener('boundary-function',boundaryFunction);window.removeEventListener('boundary-object',boundaryObject);window.removeEventListener('boundary-proxy',boundaryProxy);window.removeEventListener('boundary-throw',boundaryThrow);window.onfocus=null;window.onerror=null;true"sv));
    });
    auto report_callback
        = page_eval(u"(()=>{noteBoundary();Promise.resolve().then(()=>boundaryJobs++)})"sv);
    if (report_callback.is_error())
        return Error::from_string_literal("Cannot create page report callback");
    auto report_value = report_callback.release_value();
    GC::Root report_function { report_value.as_object() };
    auto report = JS::NativeFunction::create(
        *source->realm(),
        [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            auto completion = Firefly::Callbacks::probe_report(*report_function);
            if (completion.is_error())
                return completion.release_error();
            return completion.release_value();
        },
        0, "nativePageReport"_utf16, GC::Ptr<JS::Realm> { *source->realm() });
    source->realm()->global_object().define_direct_property(
        "nativePageReport"_utf16, report, JS::default_attributes);
    auto baseline = guard.owned_timer_count();
    auto depth = vm.execution_context_stack().size();
    auto run = source->evaluate(uR"JS(
        (function sourceDispatchBoundarySecret(){
            window.dispatchEvent(new Event('boundary-function'));
            window.dispatchEvent(new Event('boundary-object'));
            window.dispatchEvent(new Event('boundary-proxy'));
            window.dispatchEvent(new Event('boundary-throw'));
            if(window.dispatchEvent(new Event('focus',{cancelable:true}))!==false)throw Error('Page cancellation changed');
            nativePageReport();
            setTimeout(()=>document.documentElement.setAttribute('data-source-boundary-timer','BAD'),60000);
            Promise.resolve().then(()=>document.documentElement.setAttribute('data-source-boundary-job','BAD'));
        })();true;
    )JS"sv);
    if (run.is_error())
        return Error::from_string_literal("Cannot dispatch Source boundary fixture");
    auto timers = guard.owned_timer_count() - baseline;
    source->realm()->global_object().define_direct_property(
        "nativePageReport"_utf16, JS::js_undefined(), JS::default_attributes);
    source->detach();
    {
        vm.save_execution_context_stack();
        ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
        Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
    }

    auto leaks = page_eval(u"boundaryLeaks"sv);
    auto jobs = page_eval(u"boundaryJobs"sv);
    if (leaks.is_error() || jobs.is_error())
        return Error::from_string_literal("Cannot inspect boundary fixture");
    outln("OBSERVE principal boundary source_frames={} source_owned_timers={} page_jobs={}",
        leaks.value().as_double(), timers, jobs.value().as_double());
    TRY(page_check(
        u"boundaryLeaks===0 && boundaryJobs===9 && boundaryErrors===1 && boundaryCalls===9 && boundaryChecks.every(Boolean)"sv));
    if (timers != 1 || guard.owned_timer_count() != baseline
        || vm.execution_context_stack().size() != depth)
        return Error::from_string_literal(
            "Page callbacks inherited source timer ownership or context was not restored");
    TRY(page_check(
        u"document.documentElement.getAttribute('data-source-boundary-job')===null && document.documentElement.getAttribute('data-source-boundary-timer')===null"sv));
    {
        RefPtr<Firefly::IsolatedWorld> owner
            = TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*owner));
        GC::Root realm { *owner->realm() };
        auto weak = owner->make_weak_ptr();
        auto* active = owner.ptr();
        auto request = JS::NativeFunction::create(
            page,
            [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                bool executing = active->is_executing();
                active->detach();
                owner.clear();
                vm.heap().collect_garbage();
                return JS::Value(
                    executing && !Web::Bindings::host_defined_wrapper_world(*realm).is_detached());
            },
            0, "dropBoundaryOwner"_utf16, GC::Ptr<JS::Realm> { page });
        page.global_object().define_direct_property(
            "dropBoundaryOwner"_utf16, request, JS::default_attributes);
        ScopeGuard clear_drop([&] {
            page.global_object().define_direct_property(
                "dropBoundaryOwner"_utf16, JS::js_undefined(), JS::default_attributes);
        });
        TRY(page_check(
            u"globalThis.boundaryDropped=false;window.addEventListener('boundary-drop',()=>{boundaryDropped=dropBoundaryOwner();Promise.resolve().then(()=>boundaryJobs++)},{once:true});true"sv));
        auto result = active->evaluate(uR"JS(
            (()=>{const held={node:document.createElement('span'),tag:'retained'};held.node.textContent='kept';
            window.dispatchEvent(new Event('boundary-drop'));
            document.documentElement.setAttribute('data-boundary-held',held.node.textContent+':'+held.tag);
            Promise.resolve().then(()=>document.documentElement.setAttribute('data-boundary-after-drop','BAD'))})();true;
        )JS"sv);
        if (result.is_error() || owner || weak.strong_ref()
            || !Web::Bindings::host_defined_wrapper_world(*realm).is_detached())
            return Error::from_string_literal(
                "Saved source context/last owner did not survive principal reentry and unwind");
        {
            vm.save_execution_context_stack();
            ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
            Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
        }

        TRY(page_check(
            u"boundaryDropped && boundaryJobs===10 && document.documentElement.getAttribute('data-boundary-held')==='kept:retained' && document.documentElement.getAttribute('data-boundary-after-drop')===null"sv));
        if (vm.execution_context_stack().size() != depth)
            return Error::from_string_literal(
                "Reentrant page callback did not restore caller context");
    }

    outln("PASS principal callbacks/getters/proxies/handlers/reporters hide source frames and keep "
          "page jobs/timers independent");
    return {};
}
