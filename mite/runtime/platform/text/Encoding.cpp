/**
 * Converts byte ranges and text without assuming typed-array element values. Decoder state is
 * owned per instance; shared buffers use native snapshots and VM-facing results remain
 * realm-local.
 */

#include "platform/text/Encoding.h"
#include <AK/UnicodeUtils.h>
#include <AK/Utf16StringBuilder.h>
#include <LibGC/CellAllocator.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibTextCodec/Decoder.h>
#include <LibTextCodec/RustFFI.h>
#include <cstring>
#include <memory>
#include <unicode/ucnv.h>
#include <unicode/ucnv_cb.h>
#include <unicode/ucnv_err.h>

namespace Mite {
static void U_EXPORT2 substitute_unicode(void const*, UConverterToUnicodeArgs* args, char const*,
    int32_t, UConverterCallbackReason reason, UErrorCode* status)
{
    if (reason > UCNV_IRREGULAR)
        return;
    *status = U_ZERO_ERROR;
    UChar replacement = 0xfffd;
    ucnv_cbToUWriteUChars(args, &replacement, 1, 0, status);
}

class DecoderState final : public GC::Cell {
    GC_CELL(DecoderState, GC::Cell);
    GC_DECLARE_ALLOCATOR(DecoderState);

public:
    DecoderState(StringView encoding, bool fatal, bool ignore_bom)
    {
        // Node's ICU mappings differ from the WHATWG tables for these codecs.
        // Keep that compatibility policy in our adapter, outside Ladybird.
        if (encoding.equals_ignoring_ascii_case("big5"sv)
            || encoding.equals_ignoring_ascii_case("euc-jp"sv)
            || encoding.equals_ignoring_ascii_case("iso-2022-jp"sv)
            || encoding.equals_ignoring_ascii_case("shift_jis"sv)
            || encoding.equals_ignoring_ascii_case("euc-kr"sv)) {
            UErrorCode status = U_ZERO_ERROR;
            auto label = ByteString(encoding);
            m_icu = ucnv_open(label.characters(), &status);
            if (U_SUCCESS(status))
                ucnv_setToUCallBack(m_icu, fatal ? UCNV_TO_U_CALLBACK_STOP : substitute_unicode,
                    nullptr, nullptr, nullptr, &status);
            if (U_FAILURE(status)) {
                if (m_icu)
                    ucnv_close(m_icu);
                m_icu = nullptr;
            }
        } else
            m_standard = std::make_unique<TextCodec::StreamingDecoder>(encoding,
                ignore_bom ? TextCodec::IgnoreBOM::Yes : TextCodec::IgnoreBOM::No,
                fatal ? TextCodec::ErrorMode::Fatal : TextCodec::ErrorMode::Replacement);
    }

    ~DecoderState() override
    {
        if (m_icu)
            ucnv_close(m_icu);
    }

    bool valid() const
    {
        return m_icu || m_standard;
    }

    ErrorOr<Utf16String> decode(ReadonlyBytes bytes, TextCodec::Flush flush)
    {
        if (m_standard)
            return m_standard->decode(bytes, flush);
        Utf16StringBuilder builder;
        char const empty = 0;
        char const* source
            = bytes.is_empty() ? &empty : reinterpret_cast<char const*>(bytes.data());
        auto const* end = source + bytes.size();
        UErrorCode status;
        do {
            UChar output[1024];
            UChar* target = output;
            status = U_ZERO_ERROR;
            ucnv_toUnicode(m_icu, &target, output + 1024, &source, end, nullptr,
                flush == TextCodec::Flush::Yes, &status);
            builder.append(Utf16View(
                reinterpret_cast<char16_t const*>(output), static_cast<size_t>(target - output)));
        } while (status == U_BUFFER_OVERFLOW_ERROR);
        if (flush == TextCodec::Flush::Yes)
            ucnv_resetToUnicode(m_icu);
        if (U_FAILURE(status))
            return Error::from_string_literal("Failed to decode input");
        return builder.to_string();
    }

private:
    std::unique_ptr<TextCodec::StreamingDecoder> m_standard;
    UConverter* m_icu { nullptr };
};

GC_DEFINE_ALLOCATOR(DecoderState);

namespace {
    static JS::Completion invalid_input(JS::VM& vm)
    {
        auto error = JS::TypeError::create(
            vm.realm(), "Input must be an ArrayBuffer or an ArrayBufferView"_utf16);
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, "ERR_INVALID_ARG_TYPE"_utf16), JS::default_attributes);
        return JS::throw_completion(error);
    }

    struct BufferSource {
        JS::ArrayBuffer* buffer { nullptr };
        size_t offset {}, length {};
    };

    static JS::ThrowCompletionOr<BufferSource> buffer_source(JS::VM& vm, JS::Value value)
    {
        if (value.is_undefined())
            return BufferSource {};
        if (!value.is_object())
            return invalid_input(vm);
        auto& object = value.as_object();
        if (JS::ArrayBuffer::is_engine_class_of(object)) {
            auto& buffer = static_cast<JS::ArrayBuffer&>(object);
            return BufferSource { &buffer, 0, buffer.byte_length() };
        }

        if (JS::TypedArrayBase::is_engine_class_of(object)) {
            auto& array = static_cast<JS::TypedArrayBase&>(object);
            auto witness = JS::make_typed_array_with_buffer_witness_record(
                array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness))
                return BufferSource {};
            return BufferSource { array.viewed_array_buffer(), array.byte_offset(),
                JS::typed_array_byte_length(witness) };
        }

        if (JS::DataView::is_engine_class_of(object)) {
            auto& view = static_cast<JS::DataView&>(object);
            auto witness = JS::make_data_view_with_buffer_witness_record(
                view, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_view_out_of_bounds(witness))
                return BufferSource {};
            return BufferSource { view.viewed_array_buffer(), view.byte_offset(),
                JS::get_view_byte_length(witness) };
        }

        return invalid_input(vm);
    }
}

