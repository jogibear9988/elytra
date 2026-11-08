/** Reuse upstream application preferences and Qt interface theming without patching Ladybird. */
#include "ThemeHost.h"
#include "ThemeMac.h"
#include "ThemeSettings.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <QJsonDocument>
#include <UI/Qt/SystemAppearance.h>

namespace Firefly {
void install_theme_host(JS::Realm& realm, JS::Object& host, std::function<void()> changed)
{
    host.define_native_function(
        realm, "themeSnapshot"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto bytes = QJsonDocument(system_theme_snapshot()).toJson(QJsonDocument::Compact);
            return JS::PrimitiveString::create(
                vm, Utf16String::from_utf8(StringView(bytes.data(), bytes.size())));
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "themeSetSource"_utf16,
        [changed](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return vm.throw_completion<JS::TypeError>("Invalid theme source"_utf16);
            auto value = vm.argument(0).as_string().utf16_string_view();
            auto preference = WebView::ThemePreference::System;
            if (value == u"dark"sv)
                preference = WebView::ThemePreference::Dark;
            else if (value == u"light"sv)
                preference = WebView::ThemePreference::Light;
            else if (value != u"system"sv)
                return vm.throw_completion<JS::TypeError>("Invalid theme source"_utf16);
            set_theme_settings_source(preference);
            Ladybird::SystemAppearance::the().apply_interface_theme(preference);
            changed();
            return JS::js_undefined();
        },
        1, JS::default_attributes);
}

void watch_system_theme(QObject& context, std::function<void()> changed)
{
    QObject::connect(&Ladybird::SystemAppearance::the(), &Ladybird::SystemAppearance::changed,
        &context, [changed] { changed(); });
    watch_accessibility_theme(context, std::move(changed));
}
}
