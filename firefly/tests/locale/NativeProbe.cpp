/** Check native guards directly in LibJS without the public factory or GUI application. */
#include "../isolation/CaptureSlot.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/locale/Host.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <QCoreApplication>

int main(int argc, char** argv)
{
    QCoreApplication application(argc, argv);
    bool ready = false;
    Firefly::Tests::CaptureSlot ready_slot { ready };
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root native { JS::Object::create(realm, nullptr) };
        Firefly::Locale::install_host(
            realm, *native, [&] { return ready; }, [] { return QStringLiteral("sr-Latn-RS"); });
        native->define_native_function(realm, "markReady"_utf16,
            GC::create_function(realm.heap(), GC::captures(Mite::native_reference(ready_slot)),
                [](auto& slot, JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
                    slot.get().value = true;
                    return JS::js_undefined();
                }),
            0, JS::default_attributes);
        return modules.register_host_module(
            "locale-test", *native, { "appLocaleRead", "markReady" });
    };

    return Mite::run(argc, argv, hooks);
}
