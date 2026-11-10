/** Create ESM-owned CommonJS contexts without caching or executing the synthetic parent. */
#include "core/GCCaptures.h"
#include "modules/ModuleLoader.h"
#include "modules/ModulePaths.h"
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

JS::ThrowCompletionOr<GC::Ref<JS::Object>> ModuleLoader::make_commonjs_module(
    fs::path const& filename, GC::Ptr<JS::Object> parent, bool main)
{
    GC::Root module { JS::Object::create(m_realm, m_realm.intrinsics().object_prototype()) };
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
    auto directory = filename.parent_path();
    while (!directory.empty()) {
        if (directory.filename() != "node_modules")
            search_paths.append(module_string(m_vm, path_string(directory / "node_modules")));
        auto parent_directory = directory.parent_path();
        if (parent_directory == directory)
            break;
        directory = parent_directory;
    }

    module->define_direct_property(
        "paths"_utf16, JS::Array::create_from(m_realm, search_paths), JS::default_attributes);
    return *module;
}

void ModuleLoader::install_module_utilities(JS::Object& host)
{
    install_resolve_hook_host(host);
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
                GC::Root module { TRY(self->make_commonjs_module(filename, nullptr, false)) };
                return self->make_require(filename, *module);
            }),
        1, JS::default_attributes);
}
}