GC::Ref<JS::Object> create_encoding_host(JS::Realm& realm)
{
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(
        realm, "byteView"_utf16,
        [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto input = TRY(buffer_source(vm, vm.argument(0)));
            if (!input.length)
                return JS::Value(TRY(JS::Uint8Array::create(realm, 0)));
            return JS::Value(JS::TypedArrayBase::create_from_slots(realm,
                JS::TypedArrayBase::Kind::Uint8Array, *input.buffer,
                JS::ByteLength { input.length }, JS::ByteLength { input.length }, input.offset));
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "standardize"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto label = TRY(vm.argument(0).to_utf16_string(vm));
            auto encoded = MUST(label.utf16_view().to_utf8());
            auto bytes = encoded.bytes_as_string_view();
            u8 const* canonical = nullptr;
            size_t length = 0;
            // The public C++ label helper logs invalid labels. Its underlying FFI
            // provides the same canonicalization without writing application stderr.
            if (!TextCodec::FFI::textcodec_rust_get_standardized_encoding(
                    bytes.bytes().data(), bytes.length(), &canonical, &length))
                return JS::js_null();
            return JS::PrimitiveString::create(
                vm, Utf16String::from_utf8({ reinterpret_cast<char const*>(canonical), length }));
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "createDecoder"_utf16,
        [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(vm.argument(0).to_utf16_string(vm));
            auto encoded = MUST(name.utf16_view().to_utf8());
            auto canonical = TextCodec::get_standardized_encoding(encoded);
            VERIFY(canonical.has_value());
            auto state = vm.heap().allocate<DecoderState>(
                *canonical, vm.argument(1).to_boolean(), vm.argument(2).to_boolean());
            if (!state->valid())
                return vm.throw_completion<JS::TypeError>("Native decoder is unavailable"_utf16);
            return JS::NativeFunction::create(
                realm,
                [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                    auto input = TRY(buffer_source(vm, vm.argument(0)));
                    auto flush = vm.argument(1).to_boolean() ? TextCodec::Flush::No
                                                             : TextCodec::Flush::Yes;
                    auto output = input.length
                        ? input.buffer->with_readonly_bytes(input.offset, input.length,
                              [&](ReadonlyBytes bytes) { return state->decode(bytes, flush); })
                        : state->decode({}, flush);
                    if (output.is_error()) {
                        auto error = JS::TypeError::create(
                            vm.realm(), "The encoded data was not valid"_utf16);
                        error->define_direct_property("code"_utf16,
                            JS::PrimitiveString::create(
                                vm, "ERR_ENCODING_INVALID_ENCODED_DATA"_utf16),
                            JS::default_attributes);
                        return JS::throw_completion(error);
                    }

                    return JS::PrimitiveString::create(vm, output.release_value());
                },
                2, "decodeText"_utf16);
        },
        3, JS::default_attributes);
    host->define_native_function(
        realm, "encode"_utf16,
        [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto input = TRY(vm.argument(0).to_utf16_string(vm));
            auto bytes = MUST(input.utf16_view().to_utf8());
            auto array = TRY(JS::Uint8Array::create(realm, bytes.bytes_as_string_view().length()));
            if (!bytes.is_empty())
                array->viewed_array_buffer()->overwrite(0,
                    bytes.bytes_as_string_view().characters_without_null_termination(),
                    bytes.bytes_as_string_view().length());
            return JS::Value(array);
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "encodeInto"_utf16,
        [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto input = TRY(vm.argument(0).to_utf16_string(vm));
            if (!vm.argument(1).is_object()
                || !JS::Uint8Array::is_engine_class_of(vm.argument(1).as_object()))
                return invalid_input(vm);
            auto destination = TRY(buffer_source(vm, vm.argument(1)));
            size_t read = 0, written = 0;
            auto string = input.utf16_view();
            auto* target
                = destination.length ? destination.buffer->data_at(destination.offset) : nullptr;
            while (read < string.length_in_code_units()) {
                u32 point = string.code_unit_at(read);
                size_t units = 1;
                if (point >= 0xd800 && point <= 0xdbff) {
                    if (read + 1 < string.length_in_code_units()) {
                        u32 next = string.code_unit_at(read + 1);
                        if (next >= 0xdc00 && next <= 0xdfff) {
                            point = 0x10000 + ((point - 0xd800) << 10) + next - 0xdc00;
                            units = 2;
                        } else
                            point = 0xfffd;
                    } else
                        point = 0xfffd;
                } else if (point >= 0xdc00 && point <= 0xdfff)
                    point = 0xfffd;
                u8 bytes[4];
                size_t count = 0;
                (void)AK::UnicodeUtils::code_point_to_utf8(
                    point, [&](char value) { bytes[count++] = static_cast<u8>(value); });
                if (count > destination.length - written)
                    break;
                memcpy(target + written, bytes, count);
                read += units;
                written += count;
            }

            auto result = JS::Object::create(realm, realm.intrinsics().object_prototype());
            result->define_direct_property(
                "read"_utf16, JS::Value(static_cast<double>(read)), JS::default_attributes);
            result->define_direct_property(
                "written"_utf16, JS::Value(static_cast<double>(written)), JS::default_attributes);
            return JS::Value(result);
        },
        2, JS::default_attributes);
    return host;
}
}
