/** Publish authentic window geometry as renderer-owned data on the admitted document. */
#include "Overlay.h"
#include <AK/HashMap.h>
#include <AK/NeverDestroyed.h>
#include <LibGC/Root.h>
#include <LibGC/Weak.h>
#include <LibGfx/Rect.h>
#include <LibJS/Runtime/HostObject.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibWeb/Bindings/Wrappable.h>
#include <LibWeb/Bindings/WrapperWorld.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Event.h>
#include <LibWeb/DOM/EventTarget.h>
#include <LibWeb/Geometry/DOMRect.h>
#include <LibWeb/HTML/EventLoop/EventLoop.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Navigator.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Scripting/TemporaryExecutionContext.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <LibWeb/WebIDL/CallbackType.h>
#include <cmath>

namespace Firefly::TitleBarOverlay {
namespace {
    struct Publication {
        GC::Weak<Web::DOM::Document> document;
        GC::Weak<Web::DOM::EventTarget> target;
    };

    NeverDestroyed<HashMap<String, Gfx::IntRect>> regions;
    NeverDestroyed<HashMap<String, Publication>> publications;

    template <typename T> GC::Ref<JS::HostObject> wrap(JS::Realm& realm, GC::Ref<T> implementation)
    {
        return Web::Bindings::wrap(
            Web::Bindings::host_defined_wrapper_world(realm), realm, implementation);
    }

    Optional<Gfx::IntRect> geometry(Web::DOM::Document const& document)
    {
        auto navigation = document.navigable();
        if (!navigation || navigation->has_been_destroyed()
            || !navigation->is_top_level_traversable()
            || navigation->active_document().ptr() != &document)
            return {};
        return regions->get(Web::WebDriver::BiDi::navigable_id(*navigation));
    }

    Array<double, 4> css_lengths(Web::DOM::Document const& document, Gfx::IntRect rect)
    {
        auto navigation = document.navigable();
        auto scale = navigation ? navigation->page().client().zoom_level() : 1.0;
        if (!std::isfinite(scale) || scale <= 0)
            scale = 1;
        return { rect.x() / scale, rect.y() / scale, rect.width() / scale, rect.height() / scale };
    }

    GC::Ref<Web::Geometry::DOMRect> css_rect(Web::DOM::Document const& document, Gfx::IntRect rect)
    {
        auto lengths = css_lengths(document, rect);
        return Web::Geometry::DOMRect::create(lengths[0], lengths[1], lengths[2], lengths[3]);
    }

    GC::Ptr<Web::DOM::EventTarget> target_for(Web::DOM::Document const& document)
    {
        auto navigation = document.navigable();
        if (!navigation)
            return {};
        auto it = publications->find(Web::WebDriver::BiDi::navigable_id(*navigation));
        if (it == publications->end() || it->value.document.ptr().ptr() != &document)
            return {};
        return it->value.target.ptr();
    }

