#include "IsolatedWorld.h"
#include "BridgeDataTest.h"
#include "FunctionBridgeTest.h"
#include "ContextBridgeTest.h"
#include "PromiseBridgeTest.h"
#include "WorldJobGuardTest.h"
#include "HandlerTest.h"
#include "CustomEventTest.h"
#include "ErrorEventTest.h"
#include "MessageEventTest.h"
#include "PrincipalCallbackTest.h"
#include "NodeFilterTest.h"
#include <LibCore/EventLoop.h>
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <LibMain/Main.h>
#include <LibWeb/Bindings/MainThreadVM.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/LocalTraversableNavigable.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/Platform/FontPlugin.h>

static JS::ThrowCompletionOr<JS::Value> run(JS::VM& vm, JS::Realm& realm, Utf16View source)
{
    auto parsed = JS::Script::parse(Utf16String::from_utf16(source), realm);
    if (parsed.is_error()) return vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    GC::Root script { parsed.release_value() };
    return vm.run(*script);
}

ErrorOr<int> ladybird_main(Main::Arguments)
{
    auto& loop = Core::EventLoop::initialize_for_current_thread();
    (void)loop;
    Web::Platform::FontPlugin font_plugin(false);
    Web::Platform::FontPlugin::install(font_plugin);
    GC::Root initial { Web::Bindings::create_a_principal_javascript_realm() };
    auto& vm = initial->vm();
    auto& page = Web::Bindings::principal_host_defined_page(*initial);
    auto traversable = Web::HTML::LocalTraversableNavigable::create_a_new_top_level_traversable(page, nullptr, {});
    page.set_top_level_traversable(traversable);
    GC::Root document { *traversable->active_document() };
    auto window = document->window();
    auto& principal = window->principal_realm();
    auto baseline_depth = vm.execution_context_stack().size();
    auto unassociated_window = Web::HTML::Window::create();
    if (!Firefly::IsolatedWorld::create(*unassociated_window).is_error())
        return Error::from_string_literal("Window without a document must reject world creation");
    outln("PASS unassociated window rejects world creation");
    auto first = TRY(Firefly::IsolatedWorld::create(*window));
    RefPtr<Firefly::IsolatedWorld> second = TRY(Firefly::IsolatedWorld::create(*window));
    if (first->realm() == second->realm() || first->realm() == &principal)
        return Error::from_string_literal("Worlds must own distinct realms");
    first->realm()->global_object().define_native_function(*first->realm(),"nativeProbe"_utf16,
        +[](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(42); },0,JS::default_attributes);
    auto check = [&](JS::ThrowCompletionOr<JS::Value> result, StringView name) -> bool {
        if (result.is_error()) {
            warnln("FAIL {}: {}", name, result.error_value().to_utf16_string_without_side_effects());
            return false;
        }
        if (!result.value().is_boolean() || !result.value().as_bool()) {
            warnln("FAIL {}: invariant returned false", name);
            return false;
        }
        outln("PASS {}", name);
        return true;
    };
    if (!check(run(vm,principal,uR"JS(
        globalThis.pageSecret = 'page';
        Object.prototype.pagePoison = true;
        Document.prototype.pagePrototypePoison = true;
        document.pageExpando = true;
        document.documentElement.setAttribute('data-owner','page');
        true;
    )JS"sv),"principal setup"sv)) return 1;
    if (!check(first->evaluate(uR"JS(
        globalThis.preloadSecret = 'isolated';
        document.preloadExpando = true;
        document.documentElement.setAttribute('data-owner','isolated');
        typeof pageSecret === 'undefined' && nativeProbe()===42 && !({}).pagePoison &&
        document.pageExpando === undefined && document.pagePrototypePoison === undefined &&
        ['process','require','Buffer'].every(k=>!(k in globalThis));
    )JS"sv),"separate globals/intrinsics/DOM expandos and no Mite globals"sv)) return 1;
    if (!check(run(vm,principal,uR"JS(
        typeof preloadSecret === 'undefined' && typeof nativeProbe==='undefined' && document.preloadExpando === undefined &&
        document.documentElement.getAttribute('data-owner') === 'isolated' &&
        window===self && window===top && document.defaultView===window &&
        typeof window.constructor.constructor('return typeof preloadSecret')() === 'string' &&
        window.constructor.constructor('return typeof preloadSecret')() === 'undefined' &&
        document.constructor.constructor('return typeof nativeProbe')()==='undefined' &&
        document.documentElement.constructor.constructor('return typeof nativeProbe')()==='undefined';
    )JS"sv),"shared DOM without principal wrapper/global escape"sv)) return 1;
    if (!check(second->evaluate(uR"JS(
        typeof pageSecret==='undefined' && typeof preloadSecret==='undefined' && typeof nativeProbe==='undefined' &&
        document.pageExpando===undefined && document.preloadExpando===undefined &&
        document.documentElement.getAttribute('data-owner')==='isolated';
    )JS"sv),"independent worlds on one document"sv)) return 1;
    if (!check(first->evaluate(uR"JS(
        self===window && top===window && parent===window &&
        document.defaultView===window &&
        window.constructor.constructor('return typeof preloadSecret')()==='string' &&
        document.constructor.constructor('return typeof nativeProbe')()==='function' &&
        document.documentElement.constructor.constructor('return typeof nativeProbe')()==='function';
    )JS"sv),"isolated WindowProxy paths and constructor realm"sv)) return 1;
    auto bad = first->evaluate(u"throw Error('isolated failure')"sv);
    auto syntax = first->evaluate(u"const ="sv);
    if (!bad.is_error() || !syntax.is_error() || vm.execution_context_stack().size()!=baseline_depth)
        return Error::from_string_literal("Evaluation errors must restore the caller context");
    outln("PASS thrown/syntax errors restore caller context");
    vm.heap().collect_garbage();
    if (!check(first->evaluate(u"preloadSecret==='isolated' && nativeProbe()===42 && document.documentElement.getAttribute('data-owner')==='isolated'"sv),"rooted world survives GC"sv)) return 1;
    TRY(check_bridge_data(vm, principal, *second->realm()));
    TRY(check_function_bridge(vm, *document));
    TRY(check_context_bridge(vm, *document));
    TRY(check_promise_bridge(vm, *document));
    TRY(check_world_job_guard(vm, *document));
    TRY(check_handlers(vm, *document));
    TRY(check_custom_event_values(vm, *document));
    TRY(check_error_event_values(vm, *document));
    TRY(check_message_event_values(vm, *document));
    TRY(check_principal_callbacks(vm, *document));
    TRY(check_node_filters(vm, *document));
    first->detach();first->detach();
    if (!first->is_detached() || first->realm()!=nullptr || !first->evaluate(u"true"sv).is_error())
        return Error::from_string_literal("Detached world must reject evaluation");
    vm.heap().collect_garbage();
    if (!check(second->evaluate(u"typeof preloadSecret==='undefined' && document.documentElement.getAttribute('data-owner')==='isolated'"sv),"detaching one world preserves the other"sv)) return 1;
    // Create after page prototype pollution as well as before it.
    RefPtr<Firefly::IsolatedWorld> transient = TRY(Firefly::IsolatedWorld::create(*window));
    auto* active_world = transient.ptr();
    GC::Root transient_realm { *transient->realm() };
    transient_realm->global_object().define_native_function(*transient_realm,"requestDetach"_utf16,
        [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            active_world->detach();
            bool revoked = active_world->is_detached() && active_world->realm()==nullptr;
            transient.clear(); // evaluate must retain its owner until its frame exits.
            return JS::Value(revoked);
        },0,JS::default_attributes);
    transient_realm->global_object().define_native_function(*transient_realm,"nestedEvaluation"_utf16,
        [&](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            return JS::Value(active_world->evaluate(u"true"sv).is_error());
        },0,JS::default_attributes);
    if (!check(active_world->evaluate(uR"JS(
        !({}).pagePoison && document.pagePrototypePoison===undefined &&
        nestedEvaluation() && requestDetach() && 1+1===2;
    )JS"sv),"detach during evaluation defers teardown and retains owner"sv)) return 1;
    if (transient || !Web::Bindings::host_defined_wrapper_world(*transient_realm).is_detached() || vm.execution_context_stack().size()!=baseline_depth)
        return Error::from_string_literal("Detach must finish after the evaluation context pops");
    transient_realm->global_object().define_direct_property("requestDetach"_utf16,JS::js_undefined(),JS::default_attributes);
    transient_realm->global_object().define_direct_property("nestedEvaluation"_utf16,JS::js_undefined(),JS::default_attributes);
    transient_realm = nullptr;
    {
        auto externally_detached = TRY(Firefly::IsolatedWorld::create(*window));
        Web::Bindings::host_defined_wrapper_world(*externally_detached->realm()).detach();
        if (!externally_detached->is_detached() || !externally_detached->evaluate(u"true"sv).is_error())
            return Error::from_string_literal("SDK detach must revoke owner evaluation");
        outln("PASS SDK detach revokes owner evaluation");
    }
    second.clear();
    vm.heap().collect_garbage();
    vm.pop_execution_context();
    outln("PASS explicit detach, repeated detach and destructor cleanup");
    return 0;
}
