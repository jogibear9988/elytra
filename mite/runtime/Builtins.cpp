#include "ModuleLoader.h"
#include "ModulePaths.h"
#include "ModuleSource.h"
#include "BuiltinSources.h"
#include "BuiltinNames.h"
#include "Process.h"

#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/SyntheticModule.h>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace Mite {
using namespace ModulePaths;

struct BuiltinDefinition {
    std::string_view name;
    std::string_view source;
    std::span<std::string_view const> exports;
};
static constexpr std::string_view events_exports[] = {
    "EventEmitter", "errorMonitor", "captureRejectionSymbol", "captureRejections",
    "defaultMaxListeners", "listenerCount", "getEventListeners", "getMaxListeners", "setMaxListeners", "once"
};
static constexpr std::string_view process_exports[] = {
    "cwd", "chdir", "env", "pid", "ppid", "platform", "arch", "execPath", "argv", "argv0", "version", "versions", "release", "exit"
};
static constexpr std::string_view path_exports[] = {
    "resolve", "normalize", "isAbsolute", "join", "relative", "dirname", "basename", "extname", "format", "parse",
    "toNamespacedPath", "_makeLong", "sep", "delimiter", "win32", "posix"
};
static constexpr BuiltinDefinition definitions[] = {
    { "events", { MITE_EVENTS_SOURCE, sizeof(MITE_EVENTS_SOURCE) - 1 }, events_exports },
    { "process", {}, process_exports },
    { "path", { MITE_PATH_SOURCE, sizeof(MITE_PATH_SOURCE) - 1 }, path_exports },
    { "path/posix", {}, path_exports },
    { "path/win32", {}, path_exports }
};
static BuiltinDefinition const* definition_for(std::string const& name)
{
    for (auto const& definition : definitions) {
        if (definition.name == name)
            return &definition;
    }
    return nullptr;
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> ModuleLoader::builtin(std::string const& name)
{
    if (auto found = m_builtins.find(name); found != m_builtins.end())
        return *found->second;
    if (!known_builtin(name))
        return error("ERR_UNKNOWN_BUILTIN_MODULE", "No such builtin module: node:" + name);
    auto const* definition = definition_for(name);
    if (!definition)
        return error("ERR_MITE_UNSUPPORTED_BUILTIN", "Builtin module is not implemented: node:" + name);
    if (name == "process") {
        m_builtins.emplace(name, m_process);
        return *m_process;
    }
    if (name == "path/posix" || name == "path/win32") {
        auto path = TRY(builtin("path"));
        auto alias = TRY(path->get(name == "path/posix" ? "posix"_utf16 : "win32"_utf16));
        if (!alias.is_object())
            return error("ERR_MITE_BUILTIN_INITIALIZATION", "Path alias must be an object");
        m_builtins.emplace(name, GC::Root<JS::Object> { alias.as_object() });
        return alias.as_object();
    }
    auto source = Utf16String::from_utf8(StringView(definition->source.data(), definition->source.size()));
    auto identity = "node:" + name;
    auto parsed = JS::Script::parse(source, m_realm, view(identity));
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    GC::Root script { parsed.release_value() };
    auto factory = TRY(m_vm.run(*script));
    GC::Root host { JS::Object::create(m_realm, nullptr) };
    host->define_direct_property("process"_utf16, m_process.ptr(), JS::default_attributes);
    host->define_direct_property("platform"_utf16,
        JS::PrimitiveString::create(m_vm, Utf16String::from_utf8(StringView(native_platform(), strlen(native_platform())))), JS::default_attributes);
    host->define_native_function(m_realm, "warn"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto name = TRY(vm.argument(0).to_utf16_string(vm));
        auto message = TRY(vm.argument(1).to_utf16_string(vm));
        auto formatted = Utf16String::formatted("{}: {}\n", name, message);
        auto line = MUST(formatted.utf16_view().to_byte_string());
        fwrite(line.characters(), 1, line.length(), stderr);
        return JS::js_undefined();
    }, 2, JS::default_attributes);
    auto value = TRY(JS::call(m_vm, factory, JS::js_undefined(), JS::Value(host.ptr())));
    if (!value.is_object())
        return error("ERR_MITE_BUILTIN_INITIALIZATION", "Builtin factory returned a non-object");
    m_builtins.emplace(name, GC::Root<JS::Object> { value.as_object() });
    return value.as_object();
}

JS::ThrowCompletionOr<GC::Ref<JS::Module>> ModuleLoader::builtin_module(std::string const& name)
{
    auto identity = "node:" + name;
    if (auto found = m_modules.find(identity); found != m_modules.end())
        return *found->second;
    // Initialize before defining the fixed ESM export list. Future native modules
    // share this registry and the same object returned by CommonJS require.
    TRY(builtin(name));
    if (!m_builtin_bridge) {
        auto bridge = JS::NativeFunction::create(m_realm, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return error("ERR_INVALID_ARG_TYPE", "Builtin name must be a string", true);
            auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
            return TRY(builtin(string(bytes)));
        }, 1, "getBuiltin"_utf16);
        m_builtin_bridge = JS::SyntheticModule::create_default_export_synthetic_module(m_realm, bridge, "mite:internal/builtin").ptr();
    }
    auto quoted = AK::JsonValue(view(name)).serialized();
    StringBuilder source;
    source.appendff("import get from 'mite:internal/builtin'; const value = get({}); export {{ value as default }};", quoted);
    auto const* definition = definition_for(name);
    VERIFY(definition);
    for (auto export_name : definition->exports) {
        StringView exported { export_name.data(), export_name.size() };
        source.appendff("export const {} = value.{};", exported, exported);
    }
    GC::Root metadata { m_vm.heap().allocate<ModuleSource>(identity) };
    auto parsed = JS::SourceTextModule::parse(Utf16String::from_utf8(source.string_view()), m_realm, view(identity), {}, metadata.ptr());
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    auto module = parsed.release_value();
    m_modules.emplace(identity, GC::Root<JS::Module> { module.ptr() });
    m_builtin_shims.insert(module.ptr());
    return module;
}

}
