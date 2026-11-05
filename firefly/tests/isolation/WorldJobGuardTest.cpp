#include "WorldJobGuardTest.h"
#include "WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Script.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_world_job_guard(JS::VM& vm, Web::DOM::Document& document)
{
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
        Promise.resolve().then(()=>{jobRuns++;document.documentElement.setAttribute('data-job','active')});
        (async()=>{await 0;document.documentElement.setAttribute('data-live-await','active')})();
        (async function*(){await 0;document.documentElement.setAttribute('data-live-generator','active');yield 1})().next();
        true;
    )JS"sv);
    if(first.is_error())return Error::from_string_literal("Cannot schedule live world job");
    checkpoint();
    TRY(page_check(u"document.documentElement.getAttribute('data-job')==='active' && document.documentElement.getAttribute('data-live-await')==='active' && document.documentElement.getAttribute('data-live-generator')==='active'"sv));
    if(guard.blocked_count()!=0)return Error::from_string_literal("Live world callback was blocked");
    auto foreign=JS::NativeFunction::create(document.window()->principal_realm(),
        +[](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{return JS::js_undefined();},
        0,"foreignJobTarget"_utf16,GC::Ptr<JS::Realm>{document.window()->principal_realm()});
    source->realm()->global_object().define_direct_property("foreignJobTarget"_utf16,foreign,JS::default_attributes);
    auto jobs=source->evaluate(uR"JS(
        globalThis.lateResolver=null;
        new Promise(resolve=>lateResolver=resolve).then(()=>document.documentElement.setAttribute('data-job','late BAD'));
        const bad=()=>document.documentElement.setAttribute('data-job','BAD');
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
    auto scheduled=control->evaluate(u"Promise.resolve().then(()=>controlJob());true"sv);
    if(scheduled.is_error())return Error::from_string_literal("Cannot schedule negative job control");
    control->detach();
    source->detach();
    vm.heap().collect_garbage();
    auto live=other->evaluate(u"Promise.resolve().then(()=>document.documentElement.setAttribute('data-other-job','active')); true"sv);
    if(live.is_error())return Error::from_string_literal("Cannot schedule independent world job");
    TRY(page_check(u"globalThis.mainJobs=0;Promise.resolve().then(()=>mainJobs++);true"sv));
    checkpoint();
    TRY(page_check(u"document.documentElement.getAttribute('data-job')==='active' && document.documentElement.getAttribute('data-other-job')==='active' && mainJobs===1"sv));
    if(control_runs!=1)return Error::from_string_literal("Unregistered control did not run its original callback");
    if(guard.blocked_count()<8)return Error::from_string_literal("Expected reaction/thenable/async jobs were not blocked");
    auto blocked=guard.blocked_count();
    auto late=JS::call(vm,*resolver,JS::js_undefined(),JS::js_undefined());
    if(late.is_error())return Error::from_string_literal("Native late resolver failed");
    checkpoint();
    TRY(page_check(u"document.documentElement.getAttribute('data-job')==='active'"sv));
    if(guard.blocked_count()!=blocked+1)return Error::from_string_literal("Late queued callback bypassed revocation");
    if(!guard.protect(*source).is_error())return Error::from_string_literal("Detached world was registered again");
    other->detach();
    checkpoint();
    outln("PASS detached-world jobs blocked for reactions/bound/proxy/thenable/async/async-generator/late resolution; page and live worlds continue");
    return {};
}
