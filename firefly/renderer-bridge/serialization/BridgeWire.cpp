/**
 * Encodes and validates the private data-only wire graph. Validate schema, reachability and
 * resource bounds before allocating destination JavaScript values.
 */

#include "serialization/BinaryPolicy.h"
#include "serialization/BinaryView.h"
#include "serialization/BridgeData.h"
#include <AK/HashTable.h>
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <AK/Utf16StringBuilder.h>
#include <bit>
#include <cmath>
#include <limits>

namespace Firefly {
namespace {
    static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559);
    constexpr size_t binary_header_bytes = 8;

    ErrorOr<void> check_limits(BridgeData::Limits limits)
    {
        if (limits.wire_profile != BridgeData::WireProfile::Default
            && limits.wire_profile != BridgeData::WireProfile::ApplicationIpc)
            return Error::from_string_literal("Unknown wire limits profile");
        auto maximum = limits.wire_profile == BridgeData::WireProfile::ApplicationIpc
            ? application_ipc_limits()
            : BridgeData::Limits {};
        // Binary snapshots were already an explicit bounded opt-in.
        if (limits.wire_profile == BridgeData::WireProfile::Default)
            maximum.binary_bytes = 1024 * 1024;
        if (limits.values > maximum.values || limits.properties > maximum.properties
            || limits.string_code_units > maximum.string_code_units || limits.depth > maximum.depth
            || limits.array_length > maximum.array_length
            || limits.binary_bytes > maximum.binary_bytes || limits.wire_bytes > maximum.wire_bytes
            || limits.total_array_length > maximum.total_array_length
            || limits.wire_bytes < binary_header_bytes)
            return Error::from_string_literal("Wire limits exceed hard maximum");
        return {};
    }

    ErrorOr<size_t> integer(JsonValue const& input, size_t maximum)
    {
        auto number = input.get_double_with_precision_loss();
        if (!number.has_value() || !std::isfinite(*number) || *number < 0 || *number > maximum
            || std::floor(*number) != *number)
            return Error::from_string_literal("Invalid wire integer");
        return static_cast<size_t>(*number);
    }

    ErrorOr<JsonArray> units(Utf16String const& text)
    {
        JsonArray result;
        auto view = text.utf16_view();
        for (size_t i = 0; i < view.length_in_code_units(); ++i)
            TRY(result.append(static_cast<u32>(view.code_unit_at(i))));
        return result;
    }

    ErrorOr<Utf16String> text(JsonValue const& value, size_t& used, size_t maximum)
    {
        if (!value.is_array() || value.as_array().size() > maximum - used)
            return Error::from_string_literal("Wire UTF-16 budget exceeded");
        used += value.as_array().size();
        Utf16StringBuilder result;
        for (auto const& unit : value.as_array().values())
            result.append_code_unit(static_cast<char16_t>(TRY(integer(unit, 65535))));
        return result.to_string();
    }

    bool is_array_index_out_of_bounds(Utf16String const& key, size_t length)
    {
        auto view = key.utf16_view();
        if (view.is_empty() || (view.length_in_code_units() > 1 && view.code_unit_at(0) == '0'))
            return false;
        u64 number = 0;
        for (size_t i = 0; i < view.length_in_code_units(); ++i) {
            auto unit = view.code_unit_at(i);
            if (unit < '0' || unit > '9')
                return false;
            number = number * 10 + unit - '0';
            if (number >= 0xffffffffULL)
                return false; // Not an ECMAScript array index.
        }

        return number >= length;
    }

    ErrorOr<void> preflight(StringView bytes, size_t wire_bytes)
    {
        if (bytes.is_empty() || bytes.length() > wire_bytes)
            return Error::from_string_literal("Wire byte budget exceeded");
        size_t depth = 0;
        bool quoted = false, escaped = false;
        for (char ch : bytes) {
            if (quoted) {
                if (escaped)
                    escaped = false;
                else if (ch == '\\')
                    escaped = true;
                else if (ch == '"')
                    quoted = false;
            } else if (ch == '"')
                quoted = true;
            else if (ch == '[') {
                if (++depth > 8)
                    return Error::from_string_literal("Wire JSON nesting budget exceeded");
            } else if (ch == ']') {
                if (!depth)
                    return Error::from_string_literal("Invalid wire JSON nesting");
                --depth;
            } else if (ch == '{' || ch == '}')
                return Error::from_string_literal("Wire objects are not permitted");
        }

        if (depth || quoted)
            return Error::from_string_literal("Unterminated wire JSON");
        return {};
    }
}

