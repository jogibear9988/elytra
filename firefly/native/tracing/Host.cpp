/** Native validation independently rejects unsupported categories and configuration. */
#include "Host.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QDir>
#include <QJsonDocument>
#include <QTemporaryDir>

namespace Firefly::Tracing {
namespace {
    JS::Value text(JS::VM& vm, QByteArray const& bytes)
    {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
    }
}

void install_host(JS::Realm& realm, JS::Object& host, Recorder& recorder)
{
    host.define_native_function(
        realm, "traceTemporaryPath"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            QTemporaryDir directory(QDir::tempPath() + "/firefly-trace-XXXXXX");
            if (!directory.isValid())
                return vm.throw_completion<JS::Error>(
                    "Cannot create private trace directory"_utf16);
            directory.setAutoRemove(false);
            return text(vm, (directory.path() + "/trace.json").toUtf8());
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "traceCategories"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return text(vm,
                QJsonDocument(QJsonArray::fromStringList(Recorder::categories()))
                    .toJson(QJsonDocument::Compact));
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "traceStart"_utf16,
        [&recorder](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string()
                || vm.argument(0).as_string().utf16_string_view().length_in_code_units() > 4096)
                return vm.throw_completion<JS::TypeError>("Invalid tracing configuration"_utf16);
            auto bytes = vm.argument(0).as_string().utf16_string_view().to_utf8();
            QString error;
            if (bytes.is_error()
                || !recorder.start(
                    QByteArray(
                        bytes.value().bytes_as_string_view().characters_without_null_termination(),
                        bytes.value().byte_count()),
                    error))
                return vm.throw_completion<JS::TypeError>(
                    "Invalid or unsupported native tracing configuration"_utf16);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "traceFinish"_utf16,
        [&recorder](
            JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> { return text(vm, recorder.finish()); },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "traceUsage"_utf16,
        [&recorder](
            JS::VM&) -> JS::ThrowCompletionOr<JS::Value> { return JS::Value(recorder.usage()); },
        0, JS::default_attributes);
}
}
