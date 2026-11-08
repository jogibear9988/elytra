/** Validate coercion-free native services and publication denial without a desktop window. */
#include "WebFrameTest.h"
#include "context/ContextBridge.h"
#include "preload/WebFrame.h"
#include "worlds/WorldJobGuard.h"
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/HTML/Window.h>

ErrorOr<void> check_web_frame(JS::VM& vm, Web::DOM::Document& document)
{
    auto world = TRY(Firefly::IsolatedWorld::create(*document.window()));
    ScopeGuard cleanup([&] { world->detach(); });
    Firefly::WorldJobGuard jobs(vm);
    TRY(jobs.protect(*world));
    auto bridge = TRY(Firefly::ContextBridge::create(*world, document));
    auto& realm = *world->realm();
    realm.global_object().define_direct_property(
        "webFrame"_utf16, Firefly::create_web_frame(*world, document), JS::default_attributes);
    realm.global_object().define_direct_property(
        "contextBridge"_utf16, bridge->exports(), JS::default_attributes);
    auto result = world->evaluate(uR"JS((() => {
        let hooks=0, errors=0;
        const coercion={valueOf(){hooks++;return 1}};
        for(const method of ['setZoomFactor','setZoomLevel']) {
            for(const value of [NaN,Infinity,-Infinity,'1',coercion,undefined]) {
                try {webFrame[method](value)} catch(e) {if(e instanceof TypeError||e instanceof RangeError)errors++}
            }
        }
        let denied=false;
        try {contextBridge.exposeInMainWorld('__zoomRaw',{fn:webFrame.setZoomFactor})}catch(e){denied=true}
        return hooks===0 && errors===12 && denied && Object.isFrozen(webFrame)
            && webFrame.getZoomFactor()>0 && Number.isFinite(webFrame.getZoomLevel());
    })())JS"sv);
    if (result.is_error() || !result.value().is_boolean() || !result.value().as_bool())
        return Error::from_string_literal("Native webFrame validation/publication gate failed");
    outln("PASS webFrame numeric validation/coercion refusal/freeze/native export guard");
    return {};
}
