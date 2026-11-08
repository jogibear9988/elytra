/** Run the real public storage facade and native provider in an explicit private keychain.
 * This is a component host, not a desktop/window lifecycle acceptance executable.
 */
#include "SafeStorageSource.h"
#include "core/Runtime.h"
#include "modules/ModuleLoader.h"
#include "native/safe-storage/Backend.h"
#include "native/safe-storage/Host.h"
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <Security/Security.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

int main(int argc, char** argv)
{
    std::string path, profile;
    int entry = 1;
    while (entry + 1 < argc) {
        if (std::string_view(argv[entry]) == "--test-keychain")
            path = argv[entry + 1];
        else if (std::string_view(argv[entry]) == "--test-profile")
            profile = argv[entry + 1];
        else
            break;
        entry += 2;
    }

    if (path.empty() || profile.empty() || entry >= argc) {
        fputs("Require explicit --test-keychain path --test-profile identity script\n", stderr);
        return 2;
    }
#ifdef __APPLE__
    // Only this caller-selected temporary keychain is opened/created. Never change
    // default/search-list preferences or open the login keychain as a fallback.
    SecKeychainRef keychain = nullptr;
    constexpr char password[] = "elytra-private-native-component-fixture";
    OSStatus status;
    if (std::filesystem::exists(path))
        status = SecKeychainOpen(path.c_str(), &keychain);
    else
        status = SecKeychainCreate(
            path.c_str(), sizeof(password) - 1, password, false, nullptr, &keychain);
    if (status != errSecSuccess || !keychain) {
        fprintf(stderr, "Private test keychain open/create failed (%d)\n", status);
        return 1;
    }

    status = SecKeychainUnlock(keychain, sizeof(password) - 1, password, true);
    if (status != errSecSuccess) {
        fprintf(stderr, "Private test keychain unlock failed (%d)\n", status);
        CFRelease(keychain);
        return 1;
    }
#endif
    std::vector<char*> arguments { argv[0] };
    for (int index = entry; index < argc; ++index)
        arguments.push_back(argv[index]);
    arguments.push_back(nullptr);
    bool ready = true;
    Mite::HostHooks hooks;
    hooks.initialize = [&](JS::VM& vm, JS::Realm& realm, Mite::ModuleLoader& modules,
                           Mite::EventLoop&) -> JS::ThrowCompletionOr<void> {
        GC::Root native { JS::Object::create(realm, nullptr) };
        Firefly::SafeStorage::install_host(
            realm, *native, [&] { return ready; },
            [&] {
#ifdef __APPLE__
                return std::make_unique<Firefly::SafeStorage::Backend>(profile, keychain);
#else
                return std::make_unique<Firefly::SafeStorage::Backend>(profile);
#endif
            });
        native->define_native_function(
            realm, "markReady"_utf16,
            [&](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                ready = vm.argument(0).to_boolean();
                return JS::js_undefined();
            },
            1, JS::default_attributes);
        TRY(modules.register_host_module("safe-storage-test", *native,
            { "safeStorageAvailable", "safeStorageEncrypt", "safeStorageDecrypt", "markReady" }));
        auto factory = TRY(modules.run_eval(FIREFLY_SAFE_STORAGE_SOURCE));
        GC::Root factory_root { factory.as_object() };
        // Actual storage implementation, deliberately minimal component lifecycle.
        auto environment = TRY(modules.run_eval(
            "({ app: { isReady: () => true, whenReady: () => Promise.resolve(), commandLine: { "
            "getSwitchValue: () => '' } }, error: (code,message) => Object.assign(new "
            "Error(message), "
            "{code}), unsupported: name => {throw Object.assign(new Error(name+' is unsupported'), "
            "{code:'ERR_FIREFLY_UNSUPPORTED_API'});} })"));
        GC::Root env { environment.as_object() };
        auto storage = TRY(JS::call(vm, factory, JS::js_undefined(), JS::Value(native.ptr()),
            TRY(env->get("app"_utf16)), TRY(env->get("error"_utf16)),
            TRY(env->get("unsupported"_utf16))));
        GC::Root api { JS::Object::create(realm, nullptr) };
        api->define_direct_property("safeStorage"_utf16, storage, JS::default_attributes);
        api->define_direct_property(
            "app"_utf16, TRY(env->get("app"_utf16)), JS::default_attributes);
        TRY(modules.register_host_module("firefly", *api, { "app", "safeStorage" }));
        TRY(modules.register_host_module("electron", *api, { "app", "safeStorage" }));
        return {};
    };

    int result = Mite::run(static_cast<int>(arguments.size()) - 1, arguments.data(), hooks);
#ifdef __APPLE__
    CFRelease(keychain);
#endif
    return result;
}
#ifdef __APPLE__
#pragma clang diagnostic pop
#endif
