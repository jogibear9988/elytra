#include "BridgeData.h"
#include <AK/HashMap.h>
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>

namespace Firefly {
ErrorOr<BridgeData> BridgeData::capture(JS::Value value, JS::Realm& source, Limits limits, CaptureFunction functions)
{
    // Root the whole source graph while the engine allocates key vectors. All
    // traversed objects are exact engine classes, and ordinary descriptor reads
    // never call getters or proxy/host hooks. No JS code runs during capture.
    auto root = GC::Root<JS::Value>::create(value, SourceLocation::current());
    GC::Root source_realm { source };
    BridgeData result;
    HashMap<JS::Object const*, size_t> seen;
    size_t properties = 0;
    size_t units = 0;
    auto charge_string = [&](size_t length) -> ErrorOr<void> {
        if (length > limits.string_code_units - units)
            return Error::from_string_literal("Bridge string budget exceeded");
        units += length;
        return {};
    };
    auto visit = [&](auto& self, JS::Value input, size_t depth) -> ErrorOr<size_t> {
        if (depth > limits.depth)
            return Error::from_string_literal("Bridge depth budget exceeded");
        if (input.is_object()) {
            if (auto found = seen.get(&input.as_object()); found.has_value()) return *found;
        }
        if (result.m_nodes.size() >= limits.values)
            return Error::from_string_literal("Bridge value budget exceeded");
        Node node {};
        if (input.is_undefined()) { }
        else if (input.is_null()) node.kind = Kind::Null;
        else if (input.is_boolean()) { node.kind = Kind::Boolean; node.number = input.as_bool(); }
        else if (input.is_number()) { node.kind = Kind::Number; node.number = input.as_double(); }
        else if (input.is_string()) {
            if (&input.as_string().vm() != &source.vm())
                return Error::from_string_literal("Bridge value belongs to another VM");
            TRY(charge_string(input.as_string().length_in_utf16_code_units()));
            node.kind = Kind::String;
            node.string = input.as_string().utf16_string();
        } else if (input.is_function() && functions) {
            auto& function = input.as_function();
            if (&function.vm() != &source.vm() || function.realm() != &source)
                return Error::from_string_literal("Bridge function requires its own source realm");
            node.kind = Kind::Function;
            node.length = TRY(functions(function));
        } else if (input.is_object()) {
            auto& object = input.as_object();
            bool array = object.engine_class_id() == JS_LAYOUT_CLASS_ID_ARRAY;
            if (&object.vm() != &source.vm() || &object.shape().realm() != &source ||
                JS::host_class_of(object) || (!array && object.engine_class_id() != JS_LAYOUT_CLASS_ID_OBJECT))
                return Error::from_string_literal("Bridge accepts only source-realm plain data objects and arrays");
            auto* expected = array ? source.intrinsics().array_prototype().ptr() : source.intrinsics().object_prototype().ptr();
            if (object.prototype() != expected && (array || object.prototype()))
                return Error::from_string_literal("Bridge rejects custom prototypes");
            if (object.has_intrinsic_accessors())
                return Error::from_string_literal("Bridge rejects intrinsic accessors");
            node.kind = array ? Kind::Array : Kind::Object;
            node.null_prototype = !object.prototype();
            if (array) {
                // Exact engine Array only: its magical length descriptor is not
                // included by the ordinary descriptor helper. No user hook runs.
                auto length = object.internal_get_own_property("length"_utf16);
                if (length.is_error() || !length.value().has_value() || !length.value()->value.has_value() ||
                    !length.value()->value->is_number() || length.value()->value->as_double() > limits.array_length)
                    return Error::from_string_literal("Bridge array length budget exceeded");
                node.length = static_cast<size_t>(length.value()->value->as_double());
            }
        } else {
            return Error::from_string_literal("Unsupported bridge data type");
        }
        size_t index = result.m_nodes.size();
        result.m_nodes.append(move(node));
        if (!input.is_object()) return index;
        auto& object = input.as_object();
        seen.set(&object, index);
        if (result.m_nodes[index].kind == Kind::Function) return index;
        // The SDK enumerates into an engine-owned temporary vector before we can
        // enforce the property limit. Snapshot/output sizes are bounded; source
        // enumeration scratch space is not a hard resource containment boundary.
        auto keys = object.ordinary_own_property_keys();
        if (keys.is_error()) return Error::from_string_literal("Cannot enumerate bridge data");
        if (keys.value().size() > limits.properties - properties)
            return Error::from_string_literal("Bridge property budget exceeded");
        properties += keys.value().size();
        for (auto key_value : keys.value()) {
            if (!key_value.is_string()) return Error::from_string_literal("Bridge rejects symbol properties");
            TRY(charge_string(key_value.as_string().length_in_utf16_code_units()));
            auto key = key_value.as_string().utf16_string();
            auto descriptor = object.ordinary_get_own_property(key);
            if (descriptor.is_error() || !descriptor.value().has_value() || descriptor.value()->is_accessor_descriptor() || !descriptor.value()->value.has_value())
                return Error::from_string_literal("Bridge rejects accessor properties");
            if (result.m_nodes[index].kind == Kind::Array && key == "length"_utf16) continue;
            auto child = TRY(self(self, *descriptor.value()->value, depth + 1));
            result.m_nodes[index].properties.append({ move(key), child, descriptor.value()->enumerable.value_or(false) });
        }
        return index;
    };
    TRY(visit(visit, value, 0));
    return result;
}

JS::ThrowCompletionOr<JS::Value> BridgeData::materialize(JS::Realm& destination, MaterializeFunction functions) const
{
    GC::Root destination_realm { destination };
    if (m_nodes.is_empty())
        return destination.vm().throw_completion<JS::TypeError>("Empty bridge snapshot"sv);
    GC::RootVector<JS::Value> values;
    for (auto const& node : m_nodes) {
        switch (node.kind) {
        case Kind::Undefined: values.append(JS::js_undefined()); break;
        case Kind::Null: values.append(JS::js_null()); break;
        case Kind::Boolean: values.append(JS::Value(node.number != 0)); break;
        case Kind::Number: values.append(JS::Value(node.number)); break;
        case Kind::String: values.append(JS::PrimitiveString::create(destination.vm(), node.string)); break;
        case Kind::Function: {
            if (!functions) return destination.vm().throw_completion<JS::TypeError>("Missing bridge function materializer"sv);
            auto function = TRY(functions(node.length));
            if (!function.is_function() || &function.as_function().vm() != &destination.vm() || function.as_function().realm() != &destination)
                return destination.vm().throw_completion<JS::TypeError>("Invalid bridge function destination"sv);
            values.append(function);
            break;
        }
        case Kind::Object: values.append(JS::Object::create(destination, node.null_prototype ? nullptr : destination.intrinsics().object_prototype().ptr())); break;
        case Kind::Array: values.append(TRY(JS::Array::create(destination, node.length))); break;
        }
    }
    // Allocate every container first to preserve cycles and shared identities.
    // Define own properties directly: even __proto__ cannot invoke a setter.
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        for (auto const& property : m_nodes[i].properties) {
            JS::PropertyAttributes attributes {};
            attributes.set_enumerable(property.enumerable);
            attributes.set_configurable(true);
            attributes.set_writable(true);
            values[i].as_object().define_direct_property(property.key, values[property.value], attributes);
        }
    }
    for (auto value : values) {
        if (value.is_object() && !TRY(value.as_object().set_integrity_level(JS::Object::IntegrityLevel::Frozen)))
            return destination.vm().throw_completion<JS::TypeError>("Cannot freeze bridge data"sv);
    }
    return values[0];
}
}
