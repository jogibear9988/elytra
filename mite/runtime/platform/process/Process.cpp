/**
 * Creates the main-process object and its native environment/identity hooks. JavaScript
 * compatibility policy lives in the process module; OS handles and exit requests stay native.
 */

#include "BuiltinSources.h"
#include "UpstreamVersion.h"
#include "core/GCCaptures.h"
#include "platform/process/ProcessHost.h"
#include "platform/process/Timing.h"
#include <AK/ScopeGuard.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Script.h>
#include <cstdlib>
#include <cstring>
#include <llhttp.h>
#include <uv.h>
#include <vector>

namespace Mite {

static GC::Ref<JS::PrimitiveString> text(JS::VM& vm, char const* bytes)
{
    return JS::PrimitiveString::create(
        vm, Utf16String::from_utf8_with_replacement_character(StringView(bytes, strlen(bytes))));
}

static JS::Completion uv_error(JS::VM& vm, char const* syscall, int code,
    char const* path = nullptr, char const* dest = nullptr)
{
    auto error = JS::Error::create(vm.realm(),
        Utf16String::from_utf8(StringView(uv_strerror(code), strlen(uv_strerror(code)))));
    error->define_direct_property(
        "code"_utf16, text(vm, uv_err_name(code)), JS::default_attributes);
    error->define_direct_property("errno"_utf16, JS::Value(code), JS::default_attributes);
    error->define_direct_property("syscall"_utf16, text(vm, syscall), JS::default_attributes);
    if (path)
        error->define_direct_property("path"_utf16, text(vm, path), JS::default_attributes);
    if (dest)
        error->define_direct_property("dest"_utf16, text(vm, dest), JS::default_attributes);
    return JS::throw_completion(error);
}

static JS::ThrowCompletionOr<ByteString> string_argument(JS::VM& vm, size_t index)
{
    if (!vm.argument(index).is_string()) {
        auto error = JS::TypeError::create(vm.realm(), "Argument must be a string"_utf16);
        error->define_direct_property(
            "code"_utf16, text(vm, "ERR_INVALID_ARG_TYPE"), JS::default_attributes);
        return JS::throw_completion(error);
    }

    auto value = MUST(vm.argument(index).as_string().utf16_string_view().to_byte_string());
    if (value.contains('\0')) {
        auto error
            = JS::TypeError::create(vm.realm(), "Arguments must not contain null bytes"_utf16);
        error->define_direct_property(
            "code"_utf16, text(vm, "ERR_INVALID_ARG_VALUE"), JS::default_attributes);
        return JS::throw_completion(error);
    }

    return value;
}

static JS::ThrowCompletionOr<ByteString> current_directory(JS::VM& vm)
{
    std::vector<char> buffer(256);
    for (;;) {
        size_t size = buffer.size();
        int code = uv_cwd(buffer.data(), &size);
        if (code == UV_ENOBUFS) {
            buffer.resize(size + 1);
            continue;
        }

        if (code)
            return uv_error(vm, "uv_cwd", code);
        return ByteString(buffer.data());
    }
}

char const* native_platform()
{
#if defined(_WIN32)
    return "win32";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux";
#else
#error Unsupported Mite process platform
#endif
}

static char const* architecture()
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x64";
#else
#error Unsupported Mite process architecture
#endif
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_process(
    JS::VM& vm, JS::Realm& realm, std::shared_ptr<ProcessExitState> exit_state)
{
    if (!exit_state)
        exit_state = std::make_shared<ProcessExitState>();
    GC::Root process { JS::Object::create(realm, realm.intrinsics().object_prototype()) };
    auto constant = JS::Attribute::Enumerable | JS::Attribute::Configurable;
    process->define_direct_property("platform"_utf16, text(vm, native_platform()), constant);
    process->define_direct_property("arch"_utf16, text(vm, architecture()), constant);
    process->define_direct_property(
        "pid"_utf16, JS::Value(static_cast<double>(uv_os_getpid())), constant);
    process->define_direct_property(
        "ppid"_utf16, JS::Value(static_cast<double>(uv_os_getppid())), constant);
    process->define_native_function(
        realm, "cwd"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto directory = TRY(current_directory(vm));
            return text(vm, directory.characters());
        },
        0, JS::default_attributes);
    process->define_native_function(
        realm, "chdir"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto directory = TRY(string_argument(vm, 0));
            auto code = uv_chdir(directory.characters());
            if (code) {
                auto before = current_directory(vm);
                return uv_error(vm, "chdir", code,
                    before.is_error() ? nullptr : before.value().characters(),
                    directory.characters());
            }

            return JS::js_undefined();
        },
        1, JS::default_attributes);
    std::vector<char> executable(256);
    for (;;) {
        size_t size = executable.size();
        auto code = uv_exepath(executable.data(), &size);
        if (code == UV_ENOBUFS) {
            executable.resize(executable.size() * 2);
            continue;
        }

        if (code)
            return uv_error(vm, "uv_exepath", code);
        break;
    }

    process->define_direct_property(
        "execPath"_utf16, text(vm, executable.data()), JS::default_attributes);
    process->define_direct_property(
        "execArgv"_utf16, TRY(JS::Array::create(realm, 0)), JS::default_attributes);
    process->define_direct_property("version"_utf16, text(vm, "v0.1.0"), JS::default_attributes);
    auto versions = JS::Object::create(realm, realm.intrinsics().object_prototype());
    versions->define_direct_property("mite"_utf16, text(vm, "0.1.0"), constant);
    versions->define_direct_property("llhttp"_utf16,
        JS::PrimitiveString::create(vm,
            Utf16String::formatted(
                "{}.{}.{}", LLHTTP_VERSION_MAJOR, LLHTTP_VERSION_MINOR, LLHTTP_VERSION_PATCH)),
        constant);
    versions->define_direct_property("uv"_utf16, text(vm, uv_version_string()), constant);
    versions->define_direct_property("libjs"_utf16, text(vm, MITE_LIBJS_REVISION), constant);
    process->define_direct_property("versions"_utf16, versions, constant);
    auto release = JS::Object::create(realm, realm.intrinsics().object_prototype());
    release->define_direct_property("name"_utf16, text(vm, "mite"), constant);
    process->define_direct_property("release"_utf16, release, constant);

    GC::Root host { JS::Object::create(realm, nullptr) };
    install_process_timing(realm, *host);
    host->define_native_function(
        realm, "envGet"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(string_argument(vm, 0));
            std::vector<char> buffer(256);
            for (;;) {
                size_t size = buffer.size();
                auto code = uv_os_getenv(name.characters(), buffer.data(), &size);
                if (code == UV_ENOENT)
                    return JS::js_undefined();
                if (code == UV_ENOBUFS) {
                    buffer.resize(size + 1);
                    continue;
                }

                if (code)
                    return uv_error(vm, "uv_os_getenv", code);
                return text(vm, buffer.data());
            }
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "envSet"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(string_argument(vm, 0));
            auto value = TRY(string_argument(vm, 1));
            auto code = uv_os_setenv(name.characters(), value.characters());
            if (code)
                return uv_error(vm, "uv_os_setenv", code);
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "envDelete"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto name = TRY(string_argument(vm, 0));
            auto code = uv_os_unsetenv(name.characters());
            if (code)
                return uv_error(vm, "uv_os_unsetenv", code);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host->define_native_function(
        realm, "envKeys"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            uv_env_item_t* entries = nullptr;
            int count = 0;
            auto code = uv_os_environ(&entries, &count);
            if (code)
                return uv_error(vm, "uv_os_environ", code);
            ScopeGuard release { [&] { uv_os_free_environ(entries, count); } };
            GC::RootVector<JS::Value> keys;
            for (int index = 0; index < count; ++index)
                keys.append(text(vm, entries[index].name));
            return JS::Array::create_from(vm.realm(), keys);
        },
        0, JS::default_attributes);
    host->define_native_function(realm, "exit"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(exit_state)),
            [](auto& exit_state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_int32() || !vm.argument(1).is_object())
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid internal process exit"_utf16);
                auto code = TRY(emit_process_exit(
                    vm, vm.argument(1).as_object(), *exit_state, vm.argument(0).as_i32(), false));
                std::exit(code);
            }),
        2, JS::default_attributes);
    auto source
        = Utf16String::from_utf8(StringView(MITE_PROCESS_SOURCE, sizeof(MITE_PROCESS_SOURCE) - 1));
    auto parsed = JS::Script::parse(source, realm, "mite:process"sv);
    if (parsed.is_error())
        return vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    GC::Root script { parsed.release_value() };
    auto factory = TRY(vm.run(*script));
    TRY(JS::call(vm, factory, JS::js_undefined(), JS::Value(host.ptr()), JS::Value(process.ptr())));
    return *process;
}

}
