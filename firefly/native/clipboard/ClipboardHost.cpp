/** Bounded JSON/byte copies keep clipboard providers independent of the JS heap. */
#include "../platform/GCIncludes.h"

#include "Clipboard.h"
#include "ClipboardHost.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QJsonDocument>

namespace Firefly {
void install_clipboard_host(JS::Realm& realm, JS::Object& host)
{
    host.define_native_function(
        realm, "clipboardRequest"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto value = vm.argument(0);
            if (!value.is_string())
                return vm.throw_completion<JS::TypeError>("Invalid clipboard wire record"_utf16);
            auto view = value.as_string().utf16_string_view();
            if (view.length_in_code_units() > 16 * 1024 * 1024)
                return vm.throw_completion<JS::RangeError>("Clipboard wire budget exceeded"_utf16);
            auto bytes = MUST(view.to_utf8());
            auto parsed = QJsonDocument::fromJson(
                QByteArray(bytes.bytes_as_string_view().characters_without_null_termination(),
                    bytes.byte_count()));
            if (!parsed.isObject())
                return vm.throw_completion<JS::TypeError>("Invalid clipboard wire record"_utf16);
            auto result
                = QJsonDocument(clipboard_request(parsed.object())).toJson(QJsonDocument::Compact);
            return JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(result.data(), result.size())));
        },
        1, JS::default_attributes);
}
}
