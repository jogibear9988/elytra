/** Common private filesystem-watcher path snapshots and genuine libuv error projection. */
#pragma once
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cmath>
#include <cstring>
#include <string>
#include <uv.h>

namespace Mite::WatcherSupport {
inline JS::ThrowCompletionOr<std::string> path_argument(JS::VM& vm, JS::Value input)
{
    std::string path;
    if (input.is_string()) {
        auto value = MUST(input.as_string().utf16_string_view().to_byte_string());

        path.assign(value.characters(), value.length());
    } else if (input.is_object() && is<JS::Uint8Array>(input.as_object())) {
        auto& array = static_cast<JS::Uint8Array&>(input.as_object());

        auto witness = JS::make_typed_array_with_buffer_witness_record(
            array, JS::ArrayBuffer::Order::SeqCst);

        if (JS::is_typed_array_out_of_bounds(witness))
            return vm.throw_completion<JS::TypeError>("Detached watch path"sv);

        auto size = JS::typed_array_byte_length(witness);

        if (size > 1048576)
            return vm.throw_completion<JS::RangeError>("Watch path exceeds byte limit"sv);

        if (size)
            path.assign(reinterpret_cast<char const*>(
                            array.viewed_array_buffer()->data_at(array.byte_offset())),
                size);
    } else
        return vm.throw_completion<JS::TypeError>("Invalid internal watch path"sv);

    if (path.size() > 1048576 || path.find('\0') != std::string::npos)
        return vm.throw_completion<JS::TypeError>("Invalid internal watch path"sv);

    return path;
}

inline JS::ThrowCompletionOr<JS::Value> error(
    JS::VM& vm, char const* syscall, bool filename = false)
{
    auto input = vm.argument(0);

    if (!input.is_number() || !std::isfinite(input.as_double()) || input.as_double() >= 0
        || input.as_double() < -65536 || std::floor(input.as_double()) != input.as_double())
        return vm.throw_completion<JS::TypeError>("Invalid internal watcher error"sv);

    auto status = static_cast<int>(input.as_double());

    auto string = [&](char const* value) {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(value, strlen(value))));
    };

    GC::Root result { JS::Error::create(vm.realm(),
        Utf16String::from_utf8(StringView(uv_strerror(status), strlen(uv_strerror(status))))) };
    result->define_direct_property(
        "code"_utf16, string(uv_err_name(status)), JS::default_attributes);

    result->define_direct_property("errno"_utf16, JS::Value(status), JS::default_attributes);

    result->define_direct_property("syscall"_utf16, string(syscall), JS::default_attributes);

    result->define_direct_property("path"_utf16, vm.argument(1), JS::default_attributes);

    if (filename)
        result->define_direct_property("filename"_utf16, vm.argument(1), JS::default_attributes);

    return result.ptr();
}
}
