/** Validate native strings independently; no engine switch is silently acknowledged. */
#include "Host.h"
#include "State.h"
#include <AK/String.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>

namespace Firefly::CommandLine {
namespace {
    JS::Completion fail(JS::VM& vm, Failure const& failure)
    {
        auto& realm = *vm.current_realm();
        auto error = JS::Error::create(realm,
            Utf16String::from_utf8(StringView(failure.message.data(), failure.message.size())));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(failure.code.data(), failure.code.size()))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<std::string> string(JS::VM& vm, JS::Value value, size_t limit)
    {
        if (!value.is_string())
            return fail(vm, { "ERR_INVALID_ARG_TYPE", "Switch arguments must be strings" });
        auto view = value.as_string().utf16_string_view();
        if (view.length_in_code_units() > limit)
            return fail(vm, { "ERR_OUT_OF_RANGE", "Switch text exceeds budget" });
        auto bytes = view.to_utf8();
        if (bytes.is_error() || bytes.value().byte_count() > limit)
            return fail(vm, { "ERR_INVALID_ARG_VALUE", "Invalid switch text" });
        std::string result(
            bytes.value().bytes_as_string_view().characters_without_null_termination(),
            bytes.value().byte_count());
        if (result.find('\0') != result.npos)
            return fail(vm, { "ERR_INVALID_ARG_VALUE", "Switch text contains NUL" });
        return result;
    }
}

void install_host(JS::Realm& realm, JS::Object& host, State& state)
{
    host.define_native_function(
        realm, "commandLineRead"_utf16,
        [&state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(string(vm, vm.argument(0), 128));
            if (!State::valid_name(name))
                return fail(vm, { "ERR_INVALID_ARG_VALUE", "Invalid switch name" });
            if (vm.argument(1).to_boolean())
                return JS::Value(state.has(name));
            auto value = state.value(name);
            auto text = String::from_utf8(StringView(value.data(), value.size()));
            if (text.is_error())
                return fail(vm, { "ERR_INVALID_ARG_VALUE", "Initial switch value is not UTF-8" });
            return JS::PrimitiveString::create(vm, Utf16String::from_utf8(text.value()));
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "commandLineChange"_utf16,
        [&state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(string(vm, vm.argument(0), 128));
            std::optional<std::string> value;
            if (!vm.argument(1).is_null())
                value = TRY(string(vm, vm.argument(1), 32768));
            if (auto error = state.change(name, std::move(value)); error.has_value())
                return fail(vm, *error);
            return JS::js_undefined();
        },
        2, JS::default_attributes);
}
}
