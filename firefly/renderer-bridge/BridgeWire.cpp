#include "BridgeData.h"
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
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559);
constexpr size_t max_wire_bytes = 1024 * 1024;
constexpr size_t max_total_array_length = 16384;
ErrorOr<void> check_limits(BridgeData::Limits limits)
{
    BridgeData::Limits maximum;
    if (limits.values > maximum.values || limits.properties > maximum.properties
        || limits.string_code_units > maximum.string_code_units || limits.depth > maximum.depth
        || limits.array_length > maximum.array_length)
        return Error::from_string_literal("Wire limits exceed hard maximum");
    return {};
}
ErrorOr<size_t> integer(JsonValue const& input, size_t maximum)
{
    auto number = input.get_double_with_precision_loss();
    if (!number.has_value() || !std::isfinite(*number) || *number < 0 || *number > maximum || std::floor(*number) != *number)
        return Error::from_string_literal("Invalid wire integer");
    return static_cast<size_t>(*number);
}
ErrorOr<JsonArray> units(Utf16String const& text)
{
    JsonArray result;
    auto view = text.utf16_view();
    for (size_t i = 0; i < view.length_in_code_units(); ++i) TRY(result.append(static_cast<u32>(view.code_unit_at(i))));
    return result;
}
ErrorOr<Utf16String> text(JsonValue const& value, size_t& used, size_t maximum)
{
    if (!value.is_array() || value.as_array().size() > maximum - used)
        return Error::from_string_literal("Wire UTF-16 budget exceeded");
    used += value.as_array().size();
    Utf16StringBuilder result;
    for (auto const& unit : value.as_array().values()) result.append_code_unit(static_cast<char16_t>(TRY(integer(unit, 65535))));
    return result.to_string();
}
bool is_array_index_out_of_bounds(Utf16String const& key, size_t length)
{
    auto view = key.utf16_view();
    if (view.is_empty() || (view.length_in_code_units() > 1 && view.code_unit_at(0) == '0')) return false;
    u64 number = 0;
    for (size_t i = 0; i < view.length_in_code_units(); ++i) {
        auto unit = view.code_unit_at(i);
        if (unit < '0' || unit > '9') return false;
        number = number * 10 + unit - '0';
        if (number >= 0xffffffffULL) return false; // Not an ECMAScript array index.
    }
    return number >= length;
}
ErrorOr<void> preflight(StringView bytes)
{
    if (bytes.is_empty() || bytes.length() > max_wire_bytes)
        return Error::from_string_literal("Wire byte budget exceeded");
    size_t depth = 0;
    bool quoted = false, escaped = false;
    for (char ch : bytes) {
        if (quoted) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') quoted = false;
        } else if (ch == '"') quoted = true;
        else if (ch == '[') {
            if (++depth > 8) return Error::from_string_literal("Wire JSON nesting budget exceeded");
        } else if (ch == ']') {
            if (!depth) return Error::from_string_literal("Invalid wire JSON nesting");
            --depth;
        } else if (ch == '{' || ch == '}') return Error::from_string_literal("Wire objects are not permitted");
    }
    if (depth || quoted) return Error::from_string_literal("Unterminated wire JSON");
    return {};
}
}

ErrorOr<void> BridgeData::validate_wire_graph(Limits limits) const
{
    TRY(check_limits(limits));
    if (m_nodes.is_empty() || m_nodes.size() > limits.values)
        return Error::from_string_literal("Wire node budget exceeded");
    size_t properties = 0, strings = 0, array_length = 0;
    auto charge = [](size_t& used, size_t amount, size_t maximum) -> ErrorOr<void> {
        if (amount > maximum - used) return Error::from_string_literal("Wire graph budget exceeded");
        used += amount; return {};
    };
    for (auto const& node : m_nodes) {
        if (node.kind == Kind::Function) return Error::from_string_literal("Function tokens cannot cross IPC");
        TRY(charge(strings, node.string.length_in_code_units(), limits.string_code_units));
        TRY(charge(properties, node.properties.size(), limits.properties));
        if (node.kind == Kind::Array) {
            if (node.length > limits.array_length) return Error::from_string_literal("Wire array length exceeded");
            TRY(charge(array_length, node.length, max_total_array_length));
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
            if (node.kind == Kind::Array && (property.key == "length"_utf16 || is_array_index_out_of_bounds(property.key, node.length)))
                return Error::from_string_literal("Wire array property exceeds length");
        }
    }
    HashTable<size_t> seen;
    auto visit = [&](auto& self, size_t index, size_t depth) -> ErrorOr<void> {
        if (depth > limits.depth) return Error::from_string_literal("Wire graph depth exceeded");
        if (seen.contains(index)) return {};
        seen.set(index);
        for (auto const& property : m_nodes[index].properties) TRY(self(self, property.value, depth + 1));
        return {};
    };
    TRY(visit(visit, 0, 0));
    if (seen.size() != m_nodes.size()) return Error::from_string_literal("Unreachable wire nodes");
    return {};
}

