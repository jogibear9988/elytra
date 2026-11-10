/** Main and preload zoom share the real upstream view state, never a mirrored JS cache. */
#include "Impl.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibWebCommon/WebView/ZoomLevel.h>
#include <UI/Qt/WebContentView.h>
#include <cmath>

namespace Firefly {
JS::ThrowCompletionOr<double> Bridge::Impl::read_zoom(JS::VM& vm, size_t index, bool optional)
{
    auto value = vm.argument(index);
    if (optional && value.is_undefined())
        return 1.0;
    // Avoid coercion callbacks during native validation and before any allocation.
    if (!value.is_number())
        return vm.throw_completion<JS::TypeError>("Zoom requires a number"_utf16);
    auto factor = value.as_double();
    if (!std::isfinite(factor) || factor < WebView::ZOOM_MIN_LEVEL
        || factor > WebView::ZOOM_MAX_LEVEL)
        return vm.throw_completion<JS::RangeError>("Zoom factor must be within 0.3 and 5"_utf16);
    return factor;
}

void Bridge::Impl::install_zoom(JS::Object& host)
{
    host.define_native_function(realm, "setContentsZoom"_utf16,
        native_binding<&Impl::native_zoom_set_contents_zoom>(), 2, JS::default_attributes);
    host.define_native_function(realm, "contentsZoom"_utf16,
        native_binding<&Impl::native_zoom_contents_zoom>(), 1, JS::default_attributes);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_zoom_set_contents_zoom(JS::VM& vm)
{
    auto* current = TRY(state(vm));
    auto factor = TRY(read_zoom(vm, 1));
    set_content_zoom(*current->view, factor);
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_zoom_contents_zoom(JS::VM& vm)
{
    return JS::Value(content_engine_view(*TRY(state(vm))->view).zoom_level());
}

}
