#pragma once
#include <AK/Error.h>
#include <AK/Utf16String.h>
#include <AK/Vector.h>
#include <LibJS/Runtime/Completion.h>

namespace Firefly {
// Internal data-only snapshot. Contains no VM pointers, descriptors or callbacks.
// This is not an IPC wire decoder or the public contextBridge implementation.
class BridgeData {
public:
    struct Limits {
        size_t values { 4096 };
        size_t properties { 4096 };
        size_t string_code_units { 65536 };
        size_t depth { 64 };
        size_t array_length { 4096 };
    };
    static ErrorOr<BridgeData> capture(JS::Value, JS::Realm&, Limits);
    JS::ThrowCompletionOr<JS::Value> materialize(JS::Realm&) const;
private:
    BridgeData() = default;
    enum class Kind { Undefined, Null, Boolean, Number, String, Object, Array };
    struct Property {
        Utf16String key;
        size_t value;
        bool enumerable;
    };
    struct Node {
        Kind kind { Kind::Undefined };
        double number { 0 };
        Utf16String string;
        Vector<Property> properties;
        size_t length { 0 };
        bool null_prototype { false };
    };
    Vector<Node> m_nodes;
};
}
