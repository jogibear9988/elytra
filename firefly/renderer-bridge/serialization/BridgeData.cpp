/**
 * Captures bounded data/binary graphs with no default hooks. Publication may supply an explicit
 * trusted getter reader. Materialization owns destination values, never source VM pointers.
 */

#include "serialization/BridgeData.h"
#include "serialization/BinaryView.h"
#include <AK/HashMap.h>
#include <LibGC/Root.h>
#include <LibGC/RootVector.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/FunctionObject.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/Realm.h>

namespace Firefly {
BridgeData BridgeData::from_string(Utf16View value)
{
    BridgeData result;
    Node node;
    node.kind = Kind::String;
    node.string = Utf16String::from_utf16(value);
    result.m_nodes.append(move(node));
    return result;
}

ErrorOr<BridgeData> BridgeData::capture(JS::Value value, JS::Realm& source, Limits limits,
    CaptureFunction functions, RedactObject redact, ReadAccessor accessors)
{
    return capture_impl(
        value, source, limits, move(functions), move(redact), move(accessors), false);
}

ErrorOr<BridgeData> BridgeData::capture_ipc(JS::Value value, JS::Realm& source, Limits limits)
{
    return capture_impl(value, source, limits, {}, {}, {}, true);
}

ErrorOr<BridgeData> BridgeData::capture_impl(JS::Value value, JS::Realm& source, Limits limits,
    CaptureFunction functions, RedactObject redact, ReadAccessor accessors, bool ipc_data)
{
    // Root the whole source graph while the engine allocates key vectors. All
    // traversed objects are exact engine classes and ordinary descriptor reads
    // never call hooks. Only an explicitly supplied publication accessor reader
    // executes JS. Retain visited values: getters can collect transient objects
    // whose addresses are still keys in the alias table.
    auto root = GC::Root<JS::Value>::create(value, SourceLocation::current());
    GC::Root source_realm { source };
    BridgeData result;
    HashMap<JS::Object const*, size_t> seen;
    GC::RootVector<JS::Value> source_values;
    size_t properties = 0;
    size_t units = 0;
    size_t binary_bytes = 0;
    size_t array_elements = 0;
    auto charge_string = [&](size_t length) -> ErrorOr<void> {
        if (length > limits.string_code_units - units)
            return Error::from_string_literal("Bridge string budget exceeded");
        units += length;
        return {};
    };

    auto visit
        = [&](auto& self, JS::Value input, size_t depth, JS::Object* parent) -> ErrorOr<size_t> {
        if (depth > limits.depth)
            return Error::from_string_literal("Bridge depth budget exceeded");
        // Preserve within-graph function aliases: Electron's object cache also
        // chooses the first encountered parent for a shared function identity.
        if (input.is_object()) {
            if (auto found = seen.get(&input.as_object()); found.has_value())
                return *found;
        }

        if (result.m_nodes.size() >= limits.values)
            return Error::from_string_literal("Bridge value budget exceeded");
        source_values.append(input);
        Node node {};
        if (input.is_undefined()) {
        } else if (input.is_null())
            node.kind = Kind::Null;
        else if (input.is_boolean()) {
            node.kind = Kind::Boolean;
            node.number = input.as_bool();
        } else if (input.is_number()) {
            node.kind = Kind::Number;
            node.number = input.as_double();
        } else if (input.is_string()) {
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
            node.length = TRY(functions(function, parent));
        } else if (input.is_object()) {
            auto& object = input.as_object();
            bool typed = JS::TypedArrayBase::is_engine_class_of(object);
            bool data_view = JS::DataView::is_engine_class_of(object);
            if (typed || data_view) {
                if (!limits.binary_bytes || limits.binary_bytes > 1024 * 1024
                    || &object.vm() != &source.vm() || &object.shape().realm() != &source
                    || JS::host_class_of(object))
                    return Error::from_string_literal("Unsupported bridge binary view");
                JS::ArrayBuffer* buffer = nullptr;
                if (typed) {
                    auto& view = static_cast<JS::TypedArrayBase&>(object);
                    auto type = binary_view_type(view.kind());
                    auto length = view.array_length();
                    auto bytes = view.byte_length();
                    if (!type.has_value() || length.is_auto() || length.is_detached()
                        || bytes.is_auto() || bytes.is_detached()
                        || length.length() > 1024 * 1024 / type->element_size
                        || bytes.length() != length.length() * type->element_size)
                        return Error::from_string_literal("Invalid bridge typed view slots");
                    node.kind = Kind::TypedArray;
                    node.length = length.length();
                    node.byte_offset = view.byte_offset();
                    node.view_type = type->tag;
                    buffer = view.viewed_array_buffer();
                    if (node.byte_offset % type->element_size)
                        return Error::from_string_literal("Unaligned bridge typed view");
                } else {
                    auto& view = static_cast<JS::DataView&>(object);
                    auto bytes = view.byte_length();
                    if (bytes.is_auto() || bytes.is_detached())
                        return Error::from_string_literal("Invalid bridge data view slots");
                    node.kind = Kind::DataView;
                    node.length = bytes.length();
                    node.byte_offset = view.byte_offset();
                    buffer = view.viewed_array_buffer();
                }

                if (!buffer || buffer->is_detached() || buffer->is_shared_array_buffer()
                    || !buffer->is_fixed_length() || node.byte_offset > buffer->byte_length())
                    return Error::from_string_literal("Unsupported bridge view backing buffer");
                size_t element_size = typed ? binary_view_type(node.view_type)->element_size : 1;
                if (node.length > (buffer->byte_length() - node.byte_offset) / element_size)
                    return Error::from_string_literal("Out-of-bounds bridge binary view");
                auto index = result.m_nodes.size();
                result.m_nodes.append(move(node));
                seen.set(&object, index);
                // Capture storage once, even for multiple views and forward
                // references. Native slots avoid getters/species/iterator hooks.
                auto backing = TRY(self(self, JS::Value(buffer), depth + 1, &object));
                result.m_nodes[index].buffer = backing;
                return index;
            }

            if (object.engine_class_id() == JS_LAYOUT_CLASS_ID_ARRAY_BUFFER) {
                auto& buffer = static_cast<JS::ArrayBuffer&>(object);
                if (!limits.binary_bytes || limits.binary_bytes > 1024 * 1024
                    || &object.vm() != &source.vm() || &object.shape().realm() != &source
                    || JS::host_class_of(object) || buffer.is_detached()
                    || buffer.is_shared_array_buffer() || !buffer.is_fixed_length())
                    return Error::from_string_literal("Unsupported bridge binary buffer");
                auto size = buffer.byte_length();
                if (size > limits.binary_bytes - binary_bytes)
                    return Error::from_string_literal("Bridge binary byte budget exceeded");
                binary_bytes += size;
                node.kind = Kind::ArrayBuffer;
                node.bytes = TRY(buffer.copy_to_byte_buffer());
                auto index = result.m_nodes.size();
                result.m_nodes.append(move(node));
                seen.set(&object, index);
                // Structured buffer snapshots ignore expando properties. No
                // user getter, iterator or byte-conversion hook is invoked.
                return index;
            }

            bool array = object.engine_class_id() == JS_LAYOUT_CLASS_ID_ARRAY;
            if (&object.vm() != &source.vm() || &object.shape().realm() != &source
                || JS::host_class_of(object)
                || (!array && object.engine_class_id() != JS_LAYOUT_CLASS_ID_OBJECT))
                return Error::from_string_literal(
                    "Bridge accepts only source-realm plain data objects and arrays");
            if (!array && redact && redact(object)) {
                // Match exact native identity, never a user-visible tag or shape.
                // Do not enumerate capability methods, accessors or expandos.
                node.kind = Kind::Object;
                auto index = result.m_nodes.size();
                result.m_nodes.append(move(node));
                seen.set(&object, index);
                return index;
            }

            auto* expected = array ? source.intrinsics().array_prototype().ptr()
                                   : source.intrinsics().object_prototype().ptr();
            if ((!ipc_data || array) && object.prototype() != expected
                && (array || object.prototype()))
                return Error::from_string_literal("Bridge rejects custom prototypes");
            if (object.has_intrinsic_accessors())
                return Error::from_string_literal("Bridge rejects intrinsic accessors");
            node.kind = array ? Kind::Array : Kind::Object;
            node.null_prototype = !object.prototype();
            if (array) {
                // Exact engine Array only: its magical length descriptor is not
                // included by the ordinary descriptor helper. No user hook runs.
                auto length = object.internal_get_own_property("length"_utf16);
                if (length.is_error() || !length.value().has_value()
                    || !length.value()->value.has_value() || !length.value()->value->is_number()
                    || length.value()->value->as_double() > limits.array_length)
                    return Error::from_string_literal("Bridge array length budget exceeded");
                node.length = static_cast<size_t>(length.value()->value->as_double());
                if (node.length > limits.total_array_length - array_elements)
                    return Error::from_string_literal(
                        "Bridge cumulative array length budget exceeded");
                array_elements += node.length;
            }
        } else {
            return Error::from_string_literal("Unsupported bridge data type");
        }

        size_t index = result.m_nodes.size();
        result.m_nodes.append(move(node));
        if (!input.is_object())
            return index;
        auto& object = input.as_object();
        seen.set(&object, index);
        if (result.m_nodes[index].kind == Kind::Function)
            return index;
        // The SDK enumerates into an engine-owned temporary vector before we can
        // enforce the property limit. Snapshot/output sizes are bounded; source
        // enumeration scratch space is not a hard resource containment boundary.
        auto keys = object.ordinary_own_property_keys();
        if (keys.is_error())
            return Error::from_string_literal("Cannot enumerate bridge data");
        if (keys.value().size() > limits.properties - properties)
            return Error::from_string_literal("Bridge property budget exceeded");
        properties += keys.value().size();
        for (auto key_value : keys.value()) {
            if (!key_value.is_string())
                return Error::from_string_literal("Bridge rejects symbol properties");
            TRY(charge_string(key_value.as_string().length_in_utf16_code_units()));
            auto key = key_value.as_string().utf16_string();
            auto descriptor = object.ordinary_get_own_property(key);
            if (descriptor.is_error() || !descriptor.value().has_value())
                return Error::from_string_literal("Bridge property changed during capture");
            auto const& property = *descriptor.value();
            JS::Value value = JS::js_undefined();
            if (property.is_accessor_descriptor()) {
                if (!accessors)
                    return Error::from_string_literal("Bridge rejects accessor properties");
                if (property.get.has_value() && *property.get)
                    value = TRY(accessors(object, **property.get));
            } else {
                if (!property.value.has_value())
                    return Error::from_string_literal("Bridge property has no value");
                value = *property.value;
            }

            if (result.m_nodes[index].kind == Kind::Array && key == "length"_utf16)
                continue;
            auto child = TRY(self(self, value, depth + 1, &object));
            result.m_nodes[index].properties.append(
                { move(key), child, property.enumerable.value_or(false) });
        }

        return index;
    };

    TRY(visit(visit, value, 0, nullptr));
    return result;
}

JS::ThrowCompletionOr<JS::Value> BridgeData::materialize(
    JS::Realm& destination, MaterializeFunction functions, Materialization integrity) const
{
    GC::Root destination_realm { destination };
    if (m_nodes.is_empty())
        return destination.vm().throw_completion<JS::TypeError>("Empty bridge snapshot"_utf16);
    if (integrity == Materialization::Frozen) {
        for (auto const& node : m_nodes) {
            if (node.kind == Kind::ArrayBuffer || node.kind == Kind::TypedArray
                || node.kind == Kind::DataView)
                return destination.vm().throw_completion<JS::TypeError>(
                    "Binary snapshots require explicit mutable materialization"_utf16);
        }
    }

    GC::RootVector<JS::Value> values;
    for (auto const& node : m_nodes) {
        switch (node.kind) {
        case Kind::Undefined:
            values.append(JS::js_undefined());
            break;
        case Kind::Null:
            values.append(JS::js_null());
            break;
        case Kind::Boolean:
            values.append(JS::Value(node.number != 0));
            break;
        case Kind::Number:
            values.append(JS::Value(node.number));
            break;
        case Kind::ArrayBuffer: {
            auto bytes = ByteBuffer::copy(node.bytes);
            if (bytes.is_error())
                return destination.vm().throw_completion<JS::RangeError>(
                    "Cannot allocate bridge binary snapshot"_utf16);
            values.append(JS::ArrayBuffer::create(destination, bytes.release_value()).ptr());
            break;
        }

        case Kind::TypedArray:
        case Kind::DataView:
            values.append(JS::js_undefined()); // Construct after every backing buffer exists.
            break;

        case Kind::String:
            values.append(JS::PrimitiveString::create(destination.vm(), node.string));
            break;
        case Kind::Function: {
            if (!functions)
                return destination.vm().throw_completion<JS::TypeError>(
                    "Missing bridge function materializer"_utf16);
            auto function = TRY(functions(node.length));
            if (!function.is_function() || &function.as_function().vm() != &destination.vm()
                || function.as_function().realm() != &destination)
                return destination.vm().throw_completion<JS::TypeError>(
                    "Invalid bridge function destination"_utf16);
            values.append(function);
            break;
        }

        case Kind::Object:
            values.append(JS::Object::create(destination,
                node.null_prototype ? nullptr : destination.intrinsics().object_prototype().ptr()));
            break;
        case Kind::Array:
            values.append(TRY(JS::Array::create(destination, node.length)));
            break;
        }
    }

    for (size_t i = 0; i < m_nodes.size(); ++i) {
        auto const& node = m_nodes[i];
        if (node.kind != Kind::TypedArray && node.kind != Kind::DataView)
            continue;
        if (node.buffer >= m_nodes.size() || m_nodes[node.buffer].kind != Kind::ArrayBuffer)
            return destination.vm().throw_completion<JS::TypeError>(
                "Invalid binary view backing"_utf16);
        auto& buffer = static_cast<JS::ArrayBuffer&>(values[node.buffer].as_object());
        auto type = binary_view_type(node.view_type);
        size_t width = node.kind == Kind::TypedArray && type.has_value() ? type->element_size : 1;
        if ((node.kind == Kind::TypedArray && !type.has_value()) || node.byte_offset % width
            || node.byte_offset > buffer.byte_length()
            || node.length > (buffer.byte_length() - node.byte_offset) / width)
            return destination.vm().throw_completion<JS::TypeError>(
                "Invalid binary view geometry"_utf16);
        if (node.kind == Kind::TypedArray)
            values[i] = JS::TypedArrayBase::create_from_slots(
                destination, type->kind, buffer, node.length, node.length * width, node.byte_offset)
                            .ptr();
        else
            values[i]
                = JS::DataView::create(destination, &buffer, node.length, node.byte_offset).ptr();
    }
    // Allocate every container first to preserve cycles and shared identities.
    // Define own properties directly: even __proto__ cannot invoke a setter.
    for (size_t i = 0; i < m_nodes.size(); ++i) {
        for (auto const& property : m_nodes[i].properties) {
            JS::PropertyAttributes attributes {};
            attributes.set_enumerable(property.enumerable);
            attributes.set_configurable(true);
            attributes.set_writable(true);
            values[i].as_object().define_direct_property(
                property.key, values[property.value], attributes);
        }
    }

    for (auto value : values) {
        bool binary = value.is_object()
            && (JS::TypedArrayBase::is_engine_class_of(value.as_object())
                || JS::DataView::is_engine_class_of(value.as_object())
                || value.as_object().engine_class_id() == JS_LAYOUT_CLASS_ID_ARRAY_BUFFER);
        if (value.is_object()
            && (integrity == Materialization::Frozen
                || (integrity == Materialization::FrozenContainers && !binary))
            && !TRY(value.as_object().set_integrity_level(JS::Object::IntegrityLevel::Frozen)))
            return destination.vm().throw_completion<JS::TypeError>(
                "Cannot freeze bridge data"_utf16);
    }

    return values[0];
}
}
