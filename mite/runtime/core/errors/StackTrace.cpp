/** Native frame identity, lazy headers and GC-owned captured-stack state; no V8 dependency. */
#include "StackTrace.h"
#include "CallSites.h"
#include "core/GCCaptures.h"
#include <AK/ScopeGuard.h>
#include <AK/Utf16StringBuilder.h>
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/ExecutionContext.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PropertyDescriptor.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <cmath>

namespace Mite {
namespace {
    // Every captured state field has an explicit edge visitor; LibGC never
    // guesses the contents of a native lambda capture buffer.
    struct State {
        void visit_edges(GC::Cell::Visitor& visitor) const
        {
            visitor.visit(records);
            visitor.visit(lookup);
            visitor.visit(store);
            visitor.visit(constructor);
            visitor.visit(native_getter);
            visitor.visit(native_setter);
            visitor.visit(sites);
        }

        GC::Ptr<JS::Object> records;
        GC::Ptr<JS::FunctionObject> lookup;
        GC::Ptr<JS::FunctionObject> store;
        GC::Ptr<JS::Object> constructor;
        GC::Ptr<JS::FunctionObject> native_getter;
        GC::Ptr<JS::FunctionObject> native_setter;
        GC::Ptr<JS::Object> sites;
    };

    JS::Completion unsupported(JS::VM& vm, Utf16String message)
    {
        GC::Root error { JS::Error::create(vm.realm(), move(message)) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, "ERR_MITE_UNSUPPORTED_API"_utf16),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<JS::Value> find_record(JS::VM& vm, State const& state, JS::Value receiver)
    {
        if (!receiver.is_object() || receiver.as_object().is_proxy())
            return JS::js_undefined();
        auto* object = &receiver.as_object();
        while (object && !object->is_proxy()) {
            auto record = TRY(JS::call(vm, *state.lookup, state.records.ptr(), object));
            if (!record.is_undefined())
                return record;
            object = TRY(object->internal_get_prototype_of());
        }

        return JS::js_undefined();
    }

    JS::ThrowCompletionOr<Utf16String> header(JS::VM& vm, JS::Object& target)
    {
        GC::RootVector<JS::Value> values;
        auto name_value = TRY(target.get("name"_utf16));
        values.append(name_value);
        auto name = name_value.is_undefined() ? "Error"_utf16 : TRY(name_value.to_utf16_string(vm));
        auto message_value = TRY(target.get("message"_utf16));
        values.append(message_value);
        auto message = message_value.is_undefined() ? Utf16String {}
                                                    : TRY(message_value.to_utf16_string(vm));
        if (name.is_empty())
            return message;
        if (message.is_empty())
            return name;
        return Utf16String::formatted("{}: {}", name, message);
    }

    JS::ThrowCompletionOr<JS::Value> default_stack(
        JS::VM& vm, JS::Object& record, JS::Object& target)
    {
        if (TRY(record.get("header"_utf16)).to_boolean())
            return unsupported(
                vm, "Recursive captured-stack name/message formatting is not implemented"_utf16);
        record.define_direct_property("header"_utf16, JS::Value(true), JS::default_attributes);
        ScopeGuard clear_header { [&record] {
            record.define_direct_property("header"_utf16, JS::Value(false), JS::default_attributes);
        } };
        auto title = TRY(header(vm, target));
        auto frames = TRY(record.get("frames"_utf16));
        return JS::PrimitiveString::create(
            vm, Utf16String::formatted("{}{}", title, frames.as_string().utf16_string_view()));
    }

    JS::ThrowCompletionOr<JS::Value> read_stack(JS::VM& vm, State const& state)
    {
        auto receiver = vm.this_value();
        auto value = TRY(find_record(vm, state, receiver));
        if (value.is_undefined()) {
            // Ordinary engine Errors keep their native stack semantics. A
            // captured getter borrowed onto one delegates to the original slot.
            if (receiver.is_object() && JS::Error::is_engine_class_of(receiver.as_object())) {
                auto own = TRY(receiver.as_object().internal_get_own_property("stack"_utf16));
                if (own.has_value() && own->value.has_value())
                    return *own->value;
                return JS::call(vm, *state.native_getter, receiver);
            }

            return JS::js_undefined();
        }

        GC::Root record { value.as_object() };
        if (TRY(record->get("ready"_utf16)).to_boolean())
            return record->get("value"_utf16);
        // A formatter may read this stack again or create another captured
        // error. Nested reads use the default formatter instead of reentering
        // the realm's prepare hook. Always release the guard on abrupt completion.
        bool nested = TRY(state.records->get("preparing"_utf16)).to_boolean();
        if (!nested)
            state.records->define_direct_property(
                "preparing"_utf16, JS::Value(true), JS::default_attributes);
        ScopeGuard clear_preparing { [&state, nested] {
            if (!nested)
                state.records->define_direct_property(
                    "preparing"_utf16, JS::Value(false), JS::default_attributes);
        } };
        auto prepare
            = nested ? JS::js_undefined() : TRY(state.constructor->get("prepareStackTrace"_utf16));
        GC::Root target { TRY(record->get("target"_utf16)).as_object() };
        GC::RootVector<JS::Value> result;
        if (prepare.is_function()) {
            GC::Root callback { prepare.as_function() };
            auto sites = TRY(record->get("sites"_utf16));
            result.append(
                TRY(JS::call(vm, *callback, state.constructor.ptr(), target.ptr(), sites)));
        } else {
            result.append(TRY(default_stack(vm, *record, *target)));
        }

        record->define_direct_property("value"_utf16, result[0], JS::default_attributes);
        record->define_direct_property("ready"_utf16, JS::Value(true), JS::default_attributes);
        return result[0];
    }

    JS::ThrowCompletionOr<JS::Value> write_stack(JS::VM& vm, State const& state)
    {
        auto receiver = vm.this_value();
        auto value = TRY(find_record(vm, state, receiver));
        if (value.is_object()) {
            auto& record = value.as_object();
            record.define_direct_property("value"_utf16, vm.argument(0), JS::default_attributes);
            record.define_direct_property("ready"_utf16, JS::Value(true), JS::default_attributes);
        } else if (receiver.is_object() && JS::Error::is_engine_class_of(receiver.as_object())) {
            TRY(JS::call(vm, *state.native_setter, receiver, vm.argument(0)));
        }

        return JS::js_undefined();
    }

    JS::ThrowCompletionOr<JS::Value> capture(
        JS::VM& vm, State const& state, JS::FunctionObject& getter, JS::FunctionObject& setter)
    {
        auto target_value = vm.argument(0);
        if (!target_value.is_object() || target_value.as_object().is_proxy())
            return vm.throw_completion<JS::TypeError>(
                "captureStackTrace requires an extensible non-Proxy object"_utf16);
        GC::Root target { target_value.as_object() };
        if (!target->extensible())
            return vm.throw_completion<JS::TypeError>(
                "captureStackTrace requires an extensible object"_utf16);
        auto descriptor = TRY(target->internal_get_own_property("stack"_utf16));
        if (descriptor.has_value() && !descriptor->configurable.value_or(false))
            return vm.throw_completion<JS::TypeError>(
                "Cannot replace non-configurable stack property"_utf16);

        // V8 reads data properties without invoking accessors, including inherited data.
        auto limit_value = state.constructor->get_without_side_effects("stackTraceLimit"_utf16);
        auto enabled = limit_value.is_number();
        auto number = enabled ? limit_value.as_double() : 0;
        constexpr size_t maximum_frames = 4096;
        auto limit = !std::isnan(number) && number > 0
            ? static_cast<size_t>(std::min(std::floor(number), static_cast<double>(maximum_frames)))
            : 0;
        auto stack = vm.stack_trace();
        size_t first = 0;
        if (!stack.is_empty() && stack[0].execution_context == &vm.running_execution_context())
            first = 1; // The native capture function is not a caller frame.
        auto skip = vm.argument(1);
        if (skip.is_function()) {
            first = stack.size();
            for (size_t index = 0; index < stack.size(); ++index) {
                if (stack[index].execution_context
                    && stack[index].execution_context->function.ptr() == &skip.as_function()) {
                    first = index + 1;
                    break;
                }
            }
        }

        Utf16StringBuilder frames;
        Vector<CapturedSite> metadata;
        size_t count = 0;
        for (size_t index = first; index < stack.size(); ++index) {
            auto const& frame = stack[index];
            if (!frame.execution_context)
                return unsupported(vm, "LibJS stack frame has no live execution context"_utf16);
            auto* function = frame.execution_context->function.ptr();
            if (!function && !frame.source_range.has_value())
                continue; // The embedder's outer realm context has no JS call site.
            if (count == limit) {
                if (limit == maximum_frames && number > static_cast<double>(maximum_frames))
                    return unsupported(vm, "Captured stack exceeds Mite's 4096-frame limit"_utf16);
                break;
            }

            ++count;
            auto name = function ? function->name_for_call_stack() : Utf16String {};
            Utf16StringBuilder display;
            if (frame.source_range.has_value()) {
                auto const& range = *frame.source_range;
                if (!name.is_empty())
                    display.appendff("{} (", name);
                display.appendff(
                    "{}:{}:{}", range.filename(), range.start.line, range.start.column);
                if (!name.is_empty())
                    display.append_ascii(")"sv);
            } else {
                display.append(name.is_empty() ? "<unknown>"_utf16 : name);
            }

            frames.append_ascii("\n    at "sv);
            frames.append(display.view());
            metadata.append({ name, frame.source_range, display.to_string(),
                function && JS::NativeFunction::is_engine_class_of(*function) });
        }

        // Finish borrowing every execution context before allocating JS CallSites
        // or invoking any intrinsic. Only owned source/string metadata survives.
        GC::RootVector<JS::Value> sites;
        for (auto const& site : metadata)
            sites.append(TRY(make_call_site(vm, *state.sites, site)));
        GC::Root site_array { JS::Array::create_from(vm.realm(), sites) };

        GC::Root record { JS::Object::create(vm.realm(), nullptr) };
        record->define_direct_property("target"_utf16, target.ptr(), JS::default_attributes);
        record->define_direct_property("sites"_utf16, site_array.ptr(), JS::default_attributes);
        record->define_direct_property("frames"_utf16,
            JS::PrimitiveString::create(vm, frames.to_string()), JS::default_attributes);
        record->define_direct_property("ready"_utf16, JS::Value(!enabled), JS::default_attributes);
        record->define_direct_property("header"_utf16, JS::Value(false), JS::default_attributes);
        record->define_direct_property("value"_utf16, JS::js_undefined(), JS::default_attributes);
        JS::PropertyDescriptor installed;
        installed.get = &getter;
        installed.set = &setter;
        installed.enumerable = false;
        installed.configurable = true;
        TRY(target->define_property_or_throw("stack"_utf16, installed));
        TRY(JS::call(vm, *state.store, state.records.ptr(), target.ptr(), record.ptr()));
        return JS::js_undefined();
    }
}

JS::ThrowCompletionOr<void> install_error_stacks(JS::Realm& realm)
{
    auto& vm = realm.vm();
    GC::Root constructor { TRY(realm.global_object().get("Error"_utf16)).as_object() };
    GC::Root weak_constructor { TRY(realm.global_object().get("WeakMap"_utf16)).as_function() };
    GC::Root records { TRY(JS::construct(vm, *weak_constructor)) };
    records->define_direct_property("preparing"_utf16, JS::Value(false), JS::default_attributes);
    GC::Root prototype { TRY(weak_constructor->get("prototype"_utf16)).as_object() };
    GC::Root lookup { TRY(prototype->get("get"_utf16)).as_function() };
    GC::Root store { TRY(prototype->get("set"_utf16)).as_function() };
    GC::Root error_prototype { TRY(constructor->get("prototype"_utf16)).as_object() };
    auto native_stack = TRY(error_prototype->internal_get_own_property("stack"_utf16));
    VERIFY(
        native_stack.has_value() && native_stack->get.has_value() && native_stack->set.has_value());
    GC::Root sites { TRY(install_call_sites(realm)) };
    State state { records.ptr(), lookup.ptr(), store.ptr(), constructor.ptr(), *native_stack->get,
        *native_stack->set, sites.ptr() };
    GC::Root getter { JS::NativeFunction::create(realm,
        GC::create_function(realm.vm().heap(), GC::captures(state),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return read_stack(vm, state);
            }),
        0, "get stack"_utf16) };
    GC::Root setter { JS::NativeFunction::create(realm,
        GC::create_function(realm.vm().heap(), GC::captures(state),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return write_stack(vm, state);
            }),
        1, "set stack"_utf16) };
    constructor->define_direct_property(
        "stackTraceLimit"_utf16, JS::Value(10), JS::default_attributes);
    constructor->define_native_function(realm, "captureStackTrace"_utf16,
        GC::create_function(realm.vm().heap(),
            GC::captures(state, GC::Ptr<JS::FunctionObject>(getter.ptr()),
                GC::Ptr<JS::FunctionObject>(setter.ptr())),
            [](auto& state, auto& get, auto& set, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                return capture(vm, state, *get, *set);
            }),
        2, JS::Attribute::Writable | JS::Attribute::Configurable);
    return {};
}
}
