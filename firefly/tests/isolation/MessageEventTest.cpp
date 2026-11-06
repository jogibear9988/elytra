#include "MessageEventTest.h"
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

[[gnu::noinline]] static ErrorOr<GC::Weak<JS::Object>> weak_message_data(
    Firefly::IsolatedWorld& owner)
{
    auto result = owner.evaluate(u"messageProbe.data"sv);
    if (result.is_error())
        return Error::from_string_literal("Cannot get message data");
    auto value = result.release_value();
    if (!value.is_object())
        return Error::from_string_literal("Missing message object");
    return GC::Weak { value.as_object() };
}

ErrorOr<void> check_message_event_values(JS::VM& vm, Web::DOM::Document& document)
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
    auto page_eval = [&](Utf16View text) {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), page);
        VERIFY(!parsed.is_error());
        GC::Root script { parsed.release_value() };
        return vm.run(*script);
    };

    auto page_check = [&](Utf16View text) -> ErrorOr<void> {
        auto result = page_eval(text);
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("MessageEvent page assertion failed");
        return {};
    };

    auto world_check = [](Firefly::IsolatedWorld& owner, Utf16View text) -> ErrorOr<void> {
        auto result = owner.evaluate(text);
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("MessageEvent world assertion failed");
        return {};
    };

    auto created = source->evaluate(uR"JS(
        globalThis.messageData={n:42,items:[1,,3]};messageData.self=messageData;messageData.alias=messageData.items;
        globalThis.messageProbe=new MessageEvent('message-test',{data:messageData,origin:'spoofed',lastEventId:'id',source:window});
        messageProbe;
    )JS"sv);
    if (created.is_error())
        return Error::from_string_literal("Cannot create source MessageEvent");
    auto value = created.release_value();
    auto* impl = Web::Bindings::impl_from<Web::DOM::Event>(&value.as_object());
    if (!impl)
        return Error::from_string_literal("Missing native MessageEvent");
    GC::Root event { *impl };
    page.global_object().define_direct_property("messageProbe"_utf16,
        Web::Bindings::event(page, GC::Ref { *event }), JS::default_attributes);
    target->realm()->global_object().define_direct_property("messageProbe"_utf16,
        Web::Bindings::event(*target->realm(), GC::Ref { *event }), JS::default_attributes);
    ScopeGuard clear_page([&] {
        MUST(page_check(
            u"delete globalThis.messageProbe;delete globalThis.cachedMessage;delete globalThis.oldPorts;delete globalThis.pageMessage;delete globalThis.pageData;true"sv));
    });
    TRY(world_check(*source,
        u"messageProbe.data===messageData && !Object.isFrozen(messageData) && messageProbe.source===window"sv));
    TRY(page_check(uR"JS(
        globalThis.cachedMessage=messageProbe.data;globalThis.oldPorts=messageProbe.ports;
        cachedMessage===messageProbe.data && cachedMessage.n===42 && cachedMessage.self===cachedMessage &&
        cachedMessage.alias===cachedMessage.items && !(1 in cachedMessage.items) &&
        Object.isFrozen(cachedMessage) && Object.isFrozen(cachedMessage.items) &&
        Object.getPrototypeOf(cachedMessage)===Object.prototype && Object.getPrototypeOf(cachedMessage.items)===Array.prototype &&
        messageProbe.origin==='spoofed' && messageProbe.lastEventId==='id' && messageProbe.source===window &&
        oldPorts===messageProbe.ports && oldPorts.length===0 && Object.isFrozen(oldPorts) && Object.getPrototypeOf(oldPorts)===Array.prototype;
    )JS"sv));
    TRY(world_check(*target,
        u"(()=>{const value=messageProbe.data;return value===messageProbe.data && value.self===value && Object.isFrozen(value) && Object.getPrototypeOf(value)===Object.prototype && messageProbe.source===window})()"sv));
    auto cached_weak = TRY(weak_message_data(*target));
    vm.heap().collect_garbage();
    if (!cached_weak.ptr())
        return Error::from_string_literal("MessageEvent did not trace its data cache");
    TRY(world_check(*source, u"messageData.n=99;messageProbe.data.n===99"sv));
    TRY(page_check(u"messageProbe.data===cachedMessage && cachedMessage.n===42"sv));
    TRY(world_check(*source, uR"JS(
        document.addEventListener('message-test',event=>{
            event.initMessageEvent('BAD',false,false,{bad:true},'BAD','BAD',null,[]);
            document.documentElement.setAttribute('data-message-dispatch',String(event.type==='message-test' && event.data===messageData));
        });true;
    )JS"sv));
    TRY(page_check(
        u"document.dispatchEvent(messageProbe);messageProbe.type==='message-test' && messageProbe.data===cachedMessage && messageProbe.origin==='spoofed' && document.documentElement.getAttribute('data-message-dispatch')==='true'"sv));
    TRY(world_check(*source,
        u"messageProbe.initMessageEvent('message-test',false,false,{next:7},'new-origin','new-id',null,[]);true"sv));
    TRY(page_check(
        u"messageProbe.data!==cachedMessage && messageProbe.data.next===7 && messageProbe.data===messageProbe.data && messageProbe.origin==='new-origin' && messageProbe.lastEventId==='new-id' && messageProbe.source===null && messageProbe.ports!==oldPorts && messageProbe.ports.length===0"sv));
    for (auto code :
        { u"messageProbe.initMessageEvent('x',false,false,()=>42);true"sv,
            u"messageProbe.initMessageEvent('x',false,false,new Proxy({}, {get(){traps++;throw 1},ownKeys(){traps++;throw 1}}));true"sv,
            u"messageProbe.initMessageEvent('x',false,false,Object.defineProperty({},'secret',{get(){traps++;throw 1}}));true"sv,
            u"messageProbe.initMessageEvent('x',false,false,'x'.repeat(65537));true"sv,
            u"messageProbe.initMessageEvent('x',false,false,Symbol('private'));true"sv }) {
        TRY(world_check(*source, u"globalThis.traps=0;true"sv));
        TRY(world_check(*source, code));
        TRY(page_check(
            u"(()=>{try{messageProbe.data;return false}catch(e){return e instanceof TypeError && !e.stack.includes('firefly:isolated-world')}})()"sv));
        TRY(world_check(*target,
            u"(()=>{try{messageProbe.data;return false}catch(e){return e instanceof TypeError}})()"sv));
        TRY(world_check(*source, u"traps===0"sv));
    }

    TRY(page_check(
        u"globalThis.pageData={n:8};globalThis.pageMessage=new MessageEvent('page-message',{data:pageData});pageMessage.data===pageData && !Object.isFrozen(pageData)"sv));
    auto incoming = page_eval(u"pageMessage"sv);
    if (incoming.is_error())
        return Error::from_string_literal("Cannot get page message");
    auto incoming_value = incoming.release_value();
    auto* page_impl = Web::Bindings::impl_from<Web::DOM::Event>(&incoming_value.as_object());
    if (!page_impl)
        return Error::from_string_literal("Missing page message implementation");
    target->realm()->global_object().define_direct_property("pageMessage"_utf16,
        Web::Bindings::event(*target->realm(), GC::Ref { *page_impl }), JS::default_attributes);
    TRY(world_check(*target,
        u"pageMessage.data.n===8 && Object.isFrozen(pageMessage.data) && Object.getPrototypeOf(pageMessage.data)===Object.prototype"sv));
    TRY(world_check(
        *source, u"messageProbe.initMessageEvent('x',false,false,{published:12});true"sv));
    TRY(page_check(u"globalThis.cachedMessage=messageProbe.data;cachedMessage.published===12"sv));
    TRY(world_check(*target, u"messageProbe.data.published===12"sv));
    auto revoked_copy = TRY(weak_message_data(*target));
    source->detach();
    TRY(page_check(u"messageProbe.data===cachedMessage"sv));
    auto fresh = TRY(Firefly::IsolatedWorld::create(*document.window()));
    TRY(guard.protect(*fresh));
    ScopeGuard detach_fresh([&] { fresh->detach(); });
    fresh->realm()->global_object().define_direct_property("messageProbe"_utf16,
        Web::Bindings::event(*fresh->realm(), GC::Ref { *event }), JS::default_attributes);
    TRY(world_check(*fresh,
        u"(()=>{try{messageProbe.data;return false}catch(e){return e instanceof TypeError}})()"sv));
    target->detach();
    vm.heap().collect_garbage();
    if (revoked_copy.ptr())
        return Error::from_string_literal("MessageEvent retained detached target cache");
    outln("PASS MessageEvent data snapshots/cache/init/revoke/GC, rejected capabilities and "
          "unchanged source/empty ports metadata");
    return {};
}