ErrorOr<ByteString> BridgeData::encode_wire(Limits limits) const
{
    TRY(validate_wire_graph(limits));
    JsonArray nodes;
    for (auto const& node : m_nodes) {
        JsonArray row; TRY(row.append(static_cast<u32>(node.kind)));
        switch (node.kind) {
        case Kind::Undefined: case Kind::Null: break;
        case Kind::Boolean: TRY(row.append(node.number != 0)); break;
        case Kind::Number: {
            auto bits = std::bit_cast<u64>(node.number);
            if ((bits & 0x7ff0000000000000ULL) == 0x7ff0000000000000ULL && (bits & 0x000fffffffffffffULL))
                bits = 0x7ff8000000000000ULL; // Canonical JS NaN; never encode engine tags.
            char hex[16];
            for (size_t i=0;i<16;++i) {hex[15-i]="0123456789abcdef"[bits & 15];bits >>= 4;}
            TRY(row.append(StringView(hex,16)));break;
        }
        case Kind::String: TRY(row.append(TRY(units(node.string)))); break;
        case Kind::Object: case Kind::Array: {
            if (node.kind == Kind::Array) TRY(row.append(node.length));
            else TRY(row.append(node.null_prototype));
            JsonArray properties;
            for (auto const& property : node.properties) {
                JsonArray entry;
                TRY(entry.append(TRY(units(property.key))));TRY(entry.append(property.value));TRY(entry.append(property.enumerable));
                TRY(properties.append(move(entry)));
            }
            TRY(row.append(move(properties)));break;
        }
        case Kind::Function: VERIFY_NOT_REACHED();
        }
        TRY(nodes.append(move(row)));
    }
    JsonArray envelope;TRY(envelope.append(1));TRY(envelope.append(move(nodes)));
    auto result = JsonValue(move(envelope)).serialized().to_byte_string();
    TRY(preflight(result));
    return result;
}

ErrorOr<BridgeData> BridgeData::decode_wire(StringView bytes, Limits limits)
{
    TRY(check_limits(limits));
    TRY(preflight(bytes)); // Bound bytes/JSON recursion before allocating the parser tree.
    auto parsed = TRY(JsonValue::from_string(bytes));
    if (!parsed.is_array() || parsed.as_array().size() != 2 || TRY(integer(parsed.as_array()[0], 1)) != 1
        || !parsed.as_array()[1].is_array()) return Error::from_string_literal("Invalid wire envelope");
    auto const& rows = parsed.as_array()[1].as_array();
    if (rows.is_empty() || rows.size() > limits.values) return Error::from_string_literal("Wire node budget exceeded");
    BridgeData result;
    size_t strings = 0, properties = 0;
    for (auto const& value : rows.values()) {
        if (!value.is_array() || value.as_array().is_empty()) return Error::from_string_literal("Invalid wire node");
        auto const& row = value.as_array();
        Node node {};
        node.kind = static_cast<Kind>(TRY(integer(row[0], static_cast<size_t>(Kind::Array))));
        size_t arity = node.kind == Kind::Undefined || node.kind == Kind::Null ? 1 : node.kind == Kind::Object || node.kind == Kind::Array ? 3 : 2;
        if (row.size() != arity) return Error::from_string_literal("Invalid wire node arity");
        switch (node.kind) {
        case Kind::Undefined: case Kind::Null: break;
        case Kind::Boolean:
            if (!row[1].is_bool()) return Error::from_string_literal("Invalid wire boolean");
            node.number = row[1].as_bool();break;
        case Kind::Number: {
            if (!row[1].is_string() || row[1].as_string().bytes().size()!=16)
                return Error::from_string_literal("Invalid wire IEEE-754 number");
            u64 bits=0;
            for (auto ch : row[1].as_string().bytes()) {
                if (!((ch>='0' && ch<='9') || (ch>='a' && ch<='f')))
                    return Error::from_string_literal("Invalid wire number digit");
                bits=(bits<<4) | (ch<='9' ? ch-'0' : ch-'a'+10);
            }
            if ((bits & 0x7ff0000000000000ULL)==0x7ff0000000000000ULL && (bits & 0x000fffffffffffffULL)
                && bits!=0x7ff8000000000000ULL) return Error::from_string_literal("Noncanonical wire NaN");
            node.number=std::bit_cast<double>(bits);break;
        }
        case Kind::String: node.string = TRY(text(row[1], strings, limits.string_code_units));break;
        case Kind::Object: case Kind::Array: {
            if (node.kind == Kind::Array) node.length = TRY(integer(row[1], limits.array_length));
            else {
                if (!row[1].is_bool()) return Error::from_string_literal("Invalid wire prototype flag");
                node.null_prototype = row[1].as_bool();
            }
            if (!row[2].is_array() || row[2].as_array().size() > limits.properties - properties)
                return Error::from_string_literal("Wire property budget exceeded");
            properties += row[2].as_array().size();
            for (auto const& entry : row[2].as_array().values()) {
                if (!entry.is_array() || entry.as_array().size() != 3 || !entry.as_array()[2].is_bool())
                    return Error::from_string_literal("Invalid wire property tuple");
                auto const& fields = entry.as_array();
                auto key = TRY(text(fields[0], strings, limits.string_code_units));
                auto reference = TRY(integer(fields[1], rows.size() - 1));
                node.properties.append({move(key), reference, fields[2].as_bool()});
            }
            break;
        }
        case Kind::Function: VERIFY_NOT_REACHED();
        }
        result.m_nodes.append(move(node));
    }
    TRY(result.validate_wire_graph(limits));
    return result;
}
}
