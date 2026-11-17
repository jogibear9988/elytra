/** Read genuine byte-source slots without invoking author getters or Proxy traps. */
#pragma once
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/VM.h>
#include <cstring>

namespace Mite {
struct NativeByteSource {
    JS::ArrayBuffer* buffer;
    size_t offset;
    size_t length;
    size_t element_size;
};

template <typename Error = JS::TypeError>
inline JS::Completion byte_source_error(JS::VM& vm, char const* code, Utf16View message)
{
    auto error = Error::create(vm.realm(), message);

    error->define_direct_property("code"_utf16,
        JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(code, std::strlen(code)))),
        JS::default_attributes);

    return JS::throw_completion(error);
}

inline JS::ThrowCompletionOr<NativeByteSource> native_byte_source(
    JS::VM& vm, JS::Value value, bool allow_data_view)
{
    if (!value.is_object())
        return byte_source_error(
            vm, "ERR_INVALID_ARG_TYPE", u"Expected an ArrayBuffer or TypedArray"sv);

    auto& object = value.as_object();

    JS::ArrayBuffer* buffer = nullptr;
    size_t offset = 0, length = 0, element_size = 1;
    if (JS::ArrayBuffer::is_engine_class_of(object)) {
        buffer = &static_cast<JS::ArrayBuffer&>(object);

        length = buffer->byte_length();
    } else if (JS::TypedArrayBase::is_engine_class_of(object)) {
        auto& array = static_cast<JS::TypedArrayBase&>(object);

        buffer = array.viewed_array_buffer();

        offset = array.byte_offset();

        element_size = array.element_size();

        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);

        if (!JS::is_typed_array_out_of_bounds(witness))
            length = JS::typed_array_byte_length(witness);
    } else if (allow_data_view && JS::DataView::is_engine_class_of(object)) {
        auto& view = static_cast<JS::DataView&>(object);

        buffer = view.viewed_array_buffer();

        offset = view.byte_offset();

        auto witness
            = JS::make_data_view_with_buffer_witness_record(view, JS::ArrayBuffer::Order::SeqCst);

        if (!JS::is_view_out_of_bounds(witness))
            length = JS::get_view_byte_length(witness);
    } else {
        return byte_source_error(
            vm, "ERR_INVALID_ARG_TYPE", u"Expected an ArrayBuffer or TypedArray"sv);
    }

    if (!buffer || buffer->is_detached() || buffer->is_shared_array_buffer()
        || !buffer->is_fixed_length())
        return byte_source_error<JS::Error>(vm, "ERR_MITE_UNSUPPORTED_API",
            u"Detached, shared or resizable byte storage is unsupported"sv);

    if (offset > buffer->byte_length() || length > buffer->byte_length() - offset)
        return byte_source_error<JS::RangeError>(
            vm, "ERR_OUT_OF_RANGE", u"Byte source is outside its backing storage"sv);

    return NativeByteSource { buffer, offset, length, element_size };
}
}
