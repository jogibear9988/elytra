#include "WorldJobGuardTest.h"
#include "WorldJobGuard.h"
#include "FunctionBridge.h"
#include "CallbackGate.h"
#include <LibWeb/HTML/Timer.h>
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/Promise.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/PromiseConstructor.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

struct RejectionProbeCounters : public RefCounted<RejectionProbeCounters> {
    size_t rejects {0};
    size_t handles {0};
};

ErrorOr<void> check_world_job_guard(JS::VM& vm, Web::DOM::Document& document)
{
    auto tracker=adopt_ref(*new RejectionProbeCounters);
    vm.host_promise_rejection_tracker=[tracker,upstream=move(vm.host_promise_rejection_tracker)](JS::Promise& promise,JS::Promise::RejectionOperation op){
        if(op==JS::Promise::RejectionOperation::Reject)++tracker->rejects;else ++tracker->handles;
        upstream(promise,op);
    };
    Firefly::WorldJobGuard guard(vm);
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    auto other = TRY(Firefly::IsolatedWorld::create(*document.window()));
    TRY(guard.protect(*source)); TRY(guard.protect(*other));
    ScopeGuard detach([&] {source->detach();other->detach();});
    auto checkpoint=[&] {
        vm.save_execution_context_stack();
        ScopeGuard restore([&]{vm.restore_execution_context_stack();});
        Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
    };
    auto page_check=[&](Utf16View text)->ErrorOr<void> {
        auto parsed=JS::Script::parse(Utf16String::from_utf16(text),document.window()->principal_realm());
        if(parsed.is_error())return Error::from_string_literal("Invalid job guard fixture");
        GC::Root script {parsed.release_value()};
        auto result=vm.run(*script);
        if(result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("Job guard page assertion failed");
        return {};
    };
    auto first=source->evaluate(uR"JS(
        globalThis.jobRuns=0;
        globalThis.microOrder=['sync'];
        queueMicrotask(function(){'use strict';
            microOrder.push(this===undefined && arguments.length===0?'micro':'BAD');
        });
        Promise.resolve().then(()=>microOrder.push('promise'));
        queueMicrotask(()=>document.documentElement.setAttribute('data-micro-order',microOrder.join(',')));
        Promise.resolve().then(()=>{jobRuns++;document.documentElement.setAttribute('data-job','active')});
        (async()=>{await 0;document.documentElement.setAttribute('data-live-await','active')})();
        (async function*(){await 0;document.documentElement.setAttribute('data-live-generator','active');yield 1})().next();
        true;
    )JS"sv);
    if(first.is_error())return Error::from_string_literal("Cannot schedule live world job");
    checkpoint();
    TRY(page_check(u"document.documentElement.getAttribute('data-micro-order')==='sync,micro,promise' && document.documentElement.getAttribute('data-job')==='active' && document.documentElement.getAttribute('data-live-await')==='active' && document.documentElement.getAttribute('data-live-generator')==='active'"sv));
    if(guard.blocked_count()!=0)return Error::from_string_literal("Live world callback was blocked");
    auto foreign=JS::NativeFunction::create(document.window()->principal_realm(),
        +[](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{return JS::js_undefined();},
        0,"foreignJobTarget"_utf16,GC::Ptr<JS::Realm>{document.window()->principal_realm()});
    source->realm()->global_object().define_direct_property("foreignJobTarget"_utf16,foreign,JS::default_attributes);
    auto jobs=source->evaluate(uR"JS(
        globalThis.lateResolver=null;
        new Promise(resolve=>lateResolver=resolve).then(()=>document.documentElement.setAttribute('data-job','late BAD'));
        const bad=()=>document.documentElement.setAttribute('data-job','BAD');
        queueMicrotask(bad);
        queueMicrotask(bad.bind(null));
        queueMicrotask(new Proxy(bad,{}));
        queueMicrotask(new Proxy(foreignJobTarget,{apply(){bad()}}));
        Promise.resolve().then(bad);
        Promise.resolve().then(bad.bind(null));
        Promise.resolve().then(new Proxy(bad,{}));
        Promise.resolve().then(new Proxy(foreignJobTarget,{apply(){bad()}}));
        Promise.resolve({then:new Proxy(foreignJobTarget,{apply(){bad()}})});
        Promise.resolve({then(resolve){bad();resolve(1)}});
        (async()=>{await 0;bad()})();
        (async function*(){await 0;bad();yield 1})().next();
        lateResolver;
    )JS"sv);
    if(jobs.is_error())return Error::from_string_literal("Cannot schedule guarded jobs");
    auto resolver_value=jobs.release_value();
    GC::Root resolver {resolver_value.as_function()};
    // Negative control: a separate unregistered world still uses the original
    // engine hook. Its JS callback after detach calls only this harmless native
    // counter (no DOM wrappers from the detached world).
    auto control=TRY(Firefly::IsolatedWorld::create(*document.window()));
    GC::Root control_realm { *control->realm() };
    size_t control_runs=0;
    auto native=JS::NativeFunction::create(*control_realm,[&](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{++control_runs;return JS::js_undefined();},0,"controlJob"_utf16,GC::Ptr<JS::Realm>{*control_realm});
    control_realm->global_object().define_direct_property("controlJob"_utf16,native,JS::default_attributes);
    ScopeGuard clear_control([&]{control_realm->global_object().define_direct_property("controlJob"_utf16,JS::js_undefined(),JS::default_attributes);});
    auto scheduled=control->evaluate(u"Promise.resolve().then(()=>controlJob());queueMicrotask(()=>controlJob());true"sv);
    if(scheduled.is_error())return Error::from_string_literal("Cannot schedule negative job control");
    control->detach();
    source->detach();
    vm.heap().collect_garbage();
    auto live=other->evaluate(u"Promise.resolve().then(()=>document.documentElement.setAttribute('data-other-job','active')); true"sv);
    if(live.is_error())return Error::from_string_literal("Cannot schedule independent world job");
    TRY(page_check(u"globalThis.mainJobs=0;Promise.resolve().then(()=>mainJobs++);true"sv));
    checkpoint();
    TRY(page_check(u"document.documentElement.getAttribute('data-job')==='active' && document.documentElement.getAttribute('data-other-job')==='active' && mainJobs===1"sv));
    if(control_runs!=2)return Error::from_string_literal("Unregistered control did not run its original callback");
    if(guard.blocked_count()<12)return Error::from_string_literal("Expected reaction/thenable/async jobs were not blocked");
    auto blocked=guard.blocked_count();
    auto late=JS::call(vm,*resolver,JS::js_undefined(),JS::js_undefined());
    if(late.is_error())return Error::from_string_literal("Native late resolver failed");
    checkpoint();
    TRY(page_check(u"document.documentElement.getAttribute('data-job')==='active'"sv));
    if(guard.blocked_count()!=blocked+1)return Error::from_string_literal("Late queued callback bypassed revocation");
    if(!guard.protect(*source).is_error())return Error::from_string_literal("Detached world was registered again");
    // Active jobs retain their native owner even when the callback drops the
    // last application reference. Logical detach denies new jobs immediately;
    // physical wrapper/context teardown waits until the current job returns.
    for (auto mode : { 0, 1, 2, 3 }) {
        auto timers_before=guard.owned_timer_count();
        auto canceled_before=guard.canceled_timer_count();
        RefPtr<Firefly::IsolatedWorld> transient=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*transient));
        GC::Root realm { *transient->realm() };
        auto weak=transient->make_weak_ptr();
        auto* active=transient.ptr();
        auto request=JS::NativeFunction::create(*realm,[&](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{
            bool running=active->is_executing();
            active->detach();
            bool deferred=running && active->is_detached() && active->realm()==nullptr &&
                !Web::Bindings::host_defined_wrapper_world(*realm).is_detached();
            transient.clear();
            return JS::Value(deferred);
        },0,"requestAsyncDetach"_utf16,GC::Ptr<JS::Realm>{*realm});
        realm->global_object().define_direct_property("requestAsyncDetach"_utf16,request,JS::default_attributes);
        ScopeGuard clear_request([&]{realm->global_object().define_direct_property("requestAsyncDetach"_utf16,JS::js_undefined(),JS::default_attributes);});
        TRY(page_check(u"document.documentElement.removeAttribute('data-inflight');document.documentElement.removeAttribute('data-next-job');true"sv));
        auto setup=transient->evaluate(uR"JS(
            globalThis.finishJob=()=>{
                const deferred=requestAsyncDetach();
                document.documentElement.setAttribute('data-inflight',deferred?'safe':'BAD');
                setTimeout(()=>document.documentElement.setAttribute('data-late-timer','BAD'),60000);
                Promise.resolve().then(()=>document.documentElement.setAttribute('data-next-job','BAD'));
            };
            true;
        )JS"sv);
        if(setup.is_error())return Error::from_string_literal("Cannot initialize in-flight job fixture");
        auto queued=transient->evaluate(mode==0 ? u"Promise.resolve().then(finishJob);true"sv :
            mode==1 ? u"(async()=>{await 0;finishJob()})();true"sv :
            mode==3 ? u"queueMicrotask(finishJob);true"sv : u"(async function*(){await 0;finishJob();yield 1})().next();true"sv);
        if(queued.is_error())return Error::from_string_literal("Cannot schedule in-flight job fixture");
        auto depth=vm.execution_context_stack().size();
        vm.heap().collect_garbage();
        checkpoint();
        TRY(page_check(u"document.documentElement.getAttribute('data-inflight')==='safe' && document.documentElement.getAttribute('data-next-job')===null"sv));
        if(transient || weak.strong_ref() || !Web::Bindings::host_defined_wrapper_world(*realm).is_detached() || vm.execution_context_stack().size()!=depth)
            return Error::from_string_literal("In-flight job retained its owner or failed physical detach/context restoration");
        if(guard.owned_timer_count()!=timers_before || guard.canceled_timer_count()!=canceled_before+1)
            return Error::from_string_literal("Timer created after logical detach escaped physical cleanup");
        vm.heap().collect_garbage();
    }
    outln("PASS running reaction/await/generator jobs retain their owner and defer detach while denying later jobs");
    {
        auto producer=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*producer));
        auto function=producer->evaluate(u"(()=>Promise.resolve().then(()=>({n:42})))"sv);
        if(function.is_error())return Error::from_string_literal("Cannot create published-result fixture");
        auto endpoint=TRY(Firefly::FunctionBridge::create(*producer,function.release_value().as_function(),document));
        auto& principal=document.window()->principal_realm();
        principal.global_object().define_direct_property("publishedCall"_utf16,endpoint.function,JS::default_attributes);
        auto revoke=JS::NativeFunction::create(principal,[&](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{
            endpoint.owner->revoke();producer->detach();return JS::js_undefined();
        },0,"detachAfterPublish"_utf16,GC::Ptr<JS::Realm>{principal});
        principal.global_object().define_direct_property("detachAfterPublish"_utf16,revoke,JS::default_attributes);
        ScopeGuard cleanup([&]{
            principal.global_object().define_direct_property("detachAfterPublish"_utf16,JS::js_undefined(),JS::default_attributes);
            endpoint.owner->revoke();producer->detach();
        });
        TRY(page_check(uR"JS(
            globalThis.publishedResultReceived=false;
            publishedCall().then(v=>{publishedResultReceived=v.n===42 && Object.isFrozen(v)});
            // Source completion -> forwarding -> source detach -> target handler.
            Promise.resolve().then(()=>{Promise.resolve().then(()=>detachAfterPublish())});
            true;
        )JS"sv));
        checkpoint();
        TRY(page_check(u"publishedResultReceived"sv));
        if(!producer->is_detached())return Error::from_string_literal("Producer was not detached after publication");
    }
    outln("PASS approved target Promise continuations survive later source revocation");
    {
        auto errors=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*errors));
        ScopeGuard detach_errors([&]{errors->detach();});
        auto& principal=document.window()->principal_realm();
        errors->realm()->global_object().define_direct_property("ForeignPromise"_utf16,principal.intrinsics().promise_constructor(),JS::default_attributes);
        auto rejected_before=tracker->rejects,handled_before=tracker->handles;
        auto redacted_before=guard.source_rejection_count();
        auto rejected=errors->evaluate(uR"JS(
            globalThis.reasonTraps=0;
            const reason=new Proxy({}, {get(){reasonTraps++;throw 1}});
            Promise.reject(reason);
            globalThis.foreignRejected=ForeignPromise.reject(reason);
            ForeignPromise.resolve().then(()=>{throw reason});
            true;
        )JS"sv);
        if(rejected.is_error())return Error::from_string_literal("Cannot create source rejection fixtures");
        checkpoint();
        if(tracker->rejects!=rejected_before || tracker->handles!=handled_before || guard.source_rejection_count()<redacted_before+3)
            return Error::from_string_literal("Source rejection reached upstream tracker");
        auto foreign_value=errors->evaluate(u"foreignRejected"sv);
        if(foreign_value.is_error())return Error::from_string_literal("Cannot obtain native foreign-promise control");
        auto value=foreign_value.release_value();
        GC::Root promise { static_cast<JS::Promise&>(value.as_object()) };
        GC::Root ignore { JS::NativeFunction::create(principal,+[](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{return JS::js_undefined();},1,"ignoreSourceReason"_utf16,GC::Ptr<JS::Realm>{principal}) };
        {
            Web::HTML::TemporaryExecutionContext context(principal,Web::HTML::TemporaryExecutionContext::CallbacksEnabled::Yes);
            promise->perform_then(JS::js_undefined(),ignore.ptr(),nullptr);
        }
        if(tracker->handles!=handled_before)return Error::from_string_literal("Foreign source rejection handled outside source leaked upstream");
        // A page promise handled by source code must still receive its original
        // Handle operation; origin alone must not suppress an unrelated handle.
        TRY(page_check(u"globalThis.pagePromiseForSource=Promise.reject('page-control');true"sv));
        auto page_value=principal.global_object().get_without_side_effects("pagePromiseForSource"_utf16);
        errors->realm()->global_object().define_direct_property("pagePromiseForSource"_utf16,page_value,JS::default_attributes);
        auto handled=errors->evaluate(u"pagePromiseForSource.catch(()=>{});reasonTraps===0"sv);
        if(handled.is_error() || !handled.value().as_bool() || tracker->rejects!=rejected_before+1 || tracker->handles!=handled_before+1)
            return Error::from_string_literal("Page reject/handle control changed or source proxy getter ran");
        vm.heap().collect_garbage();
        checkpoint();
    }
    outln("PASS source/foreign-Promise rejections stay private without reason traps; page reject/handle tracking remains");
    {
        auto timer_world=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*timer_world));
        ScopeGuard detach_timers([&]{timer_world->detach();});
        auto baseline=guard.owned_timer_count();
        auto canceled=guard.canceled_timer_count();
        auto setup=timer_world->evaluate(u"globalThis.intervalId=setInterval(()=>{},60000);globalThis.timeoutId=setTimeout(()=>{},60000);intervalId!==timeoutId"sv);
        if(setup.is_error() || !setup.value().as_bool() || guard.owned_timer_count()!=baseline+2)
            return Error::from_string_literal("Native source timers were not registered");
        vm.heap().collect_garbage(); // Only the real Window map roots these live timers.
        auto snapshot=guard.owned_timer_snapshot();
        for(auto const& timer:snapshot)if(!Firefly::Callbacks::timer_active(*timer))return Error::from_string_literal("Native timer was not armed");
        auto cleared=timer_world->evaluate(u"clearTimeout(intervalId);clearInterval(timeoutId);true"sv);
        if(cleared.is_error() || guard.owned_timer_count()!=baseline)return Error::from_string_literal("Explicit cross-kind timer clear left metadata");
        for(auto const& timer:snapshot)if(Firefly::Callbacks::timer_active(*timer))return Error::from_string_literal("Explicit clear did not stop native timer");
        auto again=timer_world->evaluate(u"setInterval(()=>{},60000);setTimeout(()=>{},60000);true"sv);
        if(again.is_error() || guard.owned_timer_count()!=baseline+2)return Error::from_string_literal("Cannot rearm source timers");
        vm.heap().collect_garbage();
        snapshot=guard.owned_timer_snapshot();
        timer_world->detach();
        if(guard.owned_timer_count()!=baseline || guard.canceled_timer_count()!=canceled+2)
            return Error::from_string_literal("World detach did not remove owned timer entries");
        for(auto const& timer:snapshot)if(Firefly::Callbacks::timer_active(*timer))return Error::from_string_literal("Detach left native timer armed");
    }
    outln("PASS native source timeouts/intervals stop and leave the map on explicit clear and world detach, including GC");
    {
        RefPtr<Firefly::IsolatedWorld> cleanup_world=TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*cleanup_world));
        auto* active=cleanup_world.ptr();
        auto weak=cleanup_world->make_weak_ptr();
        bool hook_ran=false;
        cleanup_world->add_detach_hook([&]{
            hook_ran=active->is_detached() && active->realm()==nullptr && active->evaluate(u"true"sv).is_error();
            cleanup_world.clear();
            active->detach();
        });
        active->detach();
        if(!hook_ran || cleanup_world || weak.strong_ref())return Error::from_string_literal("Reentrant cleanup/last-owner release did not finish safely");
    }
    outln("PASS detach hooks reject evaluation, permit repeated detach and retain the last owner until cleanup completes");
    auto callback_errors_before=guard.source_job_error_count();
    auto micro_errors=other->evaluate(u"globalThis.callbackReasonTraps=0;queueMicrotask(()=>{throw new Proxy({}, {get(){callbackReasonTraps++;throw 1}})});true"sv);
    if(micro_errors.is_error())return Error::from_string_literal("Cannot schedule microtask exception fixture");
    checkpoint();
    auto traps=other->evaluate(u"callbackReasonTraps===0"sv);
    if(traps.is_error() || !traps.value().as_bool() || guard.source_job_error_count()!=callback_errors_before+1)
        return Error::from_string_literal("Source microtask exception was not redacted without getters");
    outln("PASS WebIDL queueMicrotask ordering/receiver/revocation/in-flight lifetime and exception redaction");
    other->detach();
    checkpoint();
    outln("PASS detached-world jobs blocked for reactions/bound/proxy/thenable/async/async-generator/late resolution; page and live worlds continue");
    return {};
}
