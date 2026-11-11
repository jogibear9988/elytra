/** Read native slots without property hooks, with separate raw-frame and graph byte budgets. */
#include "WireValue.h"
#include "native/ipc/application/Message.h"
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/TypedArray.h>

namespace Firefly {
JS::ThrowCompletionOr<JS::Value> wire_value(JS::Realm& realm, ByteString const& wire)
{
    GC::Root buffer { JS::ArrayBuffer::create(realm, MUST(ByteBuffer::copy(wire.bytes()))) };
    return JS::Uint8Array::create(realm, wire.length(), *buffer).ptr();
}

ErrorOr<ByteString> wire_bytes(JS::Value value, JS::Realm& realm, WireValuePolicy policy)
{
    if (policy != WireValuePolicy::SingleFrame && policy != WireValuePolicy::ApplicationMessage)
        return Error::from_string_literal("Unknown IPC wire byte policy");
    auto byte_limit = policy == WireValuePolicy::ApplicationMessage
        ? ApplicationIpc::logical_wire_limit
        : ApplicationIpc::wire_limit;
    if (!value.is_object() || !JS::TypedArrayBase::is_engine_class_of(value.as_object())
        || &value.as_object().shape().realm() != &realm || JS::host_class_of(value.as_object()))
        return Error::from_string_literal("IPC wire requires a native owner byte view");
    auto& view = static_cast<JS::TypedArrayBase&>(value.as_object());
    auto length = view.byte_length();
    auto* buffer = view.viewed_array_buffer();
    if (view.kind() != JS::TypedArrayBase::Kind::Uint8Array || length.is_auto()
        || length.is_detached() || length.length() > byte_limit || !buffer || buffer->is_detached()
        || buffer->is_shared_array_buffer() || !buffer->is_fixed_length()
        || view.byte_offset() > buffer->byte_length()
        || length.length() > buffer->byte_length() - view.byte_offset())
        return Error::from_string_literal("Invalid IPC wire byte storage");
    auto bytes = TRY(buffer->copy_to_byte_buffer(view.byte_offset(), length.length()));
    return ByteString(StringView(reinterpret_cast<char const*>(bytes.data()), bytes.size()));
}
}
