/** Strict UTF-8/path argument validation before any filesystem effect. */
#include "Host.h"
#include "../platform/GCIncludes.h"
#include "../platform/NativeCaptures.h"
#include "Paths.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cstring>

namespace Firefly::AppPaths {
namespace {
    JS::Completion fail(JS::VM& vm, Failure const& failure)
    {
        auto error = JS::Error::create(*vm.current_realm(),
            Utf16String::from_utf8(StringView(failure.message, std::strlen(failure.message))));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(failure.code, std::strlen(failure.code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    JS::ThrowCompletionOr<QString> string(JS::VM& vm, JS::Value value)
    {
        if (!value.is_string())
            return fail(
                vm, { "ERR_INVALID_ARG_TYPE", "Application path arguments must be strings" });
        auto view = value.as_string().utf16_string_view();
        if (view.length_in_code_units() > 32768)
            return fail(vm, { "ERR_OUT_OF_RANGE", "Application path text exceeds budget" });
        auto bytes = view.to_utf8();
        if (bytes.is_error() || bytes.value().byte_count() > 32768)
            return fail(vm, { "ERR_INVALID_ARG_VALUE", "Invalid application path text" });
        auto result = QString::fromUtf8(
            bytes.value().bytes_as_string_view().characters_without_null_termination(),
            bytes.value().byte_count());
        if (result.contains(QChar(0)))
            return fail(vm, { "ERR_INVALID_ARG_VALUE", "Application path text contains NUL" });
        return result;
    }

    JS::Value text(JS::VM& vm, QString const& value)
    {
        auto bytes = value.toUtf8();
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
    }
}

void install_host(JS::Realm& realm, JS::Object& host, Paths& paths)
{
    host.define_native_function(realm, "appPathGet"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::native_reference(paths)),
            [](auto& paths_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& paths = *TRY(Firefly::live_native_capture(vm, paths_capture));
                QString output;
                if (auto error = paths.get(TRY(string(vm, vm.argument(0))), output);
                    error.has_value())
                    return fail(vm, *error);
                return text(vm, output);
            }),
        1, JS::default_attributes);
    host.define_native_function(realm, "appPathSet"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::native_reference(paths)),
            [](auto& paths_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& paths = *TRY(Firefly::live_native_capture(vm, paths_capture));
                auto name = TRY(string(vm, vm.argument(0)));
                auto value = TRY(string(vm, vm.argument(1)));
                if (auto error = paths.set(name, value); error.has_value())
                    return fail(vm, *error);
                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host.define_native_function(realm, "appLogsSet"_utf16,
        GC::create_function(realm.heap(), GC::captures(Mite::native_reference(paths)),
            [](auto& paths_capture, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& paths = *TRY(Firefly::live_native_capture(vm, paths_capture));
                std::optional<QString> path;
                if (!vm.argument(0).is_undefined())
                    path = TRY(string(vm, vm.argument(0)));
                if (auto error = paths.set_logs(path); error.has_value())
                    return fail(vm, *error);
                return JS::js_undefined();
            }),
        0, JS::default_attributes);
}
}
