/** Genuine Mite runtime + exact native net Host + captured original transport, without Qt GUI. */
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/net/Host.h"
#include <LibGC/Heap.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>

int main(int argc, char** argv)
{
    std::shared_ptr<Firefly::NetworkState> provider;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM&, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root native { JS::Object::create(realm, nullptr) };
        provider = Firefly::Network::install_host(realm, *native);
        auto transport = TRY(modules.web_transport());
        native->define_direct_property("transport"_utf16, transport.ptr(), JS::default_attributes);
        native->define_native_function(
            realm, "collect"_utf16,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                vm.heap().collect_garbage();
                return JS::js_undefined();
            },
            0, JS::default_attributes);
        return modules.register_host_module("net-test", *native,
            { "networkIsOnline", "networkHasProxy", "networkStop", "transport", "collect" });
    };

    hooks.shutdown = [&] {
        if (provider)
            provider->stop();
        provider.reset();
    };

    return Mite::run(argc, argv, hooks);
}
