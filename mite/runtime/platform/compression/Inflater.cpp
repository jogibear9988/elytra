/** Own zlib state without retaining JS byte pointers or allocating unbounded output. */
#include "platform/compression/Inflater.h"
#include "core/GCCaptures.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <vector>
#include <zlib.h>

namespace Mite {
namespace {
    constexpr size_t input_limit = 1024 * 1024;
    constexpr size_t output_limit = 65536;

    JS::Completion failure(JS::VM& vm, char const* code, char const* message)
    {
        auto error = JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message))));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    struct Inflater {
        GC_HOLDS_NO_CELL_EDGES;

        z_stream stream {};
        int window { 0 };
        bool active { false };
        bool closed { false };
        bool member_ended { false };
        bool padding { false };
        bool allow_padding { false };

        ~Inflater()
        {
            close();
        }

        void close()
        {
            if (active)
                inflateEnd(&stream);
            active = false;
            closed = true;
        }
    };

    JS::ThrowCompletionOr<std::vector<unsigned char>> input(JS::VM& vm)
    {
        if (!vm.argument(0).is_object()
            || !JS::Uint8Array::is_engine_class_of(vm.argument(0).as_object()))
            return vm.throw_completion<JS::TypeError>("Invalid internal inflate bytes"_utf16);
        auto& array = static_cast<JS::Uint8Array&>(vm.argument(0).as_object());
        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);
        if (JS::is_typed_array_out_of_bounds(witness))
            return vm.throw_completion<JS::TypeError>("Detached inflate input"_utf16);
        auto length = JS::typed_array_byte_length(witness);
        if (length > input_limit)
            return failure(vm, "ERR_OUT_OF_RANGE", "Inflate step exceeds one MiB input");
        return array.viewed_array_buffer()->with_readonly_bytes(
            array.byte_offset(), length, [](ReadonlyBytes bytes) {
                return std::vector<unsigned char>(bytes.begin(), bytes.end());
            });
    }

    JS::ThrowCompletionOr<JS::Value> step(JS::VM& vm, Inflater& state)
    {
        if (state.closed)
            return failure(vm, "ERR_INVALID_STATE", "Inflater is closed");
        auto bytes = TRY(input(vm));
        bool final = vm.argument(1).to_boolean();
        bool done = false;
        size_t consumed = 0;
        size_t produced = 0;
        std::array<unsigned char, output_limit> output;
        if (state.member_ended) {
            if (bytes.empty())
                done = final;
            else if (state.window == 31
                && std::all_of(bytes.begin(), bytes.end(), [](auto byte) { return byte == 0; })) {
                if (!state.allow_padding)
                    return failure(vm, "ERR_TRAILING_JUNK_AFTER_STREAM_END",
                        "Trailing gzip padding is not a Web Stream member");
                state.padding = true;
                consumed = bytes.size();
                done = final;
            } else if (state.window != 31 || state.padding) {
                return failure(vm, "Z_DATA_ERROR", "Unexpected trailing compressed data");
            } else {
                if (inflateReset(&state.stream) != Z_OK)
                    return failure(vm, "Z_STREAM_ERROR", "Could not reset gzip member");
                state.member_ended = false;
            }
        }

        if (!state.member_ended) {
            if (!state.active) {
                if (!state.window) {
                    if (bytes.empty()) {
                        if (final)
                            return failure(vm, "Z_BUF_ERROR", "Empty deflate response");
                    } else
                        state.window = (bytes[0] & 15) == 8 ? 15 : -15;
                }

                if (state.window) {
                    if (inflateInit2(&state.stream, state.window) != Z_OK)
                        return failure(vm, "Z_MEM_ERROR", "Could not initialize inflater");
                    state.active = true;
                }
            }

            if (state.active) {
                state.stream.next_in = bytes.empty() ? Z_NULL : bytes.data();
                state.stream.avail_in = static_cast<uInt>(bytes.size());
                state.stream.next_out = output.data();
                state.stream.avail_out = static_cast<uInt>(output.size());
                int status = inflate(&state.stream, final ? Z_FINISH : Z_NO_FLUSH);
                consumed = bytes.size() - state.stream.avail_in;
                produced = output.size() - state.stream.avail_out;
                // Native buffers never survive this call, including its error path.
                state.stream.next_in = Z_NULL;
                state.stream.next_out = Z_NULL;
                state.stream.avail_in = state.stream.avail_out = 0;
                if (status == Z_STREAM_END)
                    state.member_ended = true;
                else if (status == Z_NEED_DICT)
                    return failure(vm, "Z_NEED_DICT", "Compressed stream requires a dictionary");
                else if (status != Z_OK && status != Z_BUF_ERROR)
                    return failure(vm, "Z_DATA_ERROR", "Invalid compressed stream or checksum");
                if (final && !state.member_ended && produced < output_limit)
                    return failure(vm, "Z_BUF_ERROR", "Unexpected end of compressed stream");
                done = final && state.member_ended && consumed == bytes.size();
            }
        }

        auto& realm = vm.realm();
        GC::Root result { JS::Object::create(realm, nullptr) };
        GC::Root data { MUST(JS::Uint8Array::create(realm, produced)) };
        if (produced)
            data->viewed_array_buffer()->overwrite(0, output.data(), produced);
        result->define_direct_property("data"_utf16, data.ptr(), JS::default_attributes);
        result->define_direct_property(
            "consumed"_utf16, JS::Value(static_cast<double>(consumed)), JS::default_attributes);
        result->define_direct_property("needInput"_utf16,
            JS::Value(consumed == bytes.size() && (produced < output_limit || state.member_ended)),
            JS::default_attributes);
        result->define_direct_property("done"_utf16, JS::Value(done), JS::default_attributes);
        return JS::Value(result.ptr());
    }
}

GC::Ref<JS::Object> create_inflate_host(JS::Realm& realm)
{
    GC::Root result { JS::Object::create(realm, nullptr) };
    result->define_native_function(
        realm, "create"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(vm.argument(0).to_utf16_string(vm));
            auto state = std::make_shared<Inflater>();
            if (name == "gzip"_utf16)
                state->window = 31;
            else if (name == "gzip-http"_utf16) {
                state->window = 31;
                state->allow_padding = true;
            } else if (name == "deflate"_utf16)
                state->window = 15;
            else if (name == "deflate-raw"_utf16)
                state->window = -15;
            else if (name != "auto-deflate"_utf16)
                return vm.throw_completion<JS::TypeError>("Unsupported inflate format"_utf16);
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
        1, JS::default_attributes);
    return *result;
}
}