ErrorOr<void> BridgeData::validate_wire_graph(Limits limits) const
{
    TRY(check_limits(limits));
    if (m_nodes.is_empty() || m_nodes.size() > limits.values)
        return Error::from_string_literal("Wire node budget exceeded");
    size_t properties = 0, strings = 0, array_length = 0, binary_bytes = 0;
    auto charge = [](size_t& used, size_t amount, size_t maximum) -> ErrorOr<void> {
        if (amount > maximum - used)
            return Error::from_string_literal("Wire graph budget exceeded");
        used += amount;
        return {};
    };

    for (auto const& node : m_nodes) {
        if (node.kind == Kind::Function)
            return Error::from_string_literal("Function tokens cannot cross IPC");
        if (node.kind == Kind::ArrayBuffer) {
            if (!limits.binary_bytes || !node.properties.is_empty())
                return Error::from_string_literal("Binary wire requires explicit byte policy");
            TRY(charge(binary_bytes, node.bytes.size(), limits.binary_bytes));
        }

        if (node.kind == Kind::TypedArray || node.kind == Kind::DataView) {
            if (!limits.binary_bytes || !node.properties.is_empty() || node.buffer >= m_nodes.size()
                || m_nodes[node.buffer].kind != Kind::ArrayBuffer)
                return Error::from_string_literal("Invalid wire binary view backing");
            auto size = m_nodes[node.buffer].bytes.size();
            auto type = binary_view_type(node.view_type);
            size_t width
                = node.kind == Kind::TypedArray && type.has_value() ? type->element_size : 1;
            if ((node.kind == Kind::TypedArray && !type.has_value()) || node.byte_offset % width
                || node.byte_offset > size || node.length > (size - node.byte_offset) / width)
                return Error::from_string_literal("Invalid wire binary view geometry");
        }

        TRY(charge(strings, node.string.length_in_code_units(), limits.string_code_units));
        TRY(charge(properties, node.properties.size(), limits.properties));
        if (node.kind == Kind::Array) {
            if (node.length > limits.array_length)
                return Error::from_string_literal("Wire array length exceeded");
            TRY(charge(array_length, node.length, limits.total_array_length));
            // Account for the implicit own length descriptor as capture does.
            TRY(charge(properties, 1, limits.properties));
            TRY(charge(strings, 6, limits.string_code_units));
        }

        HashTable<Utf16String> keys;
        for (auto const& property : node.properties) {
            if (property.value >= m_nodes.size() || keys.contains(property.key))
                return Error::from_string_literal("Invalid or duplicate wire property");
            keys.set(property.key);
            TRY(charge(strings, property.key.length_in_code_units(), limits.string_code_units));
            if (node.kind == Kind::Array
                && (property.key == "length"_utf16
                    || is_array_index_out_of_bounds(property.key, node.length)))
                return Error::from_string_literal("Wire array property exceeds length");
        }
    }

    HashTable<size_t> seen;
    auto visit = [&](auto& self, size_t index, size_t depth) -> ErrorOr<void> {
        if (depth > limits.depth)
            return Error::from_string_literal("Wire graph depth exceeded");
        if (seen.contains(index))
            return {};
        seen.set(index);
        if (m_nodes[index].kind == Kind::TypedArray || m_nodes[index].kind == Kind::DataView)
            TRY(self(self, m_nodes[index].buffer, depth + 1));
        for (auto const& property : m_nodes[index].properties)
            TRY(self(self, property.value, depth + 1));
        return {};
    };

    TRY(visit(visit, 0, 0));
    if (seen.size() != m_nodes.size())
        return Error::from_string_literal("Unreachable wire nodes");
    return {};
}

