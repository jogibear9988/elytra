#include "NodeFilterTest.h"
#include "IsolatedWorld.h"
#include "WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Heap.h>
#include <LibJS/Script.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibWeb/Bindings/NodeIterator.h>
#include <LibWeb/Bindings/TreeWalker.h>
#include <LibWeb/Bindings/HostDefined.h>
#include <LibWeb/Bindings/WrapperWorld.h>
#include <LibWeb/DOM/BindingsGlue.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/NodeIterator.h>
#include <LibWeb/DOM/TreeWalker.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>

ErrorOr<void> check_node_filters(JS::VM& vm, Web::DOM::Document& document)
{
    Firefly::WorldJobGuard guard(vm);
    auto& page=document.window()->principal_realm();
    auto eval=[&](Utf16View text){auto parsed=JS::Script::parse(Utf16String::from_utf16(text),page);VERIFY(!parsed.is_error());GC::Root script{parsed.release_value()};return vm.run(*script);};
    auto check=[&](Utf16View text)->ErrorOr<void>{auto result=eval(text);if(result.is_error() || !result.value().is_boolean() || !result.value().as_bool())return Error::from_string_literal("NodeFilter page assertion failed");return {};};
    ScopeGuard cleanup([&]{MUST(check(u"delete globalThis.filterIterator;delete globalThis.filterWalker;delete globalThis.filterPageJobs;delete globalThis.filterPageLeaks;true"sv));});
    auto depth=vm.execution_context_stack().size();
    {
        auto source=TRY(Firefly::IsolatedWorld::create(*document.window()));TRY(guard.protect(*source));
        ScopeGuard detach([&]{source->detach();});
        auto created=source->evaluate(uR"JS(
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
        if(created.is_error())return Error::from_string_literal("Cannot create source NodeIterator");
        auto value=GC::Root<JS::Value>::create(created.release_value(),SourceLocation::current());
        auto* iterator=Web::Bindings::impl_from<Web::DOM::NodeIterator>(&value.value().as_object());
        if(!iterator)return Error::from_string_literal("Missing native NodeIterator");
        page.global_object().define_direct_property("filterIterator"_utf16,Web::Bindings::wrap(Web::Bindings::host_defined_wrapper_world(page),page,GC::Ref{*iterator}),JS::default_attributes);
        TRY(check(u"filterIterator.nextNode().localName==='div' && document.documentElement.getAttribute('data-filter-calls')==='1' && document.documentElement.getAttribute('data-filter-conversion')==='yes'"sv));
        TRY(check(uR"JS((()=>{document.documentElement.setAttribute('data-filter-throw','yes');try{filterIterator.nextNode();return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter is unavailable' && !error.stack.includes('firefly:isolated-world') && error.secret===undefined}})())JS"sv));
        TRY(check(uR"JS((()=>{document.documentElement.setAttribute('data-filter-throw','convert');try{filterIterator.nextNode();return false}catch(error){return error instanceof TypeError && error.message==='Firefly node filter is unavailable' && !error.stack.includes('firefly:isolated-world') && error.conversionSecret===undefined}})())JS"sv));
        TRY(check(u"document.documentElement.removeAttribute('data-filter-throw');document.documentElement.removeAttribute('data-filter-lookup');true"sv));
        source->detach();vm.heap().collect_garbage();
        TRY(check(uR"JS((()=>{try{filterIterator.nextNode();return false}catch(error){return error instanceof TypeError && !error.stack.includes('firefly:isolated-world') && document.documentElement.getAttribute('data-filter-lookup')===null && document.documentElement.getAttribute('data-filter-calls')==='3'}})())JS"sv));
    }
    {
        RefPtr<Firefly::IsolatedWorld> owner=TRY(Firefly::IsolatedWorld::create(*document.window()));TRY(guard.protect(*owner));
        GC::Root realm {*owner->realm()};auto weak=owner->make_weak_ptr();auto* active=owner.ptr();
        auto drop=JS::NativeFunction::create(*realm,[&](JS::VM&)->JS::ThrowCompletionOr<JS::Value>{
            bool executing=active->is_executing();active->detach();owner.clear();vm.heap().collect_garbage();
            return JS::Value(executing && !Web::Bindings::host_defined_wrapper_world(*realm).is_detached());
        },0,"dropFilterOwner"_utf16,GC::Ptr<JS::Realm>{*realm});
        realm->global_object().define_direct_property("dropFilterOwner"_utf16,drop,JS::default_attributes);
        ScopeGuard clear_drop([&]{realm->global_object().define_direct_property("dropFilterOwner"_utf16,JS::js_undefined(),JS::default_attributes);});
        auto created=owner->evaluate(uR"JS(
            const root=document.createElement('div');root.innerHTML='<i></i>';
            document.createTreeWalker(root,NodeFilter.SHOW_ELEMENT,{get acceptNode(){
                if(!dropFilterOwner())throw Error('Owner not retained');
                return function(node){node.textContent='after-drop';return{valueOf(){document.documentElement.setAttribute('data-filter-drop',node.textContent);return 1}}};
            }});
        )JS"sv);
        if(created.is_error())return Error::from_string_literal("Cannot create last-owner TreeWalker");
        auto value=GC::Root<JS::Value>::create(created.release_value(),SourceLocation::current());auto* walker=Web::Bindings::impl_from<Web::DOM::TreeWalker>(&value.value().as_object());
        if(!walker)return Error::from_string_literal("Missing native TreeWalker");
        page.global_object().define_direct_property("filterWalker"_utf16,Web::Bindings::wrap(Web::Bindings::host_defined_wrapper_world(page),page,GC::Ref{*walker}),JS::default_attributes);
        TRY(check(u"filterWalker.nextNode().localName==='i' && document.documentElement.getAttribute('data-filter-drop')==='after-drop'"sv));
        if(owner || weak.strong_ref() || !Web::Bindings::host_defined_wrapper_world(*realm).is_detached())return Error::from_string_literal("NodeFilter did not defer last-owner cleanup through conversion");
    }
    {
        auto source=TRY(Firefly::IsolatedWorld::create(*document.window()));TRY(guard.protect(*source));ScopeGuard detach([&]{source->detach();});
        auto created=eval(uR"JS(
            globalThis.filterPageLeaks=0;globalThis.filterPageJobs=0;
            const root=document.createElement('div');root.innerHTML='<em></em>';
            globalThis.filterWalker=document.createTreeWalker(root,NodeFilter.SHOW_ELEMENT,{get acceptNode(){
                if(new Error().stack.includes('firefly:isolated-world'))filterPageLeaks++;Promise.resolve().then(()=>filterPageJobs++);
                return function(node){if(new Error().stack.includes('firefly:isolated-world'))filterPageLeaks++;Promise.resolve().then(()=>filterPageJobs++);
                    return {valueOf(){if(new Error().stack.includes('firefly:isolated-world'))filterPageLeaks++;Promise.resolve().then(()=>filterPageJobs++);return 1}}};
            }});filterWalker;
        )JS"sv);
        if(created.is_error())return Error::from_string_literal("Cannot create principal TreeWalker");
        auto value=GC::Root<JS::Value>::create(created.release_value(),SourceLocation::current());auto* walker=Web::Bindings::impl_from<Web::DOM::TreeWalker>(&value.value().as_object());
        source->realm()->global_object().define_direct_property("pageWalker"_utf16,Web::Bindings::wrap(Web::Bindings::host_defined_wrapper_world(*source->realm()),*source->realm(),GC::Ref{*walker}),JS::default_attributes);
        auto result=source->evaluate(u"pageWalker.nextNode().localName==='em'"sv);
        if(result.is_error() || !result.value().as_bool())return Error::from_string_literal("Source traversal of principal filter failed");
        source->detach();
        {vm.save_execution_context_stack();ScopeGuard restore([&]{vm.restore_execution_context_stack();});Web::HTML::main_thread_event_loop().perform_a_microtask_checkpoint();}
        TRY(check(u"filterPageLeaks===0 && filterPageJobs===3"sv));
    }
    if(vm.execution_context_stack().size()!=depth)return Error::from_string_literal("NodeFilter did not restore caller context");
    outln("PASS NodeFilter world/receiver/return conversion, redacted errors, revoked lookup, last-owner GC and principal jobs/frames");
    return {};
}
