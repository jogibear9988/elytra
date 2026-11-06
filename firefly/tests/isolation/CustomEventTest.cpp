#include "CustomEventTest.h"
#include "worlds/IsolatedWorld.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Script.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/DOM/BindingsGlue.h>
#include <LibWeb/DOM/CustomEvent.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

[[gnu::noinline]] static ErrorOr<GC::Weak<JS::Object>> weak_detail(Firefly::IsolatedWorld& world)
{
    auto result = world.evaluate(u"eventProbe.detail"sv);
    if (result.is_error())
        return Error::from_string_literal("Cannot get cached event value");
    auto value = result.release_value();
    if (!value.is_object())
        return Error::from_string_literal("Missing event data object");
    return GC::Weak { value.as_object() };
}

struct ScalarProbe {
    GC::Root<Web::DOM::CustomEvent> event;
    GC::Weak<JS::Realm> writer;
};

[[gnu::noinline]] static ErrorOr<ScalarProbe> scalar_event(Firefly::IsolatedWorld& world)
{
    auto result = world.evaluate(u"new CustomEvent('scalar',{detail:Symbol('private')})"sv);
    if (result.is_error())
        return Error::from_string_literal("Cannot create pre-registration event");
    auto value = result.release_value();
    auto* event = Web::Bindings::impl_from<Web::DOM::CustomEvent>(&value.as_object());
    if (!event)
        return Error::from_string_literal("Missing scalar event");
    return ScalarProbe { GC::Root { *event }, GC::Weak { *world.realm() } };
}

