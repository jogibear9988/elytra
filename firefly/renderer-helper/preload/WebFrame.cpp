/** Use the existing browser zoom handshake so native input, viewport and compositor agree. */
#include "WebFrame.h"
#include "worlds/IsolatedWorld.h"
#include <LibGC/Root.h>
#include <LibGC/Weak.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/Page/Page.h>
#include <LibWebCommon/WebView/ZoomLevel.h>
#include <cmath>

namespace Firefly {
GC::Ref<JS::Object> create_web_frame(IsolatedWorld& world, Web::DOM::Document& document)
{
    auto& realm = *world.realm();
    GC::Root object { JS::Object::create(realm, nullptr) };
    for (auto name : { "setZoomFactor"_utf16, "getZoomFactor"_utf16, "setZoomLevel"_utf16,
             "getZoomLevel"_utf16 }) {
        bool setter = name == "setZoomFactor"_utf16 || name == "setZoomLevel"_utf16;
        bool level = name == "setZoomLevel"_utf16 || name == "getZoomLevel"_utf16;
        GC::Root function { JS::NativeFunction::create(
            realm,
            [weak = world.make_weak_ptr(), target = GC::Weak { document }, setter, level](
                JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto owner = weak.strong_ref();
                GC::Root document { target.ptr() };
                auto valid = [&] {
                    return owner && !owner->is_detached() && owner->is_executing()
                        && owner->realm() == vm.current_realm() && document
                        && document->is_fully_active();
                };

                if (!valid())
                    return vm.throw_completion<JS::TypeError>("webFrame document is revoked"sv);
                if (!setter) {
                    auto factor = document->page().client().zoom_level();
                    return JS::Value(level ? std::log(factor) / std::log(1.2) : factor);
                }
                // No ToNumber: objects/proxies must never execute coercion hooks in native
                // services.
                if (!vm.argument(0).is_number())
                    return vm.throw_completion<JS::TypeError>("Zoom requires a number"sv);
                auto value = vm.argument(0).as_double();
                auto factor = level ? std::pow(1.2, value) : value;
                if (!std::isfinite(value) || !std::isfinite(factor)
                    || factor < WebView::ZOOM_MIN_LEVEL || factor > WebView::ZOOM_MAX_LEVEL)
                    return vm.throw_completion<JS::RangeError>(
                        "Zoom factor must be within 0.3 and 5"sv);
                // This upstream handshake updates the parent view, all hosting pages and the
                // compositor. Retain owner/document and revalidate after its nested event loop.
                document->page().client().page_did_set_browser_zoom(factor);
                if (!valid())
                    return vm.throw_completion<JS::TypeError>(
                        "webFrame document was revoked during zoom"sv);
                return JS::js_undefined();
            },
            setter ? 1 : 0, name, GC::Ptr<JS::Realm> { realm }) };
        world.block_function_export(*function);
        object->define_direct_property(name, function, JS::Attribute::Enumerable);
    }

    world.redact_object_export(*object);
    MUST(object->set_integrity_level(JS::Object::IntegrityLevel::Frozen));
    return *object;
}
}