    void notify(Web::DOM::Document& document, Gfx::IntRect rect)
    {
        auto window = document.window();
        auto target = target_for(document);
        if (!window || !target)
            return;
        Web::HTML::queue_global_task(Web::HTML::Task::Source::DOMManipulation,
            Web::HTML::relevant_global_object(*window),
            GC::create_function<void()>(document.relevant_settings_object().realm().heap(),
                GC::captures(GC::Ref { document }, GC::Ref { *target }, rect.x(), rect.width(),
                    rect.height()),
                [](auto& document, auto& target, int x, int width, int height) {
                    Gfx::IntRect rect(x, 0, width, height);
                    if (!document->is_fully_active())
                        return;
                    auto window = document->window();
                    if (!window)
                        return;
                    Web::HTML::TemporaryExecutionContext context(
                        document->relevant_settings_object());
                    auto& realm = document->relevant_settings_object().realm();
                    GC::Root event { Web::DOM::Event::create(
                        Web::HTML::relevant_global_object(*window), "geometrychange"_utf16) };
                    GC::Root wrapper { wrap(realm, GC::Ref { *event }) };
                    wrapper->define_direct_property(
                        "visible"_utf16, JS::Value(!rect.is_empty()), 0);
                    wrapper->define_direct_property(
                        "titlebarAreaRect"_utf16, wrap(realm, css_rect(*document, rect)), 0);
                    event->set_is_trusted(true);
                    target->dispatch_event(*event);
                }));
    }
}

Optional<Array<double, 4>> css_geometry(Web::DOM::Document const& document)
{
    if (auto window = document.window(); !window || !window->is_secure_context())
        return {};
    if (auto rect = geometry(document); rect.has_value())
        return css_lengths(document, *rect);
    return {};
}

void install(Web::DOM::Document& document)
{
    auto window = document.window();
    if (!window || !window->is_secure_context() || !geometry(document).has_value()
        || target_for(document))
        return;

    Web::HTML::TemporaryExecutionContext context(document.relevant_settings_object());
    auto& realm = document.relevant_settings_object().realm();
    auto overlay = Web::DOM::EventTarget::create();
    GC::Root wrapper { wrap(realm, overlay) };
    auto& heap = realm.heap();
    wrapper->define_native_accessor(realm, "visible"_utf16,
        GC::create_function(heap, GC::captures(GC::Ref { document }),
            [](auto& document, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                return JS::Value(!geometry(*document).value_or({}).is_empty());
            }),
        nullptr, JS::Attribute::Enumerable);
    wrapper->define_direct_property("getTitlebarAreaRect"_utf16,
        JS::NativeFunction::create(realm,
            GC::create_function(heap, GC::captures(GC::Ref { document }),
                [](auto& document, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    return wrap(
                        *vm.current_realm(), css_rect(*document, geometry(*document).value_or({})));
                }),
            0, "getTitlebarAreaRect"_utf16, realm),
        JS::default_attributes);
    wrapper->define_native_accessor(realm, "ongeometrychange"_utf16,
        GC::create_function(heap, GC::captures(overlay),
            [](auto& target, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                auto* callback = target->event_handler_attribute("geometrychange"_utf16);
                return callback ? JS::Value(callback->callback.ptr()) : JS::js_null();
            }),
        GC::create_function(heap, GC::captures(overlay),
            [](auto& target, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto handler = vm.argument(0);
                GC::Ptr<Web::WebIDL::CallbackType> callback;
                if (handler.is_function())
                    callback = vm.heap().allocate<Web::WebIDL::CallbackType>(
                        handler.as_object(), *vm.current_realm());
                target->set_event_handler_attribute("geometrychange"_utf16, callback);
                return JS::js_undefined();
            }),
        JS::Attribute::Enumerable);
    auto handle = Web::WebDriver::BiDi::navigable_id(*document.navigable());
    publications->set(handle, Publication { document, *overlay });
    wrap(realm, window->navigator())
        ->define_direct_property("windowControlsOverlay"_utf16, JS::Value(wrapper.ptr()), 0);
}

ErrorOr<void> extract(Web::WebDriver::BiDiSessionState& state)
{
    if (state.preload_scripts.size() > 784)
        return Error::from_string_literal("Native title bar overlay configuration budget exceeded");
    HashMap<String, Gfx::IntRect> next;
    for (auto const& record : state.preload_scripts) {
        if (record.id != "firefly.title-bar-overlay.v1"sv)
            continue;
        if (!record.function_declaration.is_empty() || !record.contexts.has_value()
            || record.contexts->size() != 1 || record.sandbox.has_value()
            || !record.user_contexts.is_empty() || !record.arguments.is_object())
            return Error::from_string_literal("Invalid native title bar overlay record");
        auto const& context = record.contexts->first();
        auto const& values = record.arguments.as_object();
        auto x = values.get_u32("x"sv);
        auto width = values.get_u32("width"sv);
        auto height = values.get_u32("height"sv);
        if (context.is_empty() || context.byte_count() > 1024 || context.contains('\0')
            || values.size() != 3 || !x.has_value() || !width.has_value() || !height.has_value()
            || *x > 32768 || *width > 32768 || *height > 32768 || next.size() >= 256
            || next.contains(context))
            return Error::from_string_literal("Invalid native title bar overlay geometry");
        TRY(next.try_set(context,
            Gfx::IntRect(
                static_cast<int>(*x), 0, static_cast<int>(*width), static_cast<int>(*height))));
    }

    auto previous = move(*regions);
    *regions = move(next);
    state.preload_scripts.remove_all_matching(
        [](auto const& record) { return record.id == "firefly.title-bar-overlay.v1"sv; });
    if (!previous.is_empty() || !regions->is_empty()) {
        for (auto const& navigation : Web::HTML::all_local_navigables()) {
            if (navigation->has_been_destroyed() || !navigation->is_top_level_traversable()
                || !navigation->active_document())
                continue;
            auto& document = *navigation->active_document();
            auto old = previous.get(Web::WebDriver::BiDi::navigable_id(*navigation));
            auto current = geometry(document);
            if (old != current) {
                // The document publishes a new media snapshot for CSS substitution.
                document.record_style_environment_change(
                    Web::DOM::StyleEnvironmentChange::Everything);
                notify(document, current.value_or({}));
            }
        }
    }

    publications->remove_all_matching([](auto const& handle, auto const& entry) {
        return !regions->contains(handle) || !entry.document || !entry.target;
    });
    return {};
}

void shutdown()
{
    publications->clear();
    regions->clear();
}
}
