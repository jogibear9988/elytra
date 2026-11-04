#include "DNS.h"
#include "EventLoop.h"
#include "Network.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>

namespace Mite {
namespace {
static JS::Value text(JS::VM& vm, char const* bytes)
{
    return JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(bytes, strlen(bytes))));
}
struct Resolver;
struct Lookup {
    Resolver& owner;
    uv_getaddrinfo_t request {};
    GC::Root<JS::FunctionObject> callback;
};
struct Resolver {
    JS::VM& vm;
    JS::Realm& realm;
    EventLoop& loop;
    std::unordered_set<Lookup*> requests;
    ~Resolver() { VERIFY(requests.empty()); }
};
static JS::ThrowCompletionOr<int> integer(JS::VM& vm, JS::Value value)
{
    if (!value.is_number() || !std::isfinite(value.as_double()) || std::trunc(value.as_double()) != value.as_double()
        || value.as_double() < -2147483648.0 || value.as_double() > 2147483647.0)
        return vm.throw_completion<JS::TypeError>("Invalid internal DNS integer"_utf16);
    return static_cast<int>(value.as_double());
}
}

GC::Ref<JS::Object> create_dns_host(JS::VM& vm, JS::Realm& realm, EventLoop& loop)
{
    auto resolver = std::make_shared<Resolver>(vm, realm, loop);
    loop.on_shutdown([resolver] {
        for (auto* request : resolver->requests) uv_cancel(reinterpret_cast<uv_req_t*>(&request->request));
    });
    auto host = JS::Object::create(realm, nullptr);
    host->define_direct_property("ADDRCONFIG"_utf16, JS::Value(AI_ADDRCONFIG), JS::default_attributes);
    host->define_direct_property("V4MAPPED"_utf16, JS::Value(AI_V4MAPPED), JS::default_attributes);
    host->define_direct_property("ALL"_utf16, JS::Value(AI_ALL), JS::default_attributes);
    host->define_native_function(realm, "isIP"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        if (!vm.argument(0).is_string()) return JS::Value(0);
        auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
        return JS::Value(network_ip_family(bytes.view()));
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "error"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        int status = TRY(integer(vm, vm.argument(0)));
        auto value = JS::Object::create(vm.realm(), nullptr);
        auto* code = (status == UV_EAI_NONAME || status == UV_EAI_NODATA) ? "ENOTFOUND" : uv_err_name(status);
        value->define_direct_property("code"_utf16, text(vm, code), JS::default_attributes);
        return value;
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "lookup"_utf16, [resolver](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        if (!vm.argument(0).is_string() || !vm.argument(3).is_function())
            return vm.throw_completion<JS::TypeError>("Invalid internal DNS arguments"_utf16);
        int family = TRY(integer(vm, vm.argument(1)));
        if (family != 0 && family != 4 && family != 6) return vm.throw_completion<JS::TypeError>("Invalid internal DNS family"_utf16);
        int flags = TRY(integer(vm, vm.argument(2)));
        if (flags & ~(AI_ADDRCONFIG | AI_V4MAPPED | AI_ALL)) return vm.throw_completion<JS::TypeError>("Invalid internal DNS flags"_utf16);
        auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
        if (bytes.view().contains('\0')) return JS::Value(UV_EAI_NONAME);
        std::string hostname(bytes.characters(), bytes.length());
        addrinfo hints {};
        hints.ai_family = family == 4 ? AF_INET : family == 6 ? AF_INET6 : AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = flags;
        auto* request = new Lookup { *resolver, {}, GC::Root { vm.argument(3).as_function() } };
        request->request.data = request;
        int status = uv_getaddrinfo(&resolver->loop.native_loop(), &request->request, [](uv_getaddrinfo_t* native, int status, addrinfo* addresses) {
            auto* request = static_cast<Lookup*>(native->data);
            auto& owner = request->owner;
            owner.requests.erase(request);
            owner.loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                GC::Root values { TRY(JS::Array::create(owner.realm, 0)) };
                std::unordered_set<std::string> seen;
                size_t index = 0;
                if (!status) for (auto* item = addresses; item; item = item->ai_next) {
                    char name[INET6_ADDRSTRLEN] {};
                    int family = item->ai_family == AF_INET ? 4 : item->ai_family == AF_INET6 ? 6 : 0;
                    int error = family == 4 ? uv_ip4_name(reinterpret_cast<sockaddr_in*>(item->ai_addr), name, sizeof(name))
                        : family == 6 ? uv_ip6_name(reinterpret_cast<sockaddr_in6*>(item->ai_addr), name, sizeof(name)) : UV_EINVAL;
                    if (error || !seen.insert(name).second) continue;
                    auto entry = JS::Object::create(owner.realm, owner.realm.intrinsics().object_prototype());
                    entry->define_direct_property("address"_utf16, text(owner.vm, name), JS::default_attributes);
                    entry->define_direct_property("family"_utf16, JS::Value(family), JS::default_attributes);
                    TRY(values->create_data_property_or_throw(index++, entry));
                }
                if (!status && !index) status = UV_EAI_NONAME;
                return JS::call(owner.vm, *request->callback, JS::js_undefined(), JS::Value(status), JS::Value(values.ptr()));
            });
            uv_freeaddrinfo(addresses);
            owner.loop.request_finished();
            delete request;
        }, hostname.c_str(), nullptr, &hints);
        if (status) delete request;
        else { resolver->requests.insert(request); resolver->loop.request_started(); }
        return JS::Value(status);
    }, 4, JS::default_attributes);
    return host;
}
}
