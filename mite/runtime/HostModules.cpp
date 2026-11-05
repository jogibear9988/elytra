#include "ModuleLoader.h"
#include "BuiltinNames.h"
#include "ModulePaths.h"
#include "ModuleSource.h"
#include <AK/JsonValue.h>
#include <AK/StringBuilder.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/SyntheticModule.h>

namespace Mite {
using namespace ModulePaths;

static bool ascii_letter(char ch)
{
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

JS::ThrowCompletionOr<void> ModuleLoader::register_host_module(std::string const& name,
    GC::Ref<JS::Object> exports, std::vector<std::string> named_exports)
{
    // A narrow native registration grammar keeps URL identities unambiguous.
    // This is not a JS API or a general-purpose package-name validator.
    if (name.empty() || !ascii_letter(name.front()) || known_builtin(name))
        return error("ERR_INVALID_ARG_VALUE", "Invalid or reserved host module name", true);
    for (char ch : name) {
        if (!ascii_letter(ch) && !(ch >= '0' && ch <= '9') && ch != '_' && ch != '-')
            return error("ERR_INVALID_ARG_VALUE", "Host module names must be unscoped ASCII names", true);
    }
    if (m_host_modules.contains(name))
        return error("ERR_MITE_HOST_MODULE_EXISTS", "Host module is already registered: " + name);
    std::unordered_set<std::string> seen;
    for (auto const& exported : named_exports) {
        if (exported.empty() || exported == "default" || !seen.insert(exported).second)
            return error("ERR_INVALID_ARG_VALUE", "Invalid or duplicate host export name", true);
        for (size_t i = 0; i < exported.size(); ++i) {
            auto ch = exported[i];
            if (!ascii_letter(ch) && ch != '_' && ch != '$' && !(i && ch >= '0' && ch <= '9'))
                return error("ERR_INVALID_ARG_VALUE", "Host export names must be ASCII identifiers", true);
        }
    }
    m_host_modules.emplace(name, HostModule { GC::Root { exports }, std::move(named_exports) });
    return {};
}

JS::ThrowCompletionOr<GC::Ref<JS::Module>> ModuleLoader::host_module(std::string const& name)
{
    auto found = m_host_modules.find(name);
    if (found == m_host_modules.end())
        return error("ERR_MODULE_NOT_FOUND", "Host module is not registered: " + name);
    auto identity = "mite-host:" + name;
    if (auto cached = m_modules.find(identity); cached != m_modules.end())
        return *cached->second;
    if (!m_host_bridge) {
        auto getter = JS::NativeFunction::create(m_realm, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string())
                return error("ERR_INVALID_ARG_TYPE", "Host module name must be a string", true);
            auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
            auto found = m_host_modules.find(string(bytes));
            if (found == m_host_modules.end())
                return error("ERR_MODULE_NOT_FOUND", "Host module is not registered");
            return found->second.exports.ptr();
        }, 1, "getHostModule"_utf16);
        m_host_bridge = JS::SyntheticModule::create_default_export_synthetic_module(m_realm, getter, "mite:internal/host").ptr();
    }
    StringBuilder source;
    auto quoted = AK::JsonValue(view(name)).serialized();
    source.appendff("import get from 'mite:internal/host'; const value = get({}); export {{value as default}};", quoted);
    size_t index = 0;
    for (auto const& exported : found->second.named_exports) {
        auto key = AK::JsonValue(view(exported)).serialized();
        // Export aliases allow reserved words without interpolating them as code.
        source.appendff("const e{} = value[{}]; export {{e{} as {}}};", index, key, index, key);
        ++index;
    }
    GC::Root metadata { m_vm.heap().allocate<ModuleSource>(identity) };
    auto parsed = JS::SourceTextModule::parse(Utf16String::from_utf8(source.string_view()), m_realm, view(identity), {}, metadata.ptr());
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    auto module = parsed.release_value();
    m_modules.emplace(identity, GC::Root<JS::Module> { module.ptr() });
    m_host_shims.insert(module.ptr());
    return module;
}
}
