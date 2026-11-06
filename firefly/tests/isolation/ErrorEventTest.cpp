#include "ErrorEventTest.h"
#include "worlds/IsolatedWorld.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Script.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/DOM/BindingsGlue.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Event.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_error_event_values(JS::VM& vm, Web::DOM::Document& document)
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
            return Error::from_string_literal("ErrorEvent page assertion failed");
        return {};
    };

    auto world_check = [&](Utf16View text) -> ErrorOr<void> {
        auto result = source->evaluate(text);
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("ErrorEvent source assertion failed");
        return {};
    };

    auto created = source->evaluate(uR"JS(
        globalThis.sourceReason={n:42};sourceReason.self=sourceReason;
        globalThis.sourceEvent=new ErrorEvent('error',{message:'source-data',filename:'unit:source',lineno:7,colno:9,error:sourceReason,cancelable:true});
        globalThis.sourceCalls=0;
        window.onerror=function(message,url,line,column,reason){
            sourceCalls++;
            const valid=arguments.length===5 && this.document===document &&
                (message==='source-data'?reason===sourceReason && reason===sourceEvent.error:
                message==='page-data'?reason===pageEvent.error && Object.isFrozen(reason) && Object.getPrototypeOf(reason)===Object.prototype:
                message==='source-function'?typeof reason==='function':false);
            document.documentElement.setAttribute('data-error-arg',String(valid));
            return true;
        };
        sourceEvent;
    )JS"sv);
    if (created.is_error())
        return Error::from_string_literal("Cannot create source ErrorEvent");
    auto value = created.release_value();
    auto* impl = Web::Bindings::impl_from<Web::DOM::Event>(&value.as_object());
    if (!impl)
        return Error::from_string_literal("Missing ErrorEvent native implementation");
    GC::Root event { *impl };
    page.global_object().define_direct_property("sourceEvent"_utf16,
        Web::Bindings::event(page, GC::Ref { *event }), JS::default_attributes);
    TRY(page_check(uR"JS(
        globalThis.pageSourceCalls=0;globalThis.pageOwnCalls=0;globalThis.readerErrors=0;globalThis.reasonTraps=0;
        globalThis.pageReason={n:8};globalThis.pageEvent=new ErrorEvent('error',{message:'page-data',error:pageReason,cancelable:true});
        window.onerror=function(message,url,line,column,reason){
            if(message==='source-data'){
                pageSourceCalls++;
                if(arguments.length!==5 || url!=='unit:source' || line!==7 || column!==9 ||
                   reason!==sourceEvent.error || reason.n!==42 || reason.self!==reason || !Object.isFrozen(reason) || Object.getPrototypeOf(reason)!==Object.prototype)throw 'bad copied onerror argument';
            }else if(message==='page-data'){
                pageOwnCalls++;if(reason!==pageReason || Object.isFrozen(reason))throw 'page identity changed';
            }else if(reason instanceof TypeError){readerErrors++;}
            return true;
        };true;
    )JS"sv));
    ScopeGuard clear_page([&] {
        MUST(page_check(
            u"window.onerror=null;delete globalThis.sourceEvent;delete globalThis.pageEvent;delete globalThis.pageReason;delete globalThis.badEvent;true"sv));
    });
    auto page_event = page_eval(u"pageEvent"sv);
    if (page_event.is_error())
        return Error::from_string_literal("Cannot get page error event");
    auto page_value = page_event.release_value();
    auto* page_impl = Web::Bindings::impl_from<Web::DOM::Event>(&page_value.as_object());
    if (!page_impl)
        return Error::from_string_literal("Missing page event implementation");
    source->realm()->global_object().define_direct_property("pageEvent"_utf16,
        Web::Bindings::event(*source->realm(), GC::Ref { *page_impl }), JS::default_attributes);
    vm.heap().collect_garbage();
    TRY(page_check(
        u"window.dispatchEvent(sourceEvent)===false && pageSourceCalls===1 && document.documentElement.getAttribute('data-error-arg')==='true'"sv));
    TRY(world_check(u"sourceReason.n=99;sourceEvent.error===sourceReason"sv));
    TRY(page_check(
        u"window.dispatchEvent(sourceEvent)===false && pageSourceCalls===2 && sourceEvent.error.n===42"sv));
    TRY(page_check(
        u"window.dispatchEvent(pageEvent)===false && pageOwnCalls===1 && document.documentElement.getAttribute('data-error-arg')==='true'"sv));
    TRY(world_check(u"sourceCalls===3"sv));
    // Both getter and fifth-argument routes must deny functions. A page handler
    // may receive a sanitized reader TypeError from ordinary exception reporting.
    TRY(world_check(
        u"globalThis.badEvent=new ErrorEvent('error',{message:'source-function',error:()=>42,cancelable:true});window.dispatchEvent(badEvent);true"sv));
    TRY(page_check(
        u"readerErrors>=1 && document.documentElement.getAttribute('data-error-arg')==='true'"sv));
    TRY(page_check(
        u"globalThis.badEvent=new ErrorEvent('error',{message:'bad-page',error:Object.defineProperty({},'secret',{get(){reasonTraps++;throw 1}}),cancelable:true});true"sv));
    auto bad_page = page_eval(u"badEvent"sv);
    if (bad_page.is_error())
        return Error::from_string_literal("Cannot get hostile page event");
    auto bad_value = bad_page.release_value();
    auto* bad_impl = Web::Bindings::impl_from<Web::DOM::Event>(&bad_value.as_object());
    if (!bad_impl)
        return Error::from_string_literal("Missing hostile native event");
    source->realm()->global_object().define_direct_property("badPageEvent"_utf16,
        Web::Bindings::event(*source->realm(), GC::Ref { *bad_impl }), JS::default_attributes);
    TRY(world_check(
        u"(()=>{try{badPageEvent.error;return false}catch(e){return e instanceof TypeError && !e.stack.includes('firefly:isolated-world')}})()"sv));
    auto blocked = guard.source_job_error_count();
    TRY(page_check(u"window.dispatchEvent(badEvent);reasonTraps===0"sv));
    if (guard.source_job_error_count() != blocked + 1)
        return Error::from_string_literal("Special onerror argument bypassed rejected page getter");
    // Native page exception reporting while a source dispatch remains on the
    // stack must preserve the page's Error object and ordinary onerror delivery.
    TRY(page_check(
        u"window.addEventListener('native-page-failure',()=>{throw new Error('native-page-control')},{once:true});globalThis.nativePageFailure=false;window.onerror=(message,url,line,column,reason)=>{nativePageFailure=reason instanceof Error && reason.message==='native-page-control';return true};true"sv));
    TRY(world_check(u"window.dispatchEvent(new Event('native-page-failure'));true"sv));
    TRY(page_check(u"nativePageFailure"sv));
    source->detach();
    TRY(page_check(
        u"sourceEvent.error.n===42 && Object.isFrozen(sourceEvent.error) && pageEvent.error===pageReason"sv));
    outln("PASS ErrorEvent getter/onerror fifth argument use cached realm-local snapshots, reject "
          "capabilities/getters and preserve native page errors");
    return {};
}