ErrorOr<void> check_custom_event_values(JS::VM& vm, Web::DOM::Document& document)
{
    Firefly::WorldJobGuard guard(vm);
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    auto target = TRY(Firefly::IsolatedWorld::create(*document.window()));
    TRY(guard.protect(*source));
    TRY(guard.protect(*target));
    ScopeGuard detach([&] {
        source->detach();
        target->detach();
    });
    auto& page = document.window()->principal_realm();
    auto page_check = [&](Utf16View text) -> ErrorOr<void> {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), page);
        if (parsed.is_error())
            return Error::from_string_literal("Invalid CustomEvent page fixture");
        GC::Root script { parsed.release_value() };
        auto result = vm.run(*script);
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("CustomEvent page assertion failed");
        return {};
    };

    auto world_check = [](Firefly::IsolatedWorld& world, Utf16View text) -> ErrorOr<void> {
        auto result = world.evaluate(text);
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("CustomEvent world assertion failed");
        return {};
    };

    auto created = source->evaluate(uR"JS(
        globalThis.data={n:42,items:[1,,3]};data.self=data;data.alias=data.items;
        globalThis.eventProbe=new CustomEvent('detail-probe',{detail:data});
        eventProbe;
    )JS"sv);
    if (created.is_error())
        return Error::from_string_literal("Cannot create source CustomEvent");
    auto created_value = created.release_value();
    auto* native = Web::Bindings::impl_from<Web::DOM::CustomEvent>(&created_value.as_object());
    if (!native)
        return Error::from_string_literal("Missing native CustomEvent");
    GC::Root event { *native };
    page.global_object().define_direct_property(
        "eventProbe"_utf16, Web::Bindings::event(page, GC::Ref { *event }), JS::default_attributes);
    target->realm()->global_object().define_direct_property("eventProbe"_utf16,
        Web::Bindings::event(*target->realm(), GC::Ref { *event }), JS::default_attributes);
    ScopeGuard clear_page([&] {
        MUST(page_check(
            u"delete globalThis.eventProbe;delete globalThis.cachedDetail;delete globalThis.pageEvent;delete globalThis.pageData;true"sv));
    });
    TRY(world_check(*source, u"eventProbe.detail===data && !Object.isFrozen(data)"sv));
    TRY(page_check(uR"JS(
        globalThis.cachedDetail=eventProbe.detail;
        cachedDetail===eventProbe.detail && cachedDetail.n===42 && cachedDetail.self===cachedDetail &&
        cachedDetail.alias===cachedDetail.items && !(1 in cachedDetail.items) &&
        Object.getPrototypeOf(cachedDetail)===Object.prototype && Object.getPrototypeOf(cachedDetail.items)===Array.prototype &&
        Object.isFrozen(cachedDetail) && Object.isFrozen(cachedDetail.items);
    )JS"sv));
    TRY(world_check(*target,
        u"(()=>{const v=eventProbe.detail;return v===eventProbe.detail && v.n===42 && v.self===v && Object.getPrototypeOf(v)===Object.prototype && Object.isFrozen(v)})()"sv));
    auto weak = TRY(weak_detail(*target));
    vm.heap().collect_garbage();
    if (!weak.ptr())
        return Error::from_string_literal("Native CustomEvent did not trace its cached copy");
    TRY(world_check(*source, u"data.n=99;eventProbe.detail===data && eventProbe.detail.n===99"sv));
    TRY(page_check(u"eventProbe.detail===cachedDetail && cachedDetail.n===42"sv));
    TRY(world_check(*source, uR"JS(
        document.addEventListener('detail-probe',event=>{
            event.initCustomEvent('changed-during-dispatch',false,false,{bad:true});
            document.documentElement.setAttribute('data-detail-dispatch',String(event.type==='detail-probe' && event.detail===data));
        });true;
    )JS"sv));
    TRY(page_check(
        u"document.dispatchEvent(eventProbe);eventProbe.type==='detail-probe' && eventProbe.detail===cachedDetail && document.documentElement.getAttribute('data-detail-dispatch')==='true'"sv));
    TRY(world_check(
        *source, u"eventProbe.initCustomEvent('detail-probe',false,false,{next:7});true"sv));
    TRY(page_check(
        u"eventProbe.detail!==cachedDetail && eventProbe.detail.next===7 && eventProbe.detail===eventProbe.detail && Object.isFrozen(eventProbe.detail)"sv));
    TRY(world_check(
        *target, u"eventProbe.detail.next===7 && eventProbe.detail===eventProbe.detail"sv));
    // Rejected values must not run source traps and must throw the reader's
    // generic TypeError, not propagate source error objects/functions.
    for (auto code :
        { u"eventProbe.initCustomEvent('detail-probe',false,false,()=>42);true"sv,
            u"eventProbe.initCustomEvent('detail-probe',false,false,new Proxy({}, {get(){traps++;throw 1},ownKeys(){traps++;throw 1},getPrototypeOf(){traps++;throw 1}}));true"sv,
            u"eventProbe.initCustomEvent('detail-probe',false,false,Object.defineProperty({},'secret',{get(){traps++;throw 1}}));true"sv,
            u"eventProbe.initCustomEvent('detail-probe',false,false,'x'.repeat(65537));true"sv,
            u"eventProbe.initCustomEvent('detail-probe',false,false,Symbol('private'));true"sv }) {
        TRY(world_check(*source, u"globalThis.traps=0;true"sv));
        TRY(world_check(*source, code));
        TRY(page_check(
            u"(()=>{try{eventProbe.detail;return false}catch(e){return e instanceof TypeError && e.message==='Firefly event value cannot cross worlds' && typeof e.stack==='string' && !e.stack.includes('firefly:isolated-world')}})()"sv));
        TRY(world_check(*target,
            u"(()=>{try{eventProbe.detail;return false}catch(e){return e instanceof TypeError && e.message==='Firefly event value cannot cross worlds' && typeof e.stack==='string' && !e.stack.includes('firefly:isolated-world')}})()"sv));
        TRY(world_check(*source, u"traps===0"sv));
    }
    // Native harness supplies a foreign prototype. Shape realm alone is not
    // provenance: this object still came from the owned writer.
    source->realm()->global_object().define_direct_property(
        "foreignPrototype"_utf16, page.intrinsics().object_prototype(), JS::default_attributes);
    TRY(world_check(*source,
        u"(()=>{const forged=Object.create(foreignPrototype);forged.privateCallback=()=>42;eventProbe.initCustomEvent('detail-probe',false,false,forged);return true})()"sv));
    TRY(page_check(
        u"(()=>{try{eventProbe.detail;return false}catch(e){return e instanceof TypeError}})()"sv));
    TRY(world_check(*source,
        u"globalThis.ownFunction=()=>42;eventProbe.initCustomEvent('detail-probe',false,false,ownFunction);eventProbe.detail===ownFunction"sv));
    outln("PASS CustomEvent same-world identity and bounded frozen cross-world "
          "snapshots/cache/reset without accessor/proxy/function leaks");
    TRY(page_check(
        u"globalThis.pageData={n:8};globalThis.pageEvent=new CustomEvent('page-detail',{detail:pageData});pageEvent.detail===pageData && !Object.isFrozen(pageData)"sv));
    auto parsed = JS::Script::parse(u"pageEvent"_utf16, page);
    if (parsed.is_error())
        return Error::from_string_literal("Cannot parse page event control");
    GC::Root script { parsed.release_value() };
    auto result = vm.run(*script);
    if (result.is_error())
        return Error::from_string_literal("Cannot get page CustomEvent");
    auto page_value = result.release_value();
    auto* page_native = Web::Bindings::impl_from<Web::DOM::CustomEvent>(&page_value.as_object());
    if (!page_native)
        return Error::from_string_literal("Missing page event implementation");
    target->realm()->global_object().define_direct_property("pageEvent"_utf16,
        Web::Bindings::event(*target->realm(), GC::Ref { *page_native }), JS::default_attributes);
    TRY(world_check(*target,
        u"pageEvent.detail.n===8 && Object.isFrozen(pageEvent.detail) && Object.getPrototypeOf(pageEvent.detail)===Object.prototype"sv));
    TRY(page_check(
        u"globalThis.pageTraps=0;pageEvent.initCustomEvent('page-detail',false,false,Object.defineProperty({},'x',{get(){pageTraps++;return 1}}));true"sv));
    TRY(world_check(*target,
        u"(()=>{try{pageEvent.detail;return false}catch(e){return e instanceof TypeError}})()"sv));
    TRY(page_check(u"pageTraps===0 && pageEvent.detail!==null"sv));
    TRY(world_check(
        *source, u"eventProbe.initCustomEvent('detail-probe',false,false,{published:12});true"sv));
    TRY(page_check(u"globalThis.cachedDetail=eventProbe.detail;cachedDetail.published===12"sv));
    TRY(world_check(*target, u"eventProbe.detail.published===12"sv));
    auto revoked_copy = TRY(weak_detail(*target));
    source->detach();
    TRY(page_check(u"eventProbe.detail===cachedDetail && cachedDetail.published===12"sv));
    auto fresh = TRY(Firefly::IsolatedWorld::create(*document.window()));
    TRY(guard.protect(*fresh));
    ScopeGuard detach_fresh([&] { fresh->detach(); });
    fresh->realm()->global_object().define_direct_property("eventProbe"_utf16,
        Web::Bindings::event(*fresh->realm(), GC::Ref { *event }), JS::default_attributes);
    TRY(world_check(*fresh,
        u"(()=>{try{eventProbe.detail;return false}catch(e){return e instanceof TypeError}})()"sv));
    target->detach();
    vm.heap().collect_garbage();
    if (revoked_copy.ptr())
        return Error::from_string_literal("Event cache retained a revoked target-world copy");
    TRY(page_check(u"eventProbe.detail===cachedDetail"sv));
    {
        // An event can predate guard registration. Its primitive value does not
        // root the writer realm, so revocation must survive weak-origin expiry.
        auto early = TRY(Firefly::IsolatedWorld::create(*document.window()));
        auto scalar = TRY(scalar_event(*early));
        TRY(guard.protect(*early));
        early->detach();
        TRY(page_check(u"true"sv));
        vm.heap().collect_garbage();
        if (scalar.writer.ptr())
            return Error::from_string_literal("Primitive event retained its writer realm");
        page.global_object().define_direct_property("scalarEvent"_utf16,
            Web::Bindings::event(page, GC::Ref { *scalar.event }), JS::default_attributes);
        ScopeGuard clear_scalar([&] { MUST(page_check(u"delete globalThis.scalarEvent;true"sv)); });
        TRY(page_check(
            u"(()=>{try{scalarEvent.detail;return false}catch(e){return e instanceof TypeError}})()"sv));
    }

    outln("PASS page values stay unchanged, source reads reject page getters, published copies "
          "survive source revoke and target caches release on detach");
    return {};
}
