/** Bounded native zlib work shared by Node-compatible Transform wrappers. */
#include "platform/compression/Codec.h"
#include "core/GCCaptures.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <vector>
#include <zlib.h>

namespace Mite {
namespace {
    constexpr size_t limit = 1024 * 1024;

    JS::Completion failure(JS::VM& vm, char const* code, char const* message)
    {
        auto error = JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message))));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    struct Codec {
        GC_HOLDS_NO_CELL_EDGES;

        z_stream stream {};
        bool compress { false };
        bool gzip { false };
        bool ended { false };
        bool padding { false };
        bool active { false };
        size_t chunk_size { 16384 };

        ~Codec()
        {
            close();
        }

        void close()
        {
            if (active) {
                if (compress)
                    deflateEnd(&stream);
                else
                    inflateEnd(&stream);
            }

            active = false;
        }
    };

    JS::ThrowCompletionOr<int> integer(JS::VM& vm, size_t index, int minimum, int maximum)
    {
        auto value = vm.argument(index);
        if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < minimum
            || value.as_double() > maximum
            || value.as_double() != static_cast<int>(value.as_double()))
            return failure(vm, "ERR_OUT_OF_RANGE", "Invalid native zlib integer");
        return static_cast<int>(value.as_double());
    }

    JS::ThrowCompletionOr<JS::Value> step(JS::VM& vm, Codec& state)
    {
        if (!state.active)
            return failure(vm, "ERR_INVALID_STATE", "Zlib handle is closed");
        auto flush = TRY(integer(vm, 1, 0, 4));
        if (flush == Z_PARTIAL_FLUSH)
            return failure(vm, "ERR_MITE_UNSUPPORTED_API", "Z_PARTIAL_FLUSH is not implemented");
        auto value = vm.argument(0);
        if (!value.is_object() || !JS::Uint8Array::is_engine_class_of(value.as_object()))
            return vm.throw_completion<JS::TypeError>("Invalid native zlib bytes"_utf16);
        auto& array = static_cast<JS::Uint8Array&>(value.as_object());
        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);
        if (JS::is_typed_array_out_of_bounds(witness))
            return vm.throw_completion<JS::TypeError>("Detached zlib input"_utf16);
        auto size = JS::typed_array_byte_length(witness);
        if (size > limit)
            return failure(vm, "ERR_OUT_OF_RANGE", "Zlib native step input exceeds one MiB");
        auto bytes = array.viewed_array_buffer()->with_readonly_bytes(
            array.byte_offset(), size, [](ReadonlyBytes view) {
                return std::vector<unsigned char>(view.begin(), view.end());
            });
        size_t consumed = 0;
        size_t produced = 0;
        std::vector<unsigned char> output(state.chunk_size);
        if (state.ended && !state.compress && state.gzip && !bytes.empty()) {
            if (bytes[0] == 0) {
                if (!std::all_of(bytes.begin(), bytes.end(), [](auto byte) { return byte == 0; }))
                    return failure(vm, "Z_DATA_ERROR", "Invalid gzip trailing data");
                state.padding = true;
                consumed = bytes.size();
            } else {
                if (state.padding || inflateReset(&state.stream) != Z_OK)
                    return failure(vm, "Z_DATA_ERROR", "Invalid gzip member");
                state.ended = false;
            }
        }

        if (!state.ended) {
            state.stream.next_in = bytes.empty() ? Z_NULL : bytes.data();
            state.stream.avail_in = static_cast<uInt>(bytes.size());
            state.stream.next_out = output.data();
            state.stream.avail_out = static_cast<uInt>(output.size());
            auto status
                = state.compress ? deflate(&state.stream, flush) : inflate(&state.stream, flush);
            consumed = bytes.size() - state.stream.avail_in;
            produced = output.size() - state.stream.avail_out;
            state.stream.next_in = state.stream.next_out = Z_NULL;
            state.stream.avail_in = state.stream.avail_out = 0;
            if (status == Z_STREAM_END)
                state.ended = true;
            else if (status == Z_NEED_DICT)
                return failure(vm, "Z_NEED_DICT", "Zlib dictionary is required");
            else if (status != Z_OK && status != Z_BUF_ERROR)
                return failure(vm, "Z_DATA_ERROR", "Invalid compressed stream or checksum");
            if (flush == Z_FINISH && !state.ended && produced < output.size())
                return failure(vm, "Z_BUF_ERROR", "Unexpected end of compressed stream");
        } else if (!state.gzip && !state.compress)
            consumed = bytes.size();
        else if (state.compress && !bytes.empty())
            return failure(vm, "Z_STREAM_ERROR", "Compression stream has ended");
        GC::Root result { JS::Object::create(vm.realm(), nullptr) };
        GC::Root data { MUST(JS::Uint8Array::create(vm.realm(), produced)) };
        if (produced)
            data->viewed_array_buffer()->overwrite(0, output.data(), produced);
        result->define_direct_property("data"_utf16, data.ptr(), JS::default_attributes);
        result->define_direct_property(
            "consumed"_utf16, JS::Value(static_cast<double>(consumed)), JS::default_attributes);
        result->define_direct_property("again"_utf16,
            JS::Value(consumed < bytes.size() || (produced == output.size() && !state.ended)),
            JS::default_attributes);
        return JS::Value(result.ptr());
    }
}

GC::Ref<JS::Object> create_zlib_host(JS::Realm& realm)
{
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_native_function(
        realm, "create"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto mode = TRY(integer(vm, 0, 0, 5));
            auto window = TRY(integer(vm, 1, 8, 15));
            auto level = TRY(integer(vm, 2, -1, 9));
            auto memory = TRY(integer(vm, 3, 1, 9));
            auto strategy = TRY(integer(vm, 4, 0, 4));
            auto chunk = TRY(integer(vm, 5, 64, limit));
            auto state = std::make_shared<Codec>();
            state->compress = mode % 2 == 0;
            state->gzip = mode >= 4;
            state->chunk_size = static_cast<size_t>(chunk);
            int bits = mode < 2 ? -window : mode >= 4 ? window + 16 : window;
            int status = state->compress
                ? deflateInit2(&state->stream, level, Z_DEFLATED, bits, memory, strategy)
                : inflateInit2(&state->stream, bits);
            if (status != Z_OK)
                return failure(vm, "Z_MEM_ERROR", "Could not initialize zlib state");
            state->active = true;
            GC::Root object { JS::Object::create(vm.realm(), nullptr) };
            object->define_native_function(vm.realm(), "step"_utf16,
                GC::create_function(vm.realm().vm().heap(), GC::captures(shared_capture(state)),
                    [](auto& state, JS::VM& vm) { return step(vm, *state); }),
                2, JS::default_attributes);
            object->define_native_function(vm.realm(), "close"_utf16,
                GC::create_function(vm.realm().vm().heap(), GC::captures(shared_capture(state)),
                    [](auto& state, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                        state->close();
                        return JS::js_undefined();
                    }),
                0, JS::default_attributes);
            return JS::Value(object.ptr());
        },
        6, JS::default_attributes);
    return *host;
}
}
