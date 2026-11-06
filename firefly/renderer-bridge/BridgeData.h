#pragma once
#include <AK/Function.h>
#include <AK/Error.h>
#include <AK/Utf16String.h>
#include <AK/Vector.h>
#include <LibJS/Runtime/Completion.h>

namespace Firefly {
// Internal graph snapshot with no VM pointers, descriptors or stored callbacks.
// Default capture accepts data only. The publication engine can opt into native
// function-registry tokens, which require its materializer. This is not IPC wire.
class BridgeData {
public:
    struct Limits {
        size_t values { 4096 };
        size_t properties { 4096 };
        size_t string_code_units { 65536 };
        size_t depth { 64 };
        size_t array_length { 4096 };
    };
    using CaptureFunction = Function<ErrorOr<size_t>(JS::FunctionObject&)>;
    using MaterializeFunction = Function<JS::ThrowCompletionOr<JS::Value>(size_t)>;
    static ErrorOr<BridgeData> capture(JS::Value, JS::Realm&, Limits, CaptureFunction = {});
    JS::ThrowCompletionOr<JS::Value> materialize(JS::Realm&, MaterializeFunction = {}) const;
private:
    BridgeData() = default;
    enum class Kind { Undefined, Null, Boolean, Number, String, Object, Array, Function };
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
