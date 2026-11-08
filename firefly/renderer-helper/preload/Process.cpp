/** Capture the executable before sandbox bootstrap; create fresh mutable data per isolated
 * document. */
#include "Process.h"
#include "worlds/IsolatedWorld.h"
#include <AK/ByteString.h>
#include <AK/NeverDestroyed.h>
#include <LibCore/System.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#ifndef FIREFLY_LIBJS_REVISION
#error Missing pinned renderer engine revision
#endif
namespace Firefly::PreloadProcess {
namespace {
    NeverDestroyed<ByteString> executable;

    StringView platform()
    {
#if defined(_WIN32)
        return "win32"sv;
#elif defined(__APPLE__)
        return "darwin"sv;
#elif defined(__linux__)
        return "linux"sv;
#else
#error Unsupported renderer platform
#endif
    }

    StringView architecture()
    {
#if defined(__aarch64__) || defined(_M_ARM64)
        return "arm64"sv;
#elif defined(__x86_64__) || defined(_M_X64)
        return "x64"sv;
#else
#error Unsupported renderer architecture
#endif
    }
}

ErrorOr<void> prepare()
{
    executable.get() = TRY(Core::System::current_executable_path());
    return {};
}

JS::ThrowCompletionOr<JS::Value> install(IsolatedWorld& world, Configuration const& config)
{
    auto& realm = *world.realm();
    auto& vm = realm.vm();
    auto text = [&](StringView value) {
        return JS::PrimitiveString::create(
            vm, Utf16String::from_utf8_with_replacement_character(value));
    };

    GC::Root process { JS::Object::create(realm, realm.intrinsics().object_prototype()) };
    // Identity is read-only; data containers below remain local mutable snapshots.
    auto constant = JS::Attribute::Enumerable | JS::Attribute::Configurable;
    process->define_direct_property("platform"_utf16, text(platform()), constant);
    process->define_direct_property("arch"_utf16, text(architecture()), constant);
    process->define_direct_property(
        "pid"_utf16, JS::Value(static_cast<double>(Core::System::getpid())), constant);
    process->define_direct_property("execPath"_utf16, text(executable.get()), constant);
    process->define_direct_property("type"_utf16, text("renderer"sv), constant);
    process->define_direct_property("version"_utf16, text("v0.1.0"sv), constant);
    process->define_direct_property("sandboxed"_utf16, JS::Value(true), constant);
    process->define_direct_property("contextIsolated"_utf16, JS::Value(true), constant);
    process->define_direct_property("isMainFrame"_utf16, JS::Value(true), constant);
    GC::Root versions { JS::Object::create(realm, realm.intrinsics().object_prototype()) };
    versions->define_direct_property("firefly"_utf16, text("0.1.0"sv), constant);
    versions->define_direct_property("libjs"_utf16,
        text(StringView(FIREFLY_LIBJS_REVISION, sizeof(FIREFLY_LIBJS_REVISION) - 1)), constant);
    versions->define_direct_property("ladybird"_utf16,
        text(StringView(FIREFLY_LIBJS_REVISION, sizeof(FIREFLY_LIBJS_REVISION) - 1)), constant);
    process->define_direct_property("versions"_utf16, versions, JS::default_attributes);
    GC::Root argv { TRY(JS::Array::create(realm, config.arguments.size() + 1)) };
    argv->define_direct_property(0, text(executable.get()), JS::default_attributes);
    for (size_t i = 0; i < config.arguments.size(); ++i)
        argv->define_direct_property(
            i + 1, text(config.arguments[i].as_string()), JS::default_attributes);
    process->define_direct_property("argv"_utf16, argv, JS::default_attributes);
    GC::Root env { JS::Object::create(realm, realm.intrinsics().object_prototype()) };
    config.environment.for_each_member([&](String const& key, JsonValue const& value) {
        env->define_direct_property(Utf16String::from_utf8_with_replacement_character(key),
            text(value.as_string()), JS::default_attributes);
    });
    process->define_direct_property("env"_utf16, env, JS::default_attributes);
    // These demanded method names fail explicitly until actual event/error and OS
    // memory contracts are implemented. They never register inert listeners/data.
    for (auto name : { "on"_utf16, "getProcessMemoryInfo"_utf16 }) {
        GC::Root function { JS::NativeFunction::create(
            realm,
            +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto error
                    = JS::Error::create(vm.realm(), u"Sandbox process method is not implemented"sv);
                error->define_direct_property("code"_utf16,
                    JS::PrimitiveString::create(vm, u"ERR_FIREFLY_UNSUPPORTED_API"sv),
                    JS::default_attributes);
                return JS::throw_completion(error);
            },
            0, name, GC::Ptr<JS::Realm> { realm }) };
        world.block_function_export(*function);
        process->define_direct_property(name, function, JS::default_attributes);
    }

    realm.global_object().define_direct_property("process"_utf16, process, JS::default_attributes);
    return process.ptr();
}
}
