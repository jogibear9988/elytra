/** Trusted main-process canvas color policy, applied to real view pixels and renderer defaults. */
#include "Impl.h"
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentPage.h>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
void Bridge::Impl::broadcast_background()
{
    if (stopping)
        return;
    std::unordered_set<WebView::WebContentClient*> clients;
    for (auto const& [id, window] : windows) {
        (void)id;
        if (window.closing || !window.native_view() || !window.native_view()->has_traversable())
            continue;
        window.native_view()->traversable().for_each_hosting_page([&](auto& page) {
            if (page.is_open() && clients.insert(&page.client()).second)
                configure_preloads(page);
        });
    }
}

void Bridge::Impl::install_background(JS::Object& host)
{
    host.define_native_function(realm, "setContentsBackground"_utf16,
        native_binding<&Impl::native_background_set_contents_background>(), 2,
        JS::default_attributes);
    host.define_native_function(realm, "contentsBackground"_utf16,
        native_binding<&Impl::native_background_contents_background>(), 1, JS::default_attributes);
    host.define_native_function(realm, "lookupColor"_utf16,
        native_binding<&Impl::native_background_lookup_color>(), 1, JS::default_attributes);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_background_set_contents_background(JS::VM& vm)
{
    auto* current = TRY(state(vm));
    auto value = vm.argument(1);
    if (!value.is_int32() || value.as_i32() < 0 || value.as_i32() > 0xffffff)
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid opaque background color"sv);
    if (current->background_generation == 2147483647)
        return preload_error(
            vm, "ERR_FIREFLY_BACKGROUND_LIMIT", "Background generation exhausted"sv);
    auto color = static_cast<u32>(value.as_i32());
    set_content_background(*current->view, color);
    current->background_rgb = color;
    ++current->background_generation;
    broadcast_background();
    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_background_contents_background(JS::VM& vm)
{
    return JS::Value(TRY(state(vm))->background_rgb);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_background_lookup_color(JS::VM& vm)
{
    if (!vm.argument(0).is_string())
        return preload_error(vm, "ERR_INVALID_ARG_TYPE", "Color name must be a string"sv);
    auto name = vm.argument(0).as_string().utf16_string_view();
    if (name.is_empty() || name.length_in_code_units() > 64)
        return JS::js_null();
    for (size_t i = 0; i < name.length_in_code_units(); ++i)
        if (name.code_unit_at(i) < 'a' || name.code_unit_at(i) > 'z')
            return JS::js_null();
    auto color = Gfx::Color::from_named_css_color_string(name);
    if (!color.has_value() || color->alpha() != 255)
        return JS::js_null();
    return JS::Value(static_cast<u32>(color->red()) << 16 | static_cast<u32>(color->green()) << 8
        | color->blue());
}

}
