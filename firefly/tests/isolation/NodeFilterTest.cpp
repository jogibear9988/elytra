#include "NodeFilterTest.h"
#include "CaptureSlot.h"
#include "callbacks/CallbackGate.h"
#include "worlds/IsolatedWorld.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Script.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/NodeIterator.h>
#include <LibWeb/Bindings/TreeWalker.h>
#include <LibWeb/Bindings/WrapperWorld.h>
#include <LibWeb/DOM/BindingsGlue.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/NodeIterator.h>
#include <LibWeb/DOM/TreeWalker.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/Window.h>

struct ForeignFilterProbe {
    GC::Root<Web::DOM::TreeWalker> iterator;
    GC::Weak<JS::Realm> writer;
    GC::Weak<Web::DOM::NodeFilter> filter;
};

// Do not leave stale Source wrapper pointers on the conservatively scanned
// caller stack when testing whether weak provenance allows realm collection.
[[gnu::noinline]] static ErrorOr<ForeignFilterProbe> foreign_filter_probe(
    Firefly::WorldJobGuard& guard, Web::DOM::Document& document, bool unknown_origin)
{
    auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
    TRY(guard.protect(*source));
    auto& page = document.window()->principal_realm();
    auto parsed = JS::Script::parse(u"({acceptNode:null})"_utf16, page);
    if (parsed.is_error())
        return Error::from_string_literal("Cannot parse foreign filter object");
    GC::Root script { parsed.release_value() };
    auto object = page.vm().run(*script);
    if (object.is_error())
        return Error::from_string_literal("Cannot create foreign filter object");
    source->realm()->global_object().define_direct_property(
        "pageFilterObject"_utf16, object.release_value(), JS::default_attributes);
    auto created = [&] {
        // Model a native filter created before any registered adapter observed
        // its provenance; do not guess that a principal callback implies origin.
        auto previous = unknown_origin ? Firefly::Callbacks::exchange_gate(page.vm(), {})
                                       : Firefly::Callbacks::Gate {};
        ScopeGuard restore([&] {
            if (unknown_origin)
                Firefly::Callbacks::exchange_gate(page.vm(), previous);
        });
        return source->evaluate(
            u"document.createTreeWalker(document.createElement('div'),NodeFilter.SHOW_ELEMENT,pageFilterObject)"sv);
    }();
    if (created.is_error())
        return Error::from_string_literal("Cannot create foreign-callback filter");
    auto value = created.release_value();
    auto* iterator = Web::Bindings::impl_from<Web::DOM::TreeWalker>(&value.as_object());
    ForeignFilterProbe result { GC::Root { *iterator }, GC::Weak { *source->realm() },
        GC::Weak { *Firefly::Callbacks::probe_filter(*iterator) } };
    source->detach();
    return result;
}