ErrorOr<ByteString> BridgeData::encode_wire(Limits limits) const
{
    TRY(validate_wire_graph(limits));
    JsonArray nodes;
    bool binary = false;
    size_t binary_bytes = 0;
    for (auto const& node : m_nodes) {
        JsonArray row;
        TRY(row.append(static_cast<u32>(node.kind)));
        switch (node.kind) {
        case Kind::Undefined:
        case Kind::Null:
            break;
        case Kind::Boolean:
            TRY(row.append(node.number != 0));
            break;
        case Kind::Number: {
            auto bits = std::bit_cast<u64>(node.number);
            if ((bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL
                && (bits & 0x000fffffffffffffULL))
                bits = 0x7ff8000000000000ULL; // Canonical JS NaN; never encode engine tags.
            char hex[16];
            for (size_t i = 0; i < 16; ++i) {
                hex[15 - i] = "0123456789abcdef"[bits & 15];
                bits >>= 4;
            }

            TRY(row.append(StringView(hex, 16)));
            break;
        }

        case Kind::String:
            TRY(row.append(TRY(units(node.string))));
            break;
        case Kind::ArrayBuffer:
            binary = true;
            binary_bytes += node.bytes.size();
            TRY(row.append(node.bytes.size()));
            break;
        case Kind::TypedArray:
        case Kind::DataView:
            if (node.kind == Kind::TypedArray)
                TRY(row.append(node.view_type));
            TRY(row.append(node.buffer));
            TRY(row.append(node.byte_offset));
            TRY(row.append(node.length));
            break;
        case Kind::Object:
        case Kind::Array: {
            if (node.kind == Kind::Array)
                TRY(row.append(node.length));
            else
                TRY(row.append(node.null_prototype));
            JsonArray properties;
            for (auto const& property : node.properties) {
                JsonArray entry;
                TRY(entry.append(TRY(units(property.key))));
                TRY(entry.append(property.value));
                TRY(entry.append(property.enumerable));
                TRY(properties.append(move(entry)));
            }

            TRY(row.append(move(properties)));
            break;
        }

        case Kind::Function:
            VERIFY_NOT_REACHED();
        }

        TRY(nodes.append(move(row)));
    }

    JsonArray envelope;
    TRY(envelope.append(binary ? 2 : 1));
    TRY(envelope.append(move(nodes)));
    auto result = JsonValue(move(envelope)).serialized().to_byte_string();
    TRY(preflight(result, limits.wire_bytes));
    if (!binary)
        return result;
    // Only graph metadata is JSON. Payload bytes remain raw, including NUL and
    // non-UTF-8. This format must never be passed through a JS/UTF-8 string API.
    if (result.length() > limits.wire_bytes - binary_header_bytes
        || binary_bytes > limits.wire_bytes - binary_header_bytes - result.length())
        return Error::from_string_literal("Binary wire byte budget exceeded");
    auto output = TRY(
        ByteBuffer::create_uninitialized(binary_header_bytes + result.length() + binary_bytes));
    __builtin_memcpy(output.data(), "FFB2", 4);
    for (size_t i = 0; i < 4; ++i)
        output[4 + i] = static_cast<u8>(result.length() >> (i * 8));
    __builtin_memcpy(output.data() + binary_header_bytes, result.characters(), result.length());
    size_t offset = binary_header_bytes + result.length();
    for (auto const& node : m_nodes) {
        if (node.kind == Kind::ArrayBuffer && !node.bytes.is_empty()) {
            __builtin_memcpy(output.data() + offset, node.bytes.data(), node.bytes.size());
            offset += node.bytes.size();
        }
    }

    return ByteString(StringView(reinterpret_cast<char const*>(output.data()), output.size()));
}

ErrorOr<BridgeData> BridgeData::decode_wire(StringView bytes, Limits limits)
{
    TRY(check_limits(limits));
    if (bytes.is_empty() || bytes.length() > limits.wire_bytes)
        return Error::from_string_literal("Wire byte budget exceeded");
    bool binary = bytes.starts_with("FFB2"sv);
    auto metadata = bytes;
    ReadonlyBytes payload;
    if (binary) {
        if (!limits.binary_bytes || bytes.length() < binary_header_bytes)
            return Error::from_string_literal("Binary wire requires explicit byte policy/header");
        u32 length = 0;
        for (size_t i = 0; i < 4; ++i)
            length |= static_cast<u32>(static_cast<u8>(bytes[4 + i])) << (i * 8);
        if (length > bytes.length() - binary_header_bytes)
            return Error::from_string_literal("Truncated binary wire metadata");
        metadata = bytes.substring_view(binary_header_bytes, length);
        payload = bytes.bytes().slice(binary_header_bytes + length);
        if (payload.size() > limits.binary_bytes)
            return Error::from_string_literal("Binary wire payload budget exceeded");
    }

    // Bound bytes/JSON recursion before allocating the parser tree.
    TRY(preflight(metadata, limits.wire_bytes));
    auto parsed = TRY(JsonValue::from_string(metadata));
    if (!parsed.is_array() || parsed.as_array().size() != 2
        || TRY(integer(parsed.as_array()[0], 2)) != (binary ? 2 : 1)
        || !parsed.as_array()[1].is_array())
        return Error::from_string_literal("Invalid wire envelope");
    auto const& rows = parsed.as_array()[1].as_array();
    if (rows.is_empty() || rows.size() > limits.values)
        return Error::from_string_literal("Wire node budget exceeded");
    BridgeData result;
    size_t strings = 0, properties = 0;
    size_t payload_offset = 0;
    bool found_binary = false;
    for (auto const& value : rows.values()) {
        if (!value.is_array() || value.as_array().is_empty())
            return Error::from_string_literal("Invalid wire node");
        auto const& row = value.as_array();
        Node node {};
        node.kind = static_cast<Kind>(
            TRY(integer(row[0], static_cast<size_t>(binary ? Kind::DataView : Kind::Array))));
        if (node.kind == Kind::Function)
            return Error::from_string_literal("Function tokens cannot cross IPC");
        size_t arity = node.kind == Kind::Undefined || node.kind == Kind::Null ? 1
            : node.kind == Kind::Object || node.kind == Kind::Array            ? 3
            : node.kind == Kind::TypedArray                                    ? 5
            : node.kind == Kind::DataView                                      ? 4
                                                                               : 2;
        if (row.size() != arity)
            return Error::from_string_literal("Invalid wire node arity");
        switch (node.kind) {
        case Kind::Undefined:
        case Kind::Null:
            break;
        case Kind::Boolean:
            if (!row[1].is_bool())
                return Error::from_string_literal("Invalid wire boolean");
            node.number = row[1].as_bool();
            break;
        case Kind::Number: {
            if (!row[1].is_string() || row[1].as_string().bytes().size() != 16)
                return Error::from_string_literal("Invalid wire IEEE-754 number");
            u64 bits = 0;
            for (auto ch : row[1].as_string().bytes()) {
                if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
                    return Error::from_string_literal("Invalid wire number digit");
                bits = (bits << 4) | (ch <= '9' ? ch - '0' : ch - 'a' + 10);
            }

            if ((bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL
                && (bits & 0x000fffffffffffffULL) && bits != 0x7ff8000000000000ULL)
                return Error::from_string_literal("Noncanonical wire NaN");
            node.number = std::bit_cast<double>(bits);
            break;
        }

        case Kind::String:
            node.string = TRY(text(row[1], strings, limits.string_code_units));
            break;
        case Kind::ArrayBuffer: {
            found_binary = true;
            auto length = TRY(integer(row[1], payload.size() - payload_offset));
            node.bytes = TRY(ByteBuffer::copy(payload.slice(payload_offset, length)));
            payload_offset += length;
            break;
        }

        case Kind::TypedArray:
        case Kind::DataView: {
            size_t field = 1;
            if (node.kind == Kind::TypedArray)
                node.view_type = static_cast<u8>(TRY(integer(row[field++], 255)));
            node.buffer = TRY(integer(row[field++], rows.size() - 1));
            node.byte_offset = TRY(integer(row[field++], limits.wire_bytes));
            node.length = TRY(integer(row[field], limits.wire_bytes));
            break;
        }

        case Kind::Object:
        case Kind::Array: {
            if (node.kind == Kind::Array)
                node.length = TRY(integer(row[1], limits.array_length));
            else {
                if (!row[1].is_bool())
                    return Error::from_string_literal("Invalid wire prototype flag");
                node.null_prototype = row[1].as_bool();
            }

            if (!row[2].is_array() || row[2].as_array().size() > limits.properties - properties)
                return Error::from_string_literal("Wire property budget exceeded");
            properties += row[2].as_array().size();
            for (auto const& entry : row[2].as_array().values()) {
                if (!entry.is_array() || entry.as_array().size() != 3
                    || !entry.as_array()[2].is_bool())
                    return Error::from_string_literal("Invalid wire property tuple");
                auto const& fields = entry.as_array();
                auto key = TRY(text(fields[0], strings, limits.string_code_units));
                auto reference = TRY(integer(fields[1], rows.size() - 1));
                node.properties.append({ move(key), reference, fields[2].as_bool() });
            }

            break;
        }

        case Kind::Function:
            VERIFY_NOT_REACHED();
        }

        result.m_nodes.append(move(node));
    }

    if (payload_offset != payload.size() || (binary && !found_binary))
        return Error::from_string_literal("Unclaimed binary wire bytes/frame");
    TRY(result.validate_wire_graph(limits));
    return result;
}
}
