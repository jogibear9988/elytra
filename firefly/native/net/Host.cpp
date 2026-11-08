/** Read-only native availability/proxy queries and deterministic provider retirement. */
#include "Host.h"
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>

namespace Firefly::Network {
namespace {
    JS::ThrowCompletionOr<JS::Value> result(JS::VM& vm, NetworkStateResult const& value)
    {
        if (value.code.empty())
            return JS::Value(value.value);
        GC::Root error { JS::Error::create(vm.realm(),
            Utf16String::from_utf8_with_replacement_character(
                StringView(value.message.data(), value.message.size()))) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm,
                Utf16String::from_utf8_with_replacement_character(
                    StringView(value.code.data(), value.code.size()))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }
}

std::shared_ptr<NetworkState> install_host(
    JS::Realm& realm, JS::Object& host, std::shared_ptr<NetworkState> provider)
{
    if (!provider)
        provider = NetworkState::create();
    VERIFY(provider);
    host.define_native_function(
        realm, "networkIsOnline"_utf16,
        [provider](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return result(vm, provider->is_online());
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "networkHasProxy"_utf16,
        [provider](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return result(vm, provider->has_proxy());
        },
        0, JS::default_attributes);
    host.define_native_function(
        realm, "networkStop"_utf16,
        [provider](JS::VM&) -> JS::ThrowCompletionOr<JS::Value> {
            provider->stop();
            return JS::js_undefined();
        },
        0, JS::default_attributes);
    return provider;
}
}