ErrorOr<void> check_node_filters(JS::VM& vm, Web::DOM::Document& document)
{
    Firefly::WorldJobGuard guard(vm);
    auto& page = document.window()->principal_realm();
    auto eval = [&](Utf16View text) {
        auto parsed = JS::Script::parse(Utf16String::from_utf16(text), page);
        VERIFY(!parsed.is_error());
        GC::Root script { parsed.release_value() };
        return vm.run(*script);
    };

    auto check = [&](Utf16View text) -> ErrorOr<void> {
        auto result = eval(text);
        if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
            return Error::from_string_literal("NodeFilter page assertion failed");
        return {};
    };

    ScopeGuard cleanup([&] {
        MUST(check(
            u"delete globalThis.filterIterator;delete globalThis.filterWalker;delete globalThis.filterPageJobs;delete globalThis.filterPageLeaks;true"sv));
    });
    auto depth = vm.execution_context_stack().size();
    {
        auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*source));
        ScopeGuard detach([&] { source->detach(); });
        auto created = source->evaluate(uR"JS(
            globalThis.filterCalls=0;
            globalThis.filterObject={get acceptNode(){document.documentElement.setAttribute('data-filter-lookup','yes');return function(node){
                if(this!==filterObject || node.constructor.constructor('return globalThis')()!==globalThis)throw Error('Wrong filter world');
                filterCalls++;document.documentElement.setAttribute('data-filter-calls',String(filterCalls));
                if(document.documentElement.getAttribute('data-filter-throw')==='yes')throw {secret:'never-page'};
                return {valueOf(){document.documentElement.setAttribute('data-filter-conversion','yes');if(document.documentElement.getAttribute('data-filter-throw')==='convert')throw {conversionSecret:true};return NodeFilter.FILTER_ACCEPT}};
            }}};
            const root=document.createElement('div');root.innerHTML='<span></span><b></b>';
            globalThis.filterIterator=document.createNodeIterator(root,NodeFilter.SHOW_ELEMENT,filterObject);
            if(filterIterator.filter!==filterObject)throw Error('Own filter identity changed');
            filterIterator;
        )JS"sv);
        if (created.is_error())
            return Error::from_string_literal("Cannot create source NodeIterator");
        auto value
            = GC::Root<JS::Value>::create(created.release_value(), SourceLocation::current());
        auto* iterator
            = Web::Bindings::impl_from<Web::DOM::NodeIterator>(&value.value().as_object());
        if (!iterator)
            return Error::from_string_literal("Missing native NodeIterator");
        page.global_object().define_direct_property("filterIterator"_utf16,
            Web::Bindings::wrap(
                Web::Bindings::host_defined_wrapper_world(page), page, GC::Ref { *iterator }),
            JS::default_attributes);
        auto revoked = source->evaluate(uR"JS((()=>{
            const revoked=Proxy.revocable(()=>1,{});
            const walker=document.createTreeWalker(document.createElement('div'),NodeFilter.SHOW_ELEMENT,revoked.proxy);revoked.revoke();
            try{walker.filter;return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter cannot cross worlds'}
        })())JS"sv);
        if (revoked.is_error() || !revoked.value().as_bool())
            return Error::from_string_literal("Unresolvable callable realm did not reject safely");
        TRY(check(
            uR"JS((()=>{try{filterIterator.filter;return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter cannot cross worlds' && !error.stack.includes('firefly:isolated-world') && document.documentElement.getAttribute('data-filter-lookup')===null}})())JS"sv));
        {
            auto other = TRY(Firefly::IsolatedWorld::create(*document.window()));
            TRY(guard.protect(*other));
            ScopeGuard detach_other([&] { other->detach(); });
            other->realm()->global_object().define_direct_property("foreignIterator"_utf16,
                Web::Bindings::wrap(Web::Bindings::host_defined_wrapper_world(*other->realm()),
                    *other->realm(), GC::Ref { *iterator }),
                JS::default_attributes);
            auto denied = other->evaluate(
                u"(()=>{try{foreignIterator.filter;return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter cannot cross worlds'}})()"sv);
            if (denied.is_error() || !denied.value().as_bool())
                return Error::from_string_literal(
                    "Another Source world received raw filter callback");
        }

        TRY(check(
            u"filterIterator.nextNode().localName==='div' && document.documentElement.getAttribute('data-filter-calls')==='1' && document.documentElement.getAttribute('data-filter-conversion')==='yes'"sv));
        TRY(check(
            uR"JS((()=>{document.documentElement.setAttribute('data-filter-throw','yes');try{filterIterator.nextNode();return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter is unavailable' && !error.stack.includes('firefly:isolated-world') && error.secret===undefined}})())JS"sv));
        TRY(check(
            uR"JS((()=>{document.documentElement.setAttribute('data-filter-throw','convert');try{filterIterator.nextNode();return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter is unavailable' && !error.stack.includes('firefly:isolated-world') && error.conversionSecret===undefined}})())JS"sv));
        TRY(check(
            u"document.documentElement.removeAttribute('data-filter-throw');document.documentElement.removeAttribute('data-filter-lookup');true"sv));
        source->detach();
        vm.heap().collect_garbage();
        TRY(check(
            uR"JS((()=>{try{filterIterator.filter;return false}catch(error){return error instanceof TypeError && !error.stack.includes('firefly:isolated-world')}})())JS"sv));
        TRY(check(
            uR"JS((()=>{try{filterIterator.nextNode();return false}catch(error){return error instanceof TypeError && !error.stack.includes('firefly:isolated-world') && document.documentElement.getAttribute('data-filter-lookup')===null && document.documentElement.getAttribute('data-filter-calls')==='3'}})())JS"sv));
    }

    {
        RefPtr<Firefly::IsolatedWorld> owner
            = TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*owner));
        GC::Root realm { *owner->realm() };
        auto weak = owner->make_weak_ptr();
        Firefly::Tests::CaptureSlot owner_slot { owner };
        auto drop = JS::NativeFunction::create(*realm,
            GC::create_function(vm.heap(), GC::captures(Mite::native_reference(owner_slot), realm),
                [](auto& slot, auto& realm, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto& owner = slot.get().value;
                    auto* active = owner.ptr();
                    bool executing = active->is_executing();
                    active->detach();
                    owner.clear();
                    vm.heap().collect_garbage();
                    return JS::Value(executing
                        && !Web::Bindings::host_defined_wrapper_world(*realm).is_detached());
                }),
            0, "dropFilterOwner"_utf16, GC::Ptr<JS::Realm> { *realm });
        realm->global_object().define_direct_property(
            "dropFilterOwner"_utf16, drop, JS::default_attributes);
        ScopeGuard clear_drop([&] {
            realm->global_object().define_direct_property(
                "dropFilterOwner"_utf16, JS::js_undefined(), JS::default_attributes);
        });
        auto created = owner->evaluate(uR"JS(
            const root=document.createElement('div');root.innerHTML='<i></i>';
            document.createTreeWalker(root,NodeFilter.SHOW_ELEMENT,{get acceptNode(){
                if(!dropFilterOwner())throw Error('Owner not retained');
                return function(node){node.textContent='after-drop';return{valueOf(){document.documentElement.setAttribute('data-filter-drop',node.textContent);return 1}}};
            }});
        )JS"sv);
        if (created.is_error())
            return Error::from_string_literal("Cannot create last-owner TreeWalker");
        auto value
            = GC::Root<JS::Value>::create(created.release_value(), SourceLocation::current());
        auto* walker = Web::Bindings::impl_from<Web::DOM::TreeWalker>(&value.value().as_object());
        if (!walker)
            return Error::from_string_literal("Missing native TreeWalker");
        page.global_object().define_direct_property("filterWalker"_utf16,
            Web::Bindings::wrap(
                Web::Bindings::host_defined_wrapper_world(page), page, GC::Ref { *walker }),
            JS::default_attributes);
        TRY(check(
            u"filterWalker.nextNode().localName==='i' && document.documentElement.getAttribute('data-filter-drop')==='after-drop'"sv));
        if (owner || weak.strong_ref()
            || !Web::Bindings::host_defined_wrapper_world(*realm).is_detached())
            return Error::from_string_literal(
                "NodeFilter did not defer last-owner cleanup through conversion");
    }

    {
        auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*source));
        ScopeGuard detach([&] { source->detach(); });
        auto created = eval(uR"JS(
            globalThis.filterPageLeaks=0;globalThis.filterPageJobs=0;
            const root=document.createElement('div');root.innerHTML='<em></em>';
            globalThis.filterWalker=document.createTreeWalker(root,NodeFilter.SHOW_ELEMENT,{get acceptNode(){
                if(new Error().stack.includes('firefly:isolated-world'))filterPageLeaks++;Promise.resolve().then(()=>filterPageJobs++);
                return function(node){if(new Error().stack.includes('firefly:isolated-world'))filterPageLeaks++;Promise.resolve().then(()=>filterPageJobs++);
                    return {valueOf(){if(new Error().stack.includes('firefly:isolated-world'))filterPageLeaks++;Promise.resolve().then(()=>filterPageJobs++);return 1}}};
            }});filterWalker;
        )JS"sv);
        if (created.is_error())
            return Error::from_string_literal("Cannot create principal TreeWalker");
        auto value
            = GC::Root<JS::Value>::create(created.release_value(), SourceLocation::current());
        auto* walker = Web::Bindings::impl_from<Web::DOM::TreeWalker>(&value.value().as_object());
        source->realm()->global_object().define_direct_property("pageWalker"_utf16,
            Web::Bindings::wrap(Web::Bindings::host_defined_wrapper_world(*source->realm()),
                *source->realm(), GC::Ref { *walker }),
            JS::default_attributes);
        auto denied = source->evaluate(
            u"(()=>{try{pageWalker.filter;return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter cannot cross worlds'}})()"sv);
        if (denied.is_error() || !denied.value().as_bool())
            return Error::from_string_literal(
                "Source world received principal raw filter callback");
        auto result = source->evaluate(u"pageWalker.nextNode().localName==='em'"sv);
        if (result.is_error() || !result.value().as_bool())
            return Error::from_string_literal("Source traversal of principal filter failed");
        source->detach();
        {
            vm.save_execution_context_stack();
            ScopeGuard restore([&] { vm.restore_execution_context_stack(); });
            Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();
        }

        TRY(check(u"filterPageLeaks===0 && filterPageJobs===3"sv));
    }

    {
        auto source = TRY(Firefly::IsolatedWorld::create(*document.window()));
        TRY(guard.protect(*source));
        ScopeGuard detach([&] { source->detach(); });
        auto prototype = eval(u"Object.prototype"sv);
        if (prototype.is_error())
            return Error::from_string_literal("Cannot obtain foreign filter prototype");
        source->realm()->global_object().define_direct_property(
            "foreignPrototype"_utf16, prototype.release_value(), JS::default_attributes);
        auto created = source->evaluate(uR"JS(
            const callback=Object.create(foreignPrototype);
            Object.defineProperty(callback,'acceptNode',{get(){document.documentElement.setAttribute('data-filter-forged-trap','BAD');return ()=>1}});
            document.createTreeWalker(document.createElement('div'),NodeFilter.SHOW_ELEMENT,callback);
        )JS"sv);
        if (created.is_error())
            return Error::from_string_literal("Cannot create foreign-prototype filter");
        auto value
            = GC::Root<JS::Value>::create(created.release_value(), SourceLocation::current());
        auto* walker = Web::Bindings::impl_from<Web::DOM::TreeWalker>(&value.value().as_object());
        page.global_object().define_direct_property("filterWalker"_utf16,
            Web::Bindings::wrap(
                Web::Bindings::host_defined_wrapper_world(page), page, GC::Ref { *walker }),
            JS::default_attributes);
        TRY(check(
            uR"JS((()=>{try{filterWalker.filter;return false}catch(error){return error instanceof TypeError && document.documentElement.getAttribute('data-filter-forged-trap')===null}})())JS"sv));
    }

    for (bool unknown_origin : { false, true }) {
        auto probe = TRY(foreign_filter_probe(guard, document, unknown_origin));
        // Observe roots at a separate native boundary before collection. This
        // also drains any earlier incremental sweep; dead producer temporaries
        // must not masquerade as ownership in the collector's register snapshot.
        // A real strong metadata edge fails this check as well as the weak-GC
        // assertions below. The predicate retains only the indirect weak handle.
        auto paths = vm.heap().find_retaining_paths(
            [&](GC::Cell& cell) {
                return static_cast<void*>(&cell) == static_cast<void*>(probe.writer.ptr().ptr());
            },
            1);
        if (!paths.is_empty())
            return Error::from_string_literal("Foreign filter writer still has a retaining path");
        page.global_object().define_direct_property("filterIterator"_utf16,
            Web::Bindings::wrap(
                Web::Bindings::host_defined_wrapper_world(page), page, GC::Ref { *probe.iterator }),
            JS::default_attributes);
        vm.heap().collect_garbage();
        if (probe.writer.ptr())
            return Error::from_string_literal("Weak filter provenance retained a Source realm");
        TRY(check(
            uR"JS((()=>{try{filterIterator.filter;return false}catch(error){return error instanceof TypeError}})())JS"sv));
        TRY(check(u"delete globalThis.filterIterator;true"sv));
        probe.iterator = {};
        vm.heap().collect_garbage();
        if (probe.filter.ptr())
            return Error::from_string_literal("Filter metadata retained a dead native filter");
    }

    if (vm.execution_context_stack().size() != depth)
        return Error::from_string_literal("NodeFilter did not restore caller context");
    outln("PASS NodeFilter world/receiver/return conversion, redacted errors, revoked lookup, "
          "last-owner GC and principal jobs/frames");
    outln("PASS filter getter same-world identity, cross-world denial, foreign-callback/unknown "
          "provenance, revoked proxies, writer GC and weak native metadata");
    return {};
}
