/**
 * Captures bounded data/binary graphs with no default hooks. Publication may supply an explicit
 * trusted getter reader. Materialization owns destination values, never source VM pointers.
 */

#pragma once
#include <AK/ByteBuffer.h>
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Function.h>
#include <AK/Utf16String.h>
#include <AK/Vector.h>
#include <LibJS/Runtime/Completion.h>

namespace Firefly {
// Internal graph snapshot with no VM pointers, descriptors or stored callbacks.
// Default capture accepts data only. The publication engine can opt into native
// function-registry tokens, which require its materializer. Wire encoding rejects
// those tokens and admits only the validated data graph.
class BridgeData {
public:
    struct Limits {
        size_t values { 4096 };
        size_t properties { 4096 };
        size_t string_code_units { 65536 };
        size_t depth { 64 };
        size_t array_length { 4096 };
        // Explicit opt-in. Public realm/process bridges use BinaryPolicy;
        // default data-only snapshots still refuse binary.
        size_t binary_bytes { 0 };
    };

    using CaptureFunction = Function<ErrorOr<size_t>(JS::FunctionObject&)>;
    // Native identity policy only: replace designated ordinary capability modules
    // with empty data nodes before inspecting any properties. Never JS callbacks.
    using RedactObject = Function<bool(JS::Object&)>;
    // FrozenContainers deliberately leaves copied binary stores/views writable;
    // engine TypedArrays cannot be frozen. Strict Frozen still refuses binary.
    // Only API publication supplies this trusted source-world reader. Default
    // capture, page arguments, IPC and callback/result snapshots never run it.
    using ReadAccessor = Function<ErrorOr<JS::Value>(JS::Object&, JS::FunctionObject&)>;
    enum class Materialization { Frozen, Mutable, FrozenContainers };
    using MaterializeFunction = Function<JS::ThrowCompletionOr<JS::Value>(size_t)>;
    // Native error replies must not enter or allocate in a busy JavaScript VM.
    static BridgeData from_string(Utf16View);

    static ErrorOr<BridgeData> capture(
        JS::Value, JS::Realm&, Limits, CaptureFunction = {}, RedactObject = {}, ReadAccessor = {});
    // Versioned, data-only wire graph; validates bytes/schema/budgets before JS allocation.
    ErrorOr<ByteString> encode_wire(Limits) const;

    static ErrorOr<BridgeData> decode_wire(StringView, Limits);

    JS::ThrowCompletionOr<JS::Value> materialize(
        JS::Realm&, MaterializeFunction = {}, Materialization = Materialization::Frozen) const;

private:
    BridgeData() = default;
    ErrorOr<void> validate_wire_graph(Limits) const;

    enum class Kind {
        Undefined = 0,
        Null = 1,
        Boolean = 2,
        Number = 3,
        String = 4,
        Object = 5,
        Array = 6,
        Function = 7,
        ArrayBuffer = 8,
        TypedArray = 9,
        DataView = 10
    };

    struct Property {
        Utf16String key;
        size_t value;
        bool enumerable;
    };

    struct Node {
        Kind kind { Kind::Undefined };
        double number { 0 };
        Utf16String string;
        ByteBuffer bytes;
        Vector<Property> properties;
        size_t length { 0 };
        size_t buffer { 0 };
        size_t byte_offset { 0 };
        u8 view_type { 0 };
        bool null_prototype { false };
    };

    Vector<Node> m_nodes;
};
}
