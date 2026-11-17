/** Create ESM-owned CommonJS contexts without caching or executing the synthetic parent. */
#include "core/GCCaptures.h"
#include "modules/BuiltinNames.h"
#include "modules/ModuleLoader.h"
#include "modules/ModulePaths.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/NativeFunction.h>

namespace Mite {
using namespace ModulePaths;

static JS::Value module_string(JS::VM& vm, std::string const& text)
{
    return JS::PrimitiveString::create(
        vm, Utf16String::from_utf8_with_replacement_character(view(text)));
}

std::vector<fs::path> ModuleLoader::node_module_paths(fs::path directory)
{
    std::error_code error;
    directory = fs::absolute(directory, error).lexically_normal();
    std::vector<fs::path> paths;
    if (error)
        return paths;
    while (!directory.empty()) {
        if (directory.filename() != "node_modules")
            paths.push_back(directory / "node_modules");
        auto parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }

    return paths;
}

JS::ThrowCompletionOr<std::vector<fs::path>> ModuleLoader::lookup_paths(JS::Value value)
{
    if (!value.is_object() || !JS::Array::is_engine_class_of(value.as_object()))
        return error("ERR_INVALID_ARG_VALUE", "Module lookup paths must be an array", true);
    GC::Root array { value.as_object() };
    auto length = TRY(array->get(m_vm.names.length)).as_double();
    if (length > 1024)
        return error("ERR_MITE_MODULE_PATH_LIMIT", "Too many module lookup paths");
    std::vector<fs::path> paths;
    for (u32 index = 0; index < length; ++index) {
        auto item = TRY(array->get(index));
        if (!item.is_string())
            return error("ERR_INVALID_ARG_TYPE", "Module lookup paths must be strings", true);
        auto text = string(MUST(item.as_string().utf16_string_view().to_byte_string()));
        if (text.find('\0') != std::string::npos)
            return error("ERR_INVALID_ARG_VALUE", "Module lookup paths must not contain NUL", true);
        paths.push_back(path_from_utf8(text));
    }

    return paths;
}

JS::ThrowCompletionOr<void> ModuleLoader::sync_commonjs_cache()
{
    GC::Root constructor { TRY(builtin("module")) };
    auto cache = TRY(constructor->get("_cache"_utf16));
    if (!cache.is_object())
        return error("ERR_INVALID_ARG_TYPE", "Module._cache must be an object", true);
    m_commonjs_cache = &cache.as_object();
    return {};
}

JS::ThrowCompletionOr<GC::Ref<JS::Object>> ModuleLoader::make_commonjs_module(
    fs::path const& filename, GC::Ptr<JS::Object> parent, bool main)
{
    GC::Root constructor { TRY(builtin("module")) };
    auto prototype_value = TRY(constructor->get("prototype"_utf16));
    GC::Root<JS::Object> prototype;
    if (prototype_value.is_object())
        prototype = &prototype_value.as_object();
    GC::Root module { JS::Object::create(m_realm, prototype.ptr()) };
    auto exports = JS::Object::create(m_realm, m_realm.intrinsics().object_prototype());
    module->define_direct_property("exports"_utf16, exports, JS::default_attributes);
    module->define_direct_property(
        "filename"_utf16, module_string(m_vm, path_string(filename)), JS::default_attributes);
    module->define_direct_property("id"_utf16,
        module_string(m_vm, main ? "." : path_string(filename)), JS::default_attributes);
    module->define_direct_property("path"_utf16,
        module_string(m_vm, path_string(filename.parent_path())), JS::default_attributes);
    module->define_direct_property(
        "parent"_utf16, parent ? JS::Value(parent) : JS::js_null(), JS::default_attributes);
    module->define_direct_property(
        "children"_utf16, TRY(JS::Array::create(m_realm, 0)), JS::default_attributes);
    module->define_direct_property("loaded"_utf16, JS::Value(false), JS::default_attributes);
    GC::RootVector<JS::Value> search_paths;
    for (auto const& path : node_module_paths(filename.parent_path()))
        search_paths.append(module_string(m_vm, path_string(path)));

    module->define_direct_property(
        "paths"_utf16, JS::Array::create_from(m_realm, search_paths), JS::default_attributes);
    return *module;
}

void ModuleLoader::install_module_utilities(JS::Object& host)
{
    install_resolve_hook_host(host);
    host.define_direct_property("cache"_utf16, m_commonjs_cache.ptr(), JS::default_attributes);
    host.define_native_function(m_realm, "setModuleConstructor"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& self = owner.get();
                if (self.m_commonjs_constructor || !vm.argument(0).is_function())
                    return self.error(
                        "ERR_INVALID_ARG_TYPE", "Module constructor is installed once", true);
                self.m_commonjs_constructor = &vm.argument(0).as_object();
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host.define_native_function(m_realm, "nodeModulePaths"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& self = owner.get();
                if (!vm.argument(0).is_string())
                    return self.error("ERR_INVALID_ARG_TYPE", "Module path must be a string", true);
                auto text
                    = string(MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string()));
                if (text.find('\0') != std::string::npos)
                    return self.error("ERR_INVALID_ARG_VALUE", "Module path contains NUL", true);
                GC::RootVector<JS::Value> paths;
                for (auto const& path : node_module_paths(path_from_utf8(text)))
                    paths.append(module_string(vm, path_string(path)));
                return JS::Array::create_from(self.m_realm, paths).ptr();
            }),
        1, JS::default_attributes);
    host.define_native_function(m_realm, "resolveFilename"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& self = owner.get();
                if (!vm.argument(0).is_string() || !vm.argument(1).is_string())
                    return self.error("ERR_INVALID_ARG_TYPE",
                        "Resolution needs request and parent filename", true);
                auto request
                    = string(MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string()));
                auto parent = path_from_utf8(
                    string(MUST(vm.argument(1).as_string().utf16_string_view().to_byte_string())));
                auto paths = TRY(self.lookup_paths(vm.argument(2)));
                auto resolved = self.resolve_require_hooked(request, parent, &paths);
                if (resolved.is_error())
                    return resolved.release_error();
                // Resolution reports an unknown reserved builtin as not found;
                // prefixed loading must instead reach builtin()'s own diagnostic.
                // The private fourth argument distinguishes that loader operation
                // without catching/replacing any exception from a resolve hook.
                if (!vm.argument(3).to_boolean() && !resolved.value().builtin.empty()
                    && !known_builtin(resolved.value().builtin))
                    return self.error("MODULE_NOT_FOUND",
                        "Cannot resolve unknown builtin: " + resolved.value().builtin);
                return module_string(vm, resolved.value().identifier);
            }),
        3, JS::default_attributes);
    host.define_native_function(m_realm, "loadResolved"_utf16,
        GC::create_function(m_vm.heap(), GC::captures(native_reference(*this)),
            [](auto& owner, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto& self = owner.get();
                if (!vm.argument(0).is_string())
                    return self.error(
                        "ERR_INVALID_ARG_TYPE", "Resolved module filename must be a string", true);
                auto name
                    = string(MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string()));
                GC::Root<JS::Object> parent;
                if (vm.argument(1).is_object())
                    parent = &vm.argument(1).as_object();
                else if (!vm.argument(1).is_nullish())
                    return self.error(
                        "ERR_INVALID_ARG_TYPE", "Module parent must be an object or null", true);
                if (self.m_host_modules.contains(name))
                    return self.m_host_modules.at(name).exports.ptr();
                bool prefix = name.starts_with("node:");
                auto builtin = prefix ? name.substr(5) : name;
                if (prefix || bare_builtin(name)) {
                    if (!prefix) {
                        TRY(self.sync_commonjs_cache());
                        auto cached = TRY(self.m_commonjs_cache->get(
                            Utf16String::from_utf8_with_replacement_character(view(name))));
                        if (cached.is_object())
                            return cached.as_object().get("exports"_utf16);
                    }

                    return TRY(self.builtin(builtin));
                }

                if (view(name).starts_with("data:"sv, CaseSensitivity::CaseInsensitive))
                    return self.error("ERR_MITE_UNSUPPORTED_REQUIRE_ESM",
                        "Synchronous require of data ES modules is not implemented yet");
                auto filename = path_from_utf8(name);
                if (!filename.is_absolute() || name.find('\0') != std::string::npos)
                    return self.error("ERR_INVALID_ARG_VALUE",
                        "Resolved module filename must be absolute without NUL", true);
                return self.load_commonjs(filename, parent.ptr(), vm.argument(2).to_boolean());
            }),
        3, JS::default_attributes);
    host.define_native_function(m_realm, "createRequire"_utf16,
        GC::create_function(m_realm.vm().heap(), GC::captures(native_reference(*this)),
            [](auto& captured_self, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto* self = &captured_self.get();

                if (!vm.argument(0).is_string())
                    return self->error("ERR_INVALID_ARG_VALUE",
                        "createRequire requires an absolute filename", true);
                auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
                auto text = string(bytes);
                auto filename = path_from_utf8(text);
                bool absolute = filename.is_absolute();
#ifdef _WIN32
                // Node also accepts drive-rooted paths such as \\app\\main.cjs,
                // resolved by Windows against the current drive.
                absolute = filename.has_root_directory();
#endif
                if (!absolute || text.find('\0') != std::string::npos)
                    return self->error("ERR_INVALID_ARG_VALUE",
                        "createRequire requires an absolute filename without NUL", true);
                // A trailing separator denotes a directory context, even if it does not
                // exist. Do not canonicalize or require an actual parent file on disk.
                bool directory = text.ends_with('/');
#ifdef _WIN32
                directory = directory || text.ends_with('\\');
#endif
                if (directory)
                    filename /= "noop.js";
                TRY(self->sync_commonjs_cache());
                GC::Root module { TRY(self->make_commonjs_module(filename, nullptr, false)) };
                return self->make_require(filename, *module);
            }),
        1, JS::default_attributes);
}
}
