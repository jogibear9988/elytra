#include "ModuleLoader.h"
#include "ModulePaths.h"
#include "ModuleSource.h"
#include "BuiltinNames.h"

#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/JSONObject.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PromiseCapability.h>
#include <LibJS/SyntheticModule.h>
#include <LibURL/Parser.h>
#include <cstring>
#include <fstream>
#include <iterator>

namespace Mite {
namespace fs = std::filesystem;

GC_DEFINE_ALLOCATOR(ModuleSource);

using namespace ModulePaths;

URL::URL ModuleLoader::path_to_url(fs::path const& path)
{
    // Protect path characters which URL parsing would otherwise interpret as
    // delimiters, escapes, backslashes or stripped whitespace.
    std::string encoded;
    for (unsigned char ch : path_string(path)) {
        if (ch == '%' || ch == '#' || ch == '?' || ch == '\\' || ch <= 0x20 || ch == 0x7f) {
            static constexpr char hex[] = "0123456789ABCDEF";
            encoded += '%';
            encoded += hex[ch >> 4];
            encoded += hex[ch & 15];
        } else {
            encoded += static_cast<char>(ch);
        }
    }
    if (encoded.empty() || encoded[0] != '/')
        encoded.insert(0, "/"); // Native Windows drive path.
    auto result = URL::Parser::basic_parse(view("file://" + encoded));
    VERIFY(result.has_value());
    return result.release_value();
}

JS::Completion ModuleLoader::error(char const* code, std::string const& message, bool type_error)
{
    auto text = Utf16String::from_utf8_with_replacement_character(view(message));
    GC::Ref<JS::Error> exception = type_error
        ? JS::TypeError::create(m_realm, text)
        : JS::Error::create(m_realm, text);
    exception->define_direct_property("code"_utf16,
        JS::PrimitiveString::create(m_vm, Utf16String::from_utf8(StringView(code, strlen(code)))), JS::default_attributes);
    return JS::throw_completion(exception);
}

ModuleLoader::ModuleLoader(JS::VM& vm, JS::Realm& realm, fs::path entry_path)
    : m_vm(vm)
    , m_realm(realm)
    , m_entry_path(std::move(entry_path))
    , m_entry_url(string(path_to_url(m_entry_path).serialize()))
    , m_promise_constructor(realm.global_object().get_without_side_effects("Promise"_utf16).as_object())
    , m_process(realm.global_object().get_without_side_effects("process"_utf16).as_object())
    , m_commonjs_cache(JS::Object::create(realm, nullptr))
{
    m_vm.host_load_imported_module = [this](JS::ImportedModuleReferrer referrer, JS::ModuleRequest const& request,
                                        GC::Ptr<GC::Cell>, JS::ImportedModulePayload payload) {
        auto load_request = [&]() -> JS::ThrowCompletionOr<GC::Ref<JS::Module>> {
            auto bytes = MUST(request.module_specifier.view().to_byte_string());
            if (bytes == "mite:internal/cjs"sv) {
                bool shim = referrer.visit([&](auto const& record) { return m_commonjs_shims.contains(record.ptr()); });
                if (shim && request.attributes.is_empty())
                    return *m_commonjs_bridge;
            }
            if (bytes == "mite:internal/builtin"sv) {
                bool shim = referrer.visit([&](auto const& record) { return m_builtin_shims.contains(record.ptr()); });
                if (shim && request.attributes.is_empty())
                    return *m_builtin_bridge;
            }
            auto url = TRY(resolve(string(bytes), referrer_url(referrer)));
            bool json_type = false;
            for (auto const& attribute : request.attributes) {
                if (attribute.key != "type"_utf16 || attribute.value != "json"_utf16)
                    return error("ERR_IMPORT_ATTRIBUTE_UNSUPPORTED", "Unsupported import attribute", true);
                json_type = true;
            }
            bool json_file = url.scheme() == "file"sv && url_path(url).extension() == ".json";
            if (json_file && !json_type)
                return error("ERR_IMPORT_ATTRIBUTE_MISSING", "JSON modules require type: json", true);
            if (!json_file && json_type)
                return error("ERR_IMPORT_ATTRIBUTE_TYPE_INCOMPATIBLE", "Module is not of type json", true);
            return load(url);
        };
        auto result = load_request();
        JS::finish_loading_imported_module(referrer, request, payload, result);
    };
    m_vm.host_get_import_meta_properties = [this](JS::SourceTextModule& module) {
        GC::RootHashMap<JS::PropertyKey, JS::Value> properties;
        auto* source = static_cast<ModuleSource*>(module.host_defined().ptr());
        VERIFY(source);
        auto url = URL::Parser::basic_parse(view(source->url));
        VERIFY(url.has_value());
        auto path = url_path(*url);
        auto add = [&](JS::PropertyKey const& name, std::string const& value) {
            properties.set(name, JS::PrimitiveString::create(m_vm, Utf16String::from_utf8_with_replacement_character(view(value))));
        };
        add("url"_utf16, source->url);
        add("filename"_utf16, path_string(path));
        add("dirname"_utf16, path_string(path.parent_path()));
        properties.set("main"_utf16, JS::Value(source->url == m_entry_url));
        return properties;
    };
}

JS::ThrowCompletionOr<JS::Value> ModuleLoader::run_eval(std::string const& source)
{
    GC::Root metadata { m_vm.heap().allocate<ModuleSource>(m_entry_url) };
    auto text = Utf16String::from_utf8_with_replacement_character(view(source));
    auto parsed = JS::Script::parse(text, m_realm, "[eval]"sv, {}, metadata.ptr());
    if (parsed.is_error())
        return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
    return m_vm.run(*parsed.release_value());
}

std::string ModuleLoader::referrer_url(JS::ImportedModuleReferrer const& referrer) const
{
    return referrer.visit([&](auto const& record) {
        auto metadata = record->host_defined();
        if (metadata && metadata->class_name() == "ModuleSource"sv)
            return static_cast<ModuleSource*>(metadata.ptr())->url;
        return m_entry_url;
    });
}

JS::ThrowCompletionOr<bool> ModuleLoader::is_es_module(fs::path const& path)
{
    std::error_code canonical_error;
    auto actual_path = fs::canonical(path, canonical_error);
    if (canonical_error)
        actual_path = path;
    auto extension = actual_path.extension();
    if (extension == ".mjs")
        return true;
    if (extension == ".cjs")
        return false;
    auto directory = actual_path.parent_path();
    while (!directory.empty()) {
        if (directory.filename() == "node_modules")
            break;
        auto config = TRY(read_package(directory));
        if (config.has_value()) {
            auto type = config->as_object().get_string("type"sv);
            return type.has_value() && *type == "module"sv;
        }
        auto parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }
    return false;
}

JS::ThrowCompletionOr<URL::URL> ModuleLoader::resolve(std::string const& specifier, std::string const& referrer)
{
    auto base = URL::Parser::basic_parse(view(referrer));
    VERIFY(base.has_value());
    bool relative = specifier.starts_with("./") || specifier.starts_with("../") || specifier.starts_with("/");
    auto url = relative ? URL::Parser::basic_parse(view(specifier), *base) : URL::Parser::basic_parse(view(specifier));
    if (!url.has_value())
        url = TRY(resolve_package(specifier, url_path(*base), false));
    if (url->scheme() == "node"sv)
        return url.release_value();
    if (url->scheme() != "file"sv)
        return error("ERR_UNSUPPORTED_ESM_URL_SCHEME", "Unsupported module URL scheme: " + string(url->scheme()));
    if (!url->serialized_host().is_empty())
        return error("ERR_INVALID_FILE_URL_HOST", "Only local file module URLs are currently supported", true);
    auto encoded_path = url->serialize_path();
    if (encoded_path.contains("%2f"sv, CaseSensitivity::CaseInsensitive) || encoded_path.contains("%5c"sv, CaseSensitivity::CaseInsensitive))
        return error("ERR_INVALID_MODULE_SPECIFIER", "Encoded path separators are not allowed in module URLs", true);
    auto bytes = url->file_path();
    if (bytes.contains('\0'))
        return error("ERR_INVALID_ARG_VALUE", "Module paths must not contain null bytes", true);
    auto path = url_path(*url);
    std::error_code filesystem_error;
    auto status = fs::status(path, filesystem_error);
    if (fs::is_directory(status))
        return error("ERR_UNSUPPORTED_DIR_IMPORT", "Directory import is not supported: " + path_string(path));
    if (filesystem_error || !fs::is_regular_file(status))
        return error("ERR_MODULE_NOT_FOUND", "Cannot find module: " + path_string(path));
    auto canonical = fs::canonical(path, filesystem_error);
    if (filesystem_error)
        return error("ERR_MODULE_NOT_FOUND", "Cannot resolve module path: " + path_string(path));
    auto canonical_url = path_to_url(canonical);
    canonical_url.set_query(url->query());
    canonical_url.set_fragment(url->fragment());
    return canonical_url;
}

JS::ThrowCompletionOr<GC::Ref<JS::Module>> ModuleLoader::load(URL::URL const& url)
{
    auto identity = string(url.serialize());
    if (auto found = m_modules.find(identity); found != m_modules.end())
        return *found->second;
    if (url.scheme() == "node"sv)
        return builtin_module(identity.substr(5));
    auto path = url_path(url);
    auto extension = path.extension();
    if (extension != ".mjs" && extension != ".js" && extension != ".cjs" && extension != ".json")
        return error("ERR_UNKNOWN_FILE_EXTENSION", "Unknown module file extension: " + path_string(extension), true);
    if (extension != ".json" && !TRY(is_es_module(path))) {
        if (!m_commonjs_bridge) {
            auto bridge = JS::NativeFunction::create(m_realm, [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_string())
                    return error("ERR_INVALID_ARG_TYPE", "Module filename must be a string", true);
                auto bytes = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
                auto name = std::u8string(reinterpret_cast<char8_t const*>(bytes.characters()), bytes.length());
                return load_commonjs(fs::path(name));
            }, 1, "loadCommonJS"_utf16);
            m_commonjs_bridge = JS::SyntheticModule::create_default_export_synthetic_module(m_realm, bridge, "mite:internal/cjs").ptr();
        }
        auto quoted = AK::JsonValue(view(path_string(path))).serialized();
        auto shim = MUST(String::formatted("import load from 'mite:internal/cjs'; const value = load({}); export {{ value as default, value as 'module.exports' }};", quoted));
        GC::Root metadata { m_vm.heap().allocate<ModuleSource>(identity) };
        auto parsed = JS::SourceTextModule::parse(Utf16String::from_utf8(shim), m_realm, view(identity), {}, metadata.ptr());
        VERIFY(!parsed.is_error());
        auto module = parsed.release_value();
        m_modules.emplace(identity, GC::Root<JS::Module> { module.ptr() });
        m_commonjs_shims.insert(module.ptr());
        return module;
    }
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return error("ERR_MODULE_NOT_FOUND", "Cannot read module: " + path_string(path));
    std::string source(std::istreambuf_iterator<char>(input), {});
    if (input.bad())
        return error("ERR_MODULE_NOT_FOUND", "Error reading module: " + path_string(path));
    auto text = Utf16String::from_utf8_with_replacement_character(view(source));
    auto parse_source = [&]() -> JS::ThrowCompletionOr<GC::Ref<JS::Module>> {
        if (extension == ".json") {
            if (!url.query().has_value() && !url.fragment().has_value()) {
                auto value = TRY(load_commonjs(path));
                return JS::SyntheticModule::create_default_export_synthetic_module(m_realm, value, ByteString(view(identity)));
            }
            return TRY(JS::parse_json_module(m_realm, text, ByteString(view(identity))));
        }
        GC::Root metadata { m_vm.heap().allocate<ModuleSource>(identity) };
        auto parsed = JS::SourceTextModule::parse(text, m_realm, view(identity), {}, metadata.ptr());
        if (parsed.is_error())
            return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
        return parsed.release_value();
    };
    auto module = TRY(parse_source());
    m_modules.emplace(identity, GC::Root<JS::Module> { module.ptr() });
    return module;
}

JS::ThrowCompletionOr<GC::Ref<JS::Promise>> ModuleLoader::run_entry()
{
    auto url = TRY(resolve(m_entry_url, m_entry_url));
    m_entry_url = string(url.serialize());
    auto module = TRY(load(url));
    GC::Root loading { module->load_requested_modules(nullptr).promise() };
    auto result = TRY(JS::new_promise_capability(m_vm, m_promise_constructor.ptr()));
    GC::Root result_root { result };
    auto execute = JS::NativeFunction::create(m_realm, [module](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        TRY(module->link(vm));
        auto evaluation = TRY(module->evaluate(vm));
        return evaluation->promise();
    }, 0, "evaluateEntry"_utf16);
    static_cast<JS::Promise&>(*loading).perform_then(execute, JS::js_undefined(), result);
    return static_cast<JS::Promise&>(*result->promise());
}

static GC::Ref<JS::PrimitiveString> js_string(JS::VM& vm, std::string const& text)
{
    return JS::PrimitiveString::create(vm, Utf16String::from_utf8_with_replacement_character(view(text)));
}

JS::ThrowCompletionOr<Optional<AK::JsonValue>> ModuleLoader::read_package(fs::path const& directory)
{
    auto file = directory / "package.json";
    std::error_code ec;
    if (!fs::exists(file, ec) && !ec)
        return Optional<AK::JsonValue> {};
    if (ec)
        return error("ERR_INVALID_PACKAGE_CONFIG", "Cannot inspect " + path_string(file));
    std::ifstream input(file, std::ios::binary);
    if (!input)
        return error("ERR_INVALID_PACKAGE_CONFIG", "Cannot read " + path_string(file));
    std::string source(std::istreambuf_iterator<char>(input), {});
    auto parsed = AK::JsonValue::from_string(view(source));
    if (input.bad() || parsed.is_error() || !parsed.value().is_object())
        return error("ERR_INVALID_PACKAGE_CONFIG", "Invalid package config " + path_string(file));
    return Optional<AK::JsonValue> { parsed.release_value() };
}

JS::ThrowCompletionOr<Optional<fs::path>> ModuleLoader::legacy_path(fs::path const& candidate)
{
    auto file = [](fs::path const& path) -> Optional<fs::path> {
        std::error_code ec;
        if (!fs::is_regular_file(path, ec))
            return {};
        auto canonical = fs::canonical(path, ec);
        if (ec)
            return {};
        return canonical;
    };
    auto with_extensions = [&](fs::path const& path) -> Optional<fs::path> {
        if (auto found = file(path); found.has_value())
            return found;
        for (auto extension : { ".js", ".json", ".node" }) {
            auto extended = path;
            extended += extension;
            if (auto found = file(extended); found.has_value())
                return found;
        }
        return {};
    };
    if (auto found = with_extensions(candidate); found.has_value())
        return found;
    std::error_code ec;
    if (!fs::is_directory(candidate, ec))
        return Optional<fs::path> {};
    auto config = TRY(read_package(candidate));
    if (config.has_value()) {
        auto main = config->as_object().get_string("main"sv);
        if (main.has_value() && !main->is_empty()) {
            auto bytes = main->bytes_as_string_view();
            auto path = candidate / fs::path(std::u8string(reinterpret_cast<char8_t const*>(bytes.characters_without_null_termination()), bytes.length()));
            if (auto found = with_extensions(path); found.has_value())
                return found;
            if (auto found = with_extensions(path / "index"); found.has_value())
                return found;
        }
    }
    return with_extensions(candidate / "index");
}

JS::ThrowCompletionOr<ModuleLoader::RequireResolution> ModuleLoader::resolve_require(std::string const& request, fs::path const& parent)
{
    if (request.empty())
        return error("ERR_INVALID_ARG_VALUE", "Module ID must not be empty", true);
    if (request.find('\0') != std::string::npos)
        return error("MODULE_NOT_FOUND", "Module ID contains a null byte");
    auto path = fs::path(std::u8string(reinterpret_cast<char8_t const*>(request.data()), request.size()));
    bool relative = request == "." || request == ".." || request.starts_with("./") || request.starts_with("../");
    if (!relative && !path.is_absolute()) {
        auto url = TRY(resolve_package(request, parent, true));
        if (url.scheme() == "node"sv) {
            auto name = string(url.serialize()).substr(5);
            return RequireResolution { {}, name, request == name ? name : "node:" + name };
        }
        if (url.scheme() != "file"sv)
            return error("ERR_UNSUPPORTED_ESM_URL_SCHEME", "Unsupported module URL scheme");
        auto encoded = url.serialize_path();
        if (encoded.contains("%2f"sv, CaseSensitivity::CaseInsensitive) || encoded.contains("%5c"sv, CaseSensitivity::CaseInsensitive))
            return error("ERR_INVALID_MODULE_SPECIFIER", "Encoded separators are not allowed in package targets", true);
        if (url.file_path().contains('\0'))
            return error("ERR_INVALID_ARG_VALUE", "Module paths must not contain null bytes", true);
        auto resolved = url_path(url);
        std::error_code ec;
        if (!fs::is_regular_file(resolved, ec))
            return error("MODULE_NOT_FOUND", "Cannot find module '" + request + "'");
        auto canonical = fs::canonical(resolved, ec);
        if (ec)
            return error("MODULE_NOT_FOUND", "Cannot resolve module '" + request + "'");
        return RequireResolution { canonical, {}, path_string(canonical) };
    }
    auto found = TRY(legacy_path(path.is_absolute() ? path : parent.parent_path() / path));
    if (!found.has_value())
        return error("MODULE_NOT_FOUND", "Cannot find module '" + request + "'");
    auto filename = found.release_value();
    return RequireResolution { filename, {}, path_string(filename) };
}

JS::ThrowCompletionOr<void> ModuleLoader::add_child(JS::Object& parent, JS::Object& child)
{
    auto children = TRY(parent.get("children"_utf16));
    if (!children.is_object() || !JS::Array::is_engine_class_of(children.as_object()))
        return {};
    auto& array = children.as_object();
    auto length = TRY(array.get(m_vm.names.length));
    for (u32 index = 0; index < length.as_double(); ++index) {
        auto existing = TRY(array.get(index));
        if (existing.is_object() && &existing.as_object() == &child)
            return {};
    }
    TRY(array.create_data_property_or_throw(static_cast<u32>(length.as_double()), &child));
    return {};
}

JS::ThrowCompletionOr<void> ModuleLoader::remove_child(JS::Object& parent, JS::Object& child)
{
    auto children = TRY(parent.get("children"_utf16));
    if (!children.is_object() || !JS::Array::is_engine_class_of(children.as_object()))
        return {};
    auto& array = children.as_object();
    auto length = TRY(array.get(m_vm.names.length));
    u32 output = 0;
    for (u32 index = 0; index < length.as_double(); ++index) {
        auto value = TRY(array.get(index));
        if (value.is_object() && &value.as_object() == &child)
            continue;
        TRY(array.set(output++, value, JS::Object::ShouldThrowExceptions::Yes));
    }
    TRY(array.set(m_vm.names.length, JS::Value(output), JS::Object::ShouldThrowExceptions::Yes));
    return {};
}

JS::Completion ModuleLoader::require_error(JS::Completion completion, JS::Object& module)
{
    auto exception = completion.value();
    if (!exception.is_object())
        return completion;
    auto code = exception.as_object().get_without_side_effects("code"_utf16);
    if (!code.is_string() || code.as_string().utf16_string_view() != "MODULE_NOT_FOUND"_utf16)
        return completion;
    GC::RootVector<JS::Value> stack;
    std::unordered_set<JS::Object*> visited;
    auto* current = &module;
    while (current && visited.insert(current).second) {
        auto filename = current->get_without_side_effects("filename"_utf16);
        if (filename.is_string())
            stack.append(filename);
        auto parent = current->get_without_side_effects("parent"_utf16);
        current = parent.is_object() ? &parent.as_object() : nullptr;
    }
    exception.as_object().define_direct_property("requireStack"_utf16, JS::Array::create_from(m_realm, stack), JS::default_attributes);
    return completion;
}

GC::Ref<JS::NativeFunction> ModuleLoader::make_require(fs::path const& filename, GC::Ref<JS::Object> module)
{
    auto require = JS::NativeFunction::create(m_realm, [this, filename, module](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        if (!vm.argument(0).is_string())
            return error("ERR_INVALID_ARG_TYPE", "Module ID must be a string", true);
        auto request = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
        auto resolved = resolve_require(string(request), filename);
        if (resolved.is_error())
            return require_error(resolved.release_error(), *module);
        auto target = resolved.release_value();
        if (!target.builtin.empty()) {
            if (!target.identifier.starts_with("node:")) {
                auto cached = TRY(m_commonjs_cache->get(Utf16String::from_utf8_with_replacement_character(view(target.identifier))));
                if (cached.is_object())
                    return cached.as_object().get("exports"_utf16);
            }
            return TRY(builtin(target.builtin));
        }
        return load_commonjs(target.path, module);
    }, 1, "require"_utf16);
    require->define_direct_property("cache"_utf16, m_commonjs_cache.ptr(), JS::default_attributes);
    require->define_direct_property("main"_utf16, m_main_module ? JS::Value(m_main_module.ptr()) : JS::js_undefined(), JS::default_attributes);
    require->define_native_function(m_realm, "resolve"_utf16, [this, filename, module](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        if (!vm.argument(0).is_string())
            return error("ERR_INVALID_ARG_TYPE", "Module ID must be a string", true);
        if (vm.argument_count() > 1 && !vm.argument(1).is_undefined())
            return error("ERR_MITE_UNSUPPORTED_API", "require.resolve options are not implemented yet");
        auto request = MUST(vm.argument(0).as_string().utf16_string_view().to_byte_string());
        auto resolved = resolve_require(string(request), filename);
        if (resolved.is_error())
            return require_error(resolved.release_error(), *module);
        auto target = resolved.release_value();
        if (!target.builtin.empty() && !known_builtin(target.builtin))
            return error("MODULE_NOT_FOUND", "Cannot resolve unknown builtin: " + target.builtin);
        return js_string(vm, target.identifier);
    }, 1, JS::default_attributes);
    return require;
}

JS::ThrowCompletionOr<JS::Value> ModuleLoader::load_commonjs(fs::path const& filename, GC::Ptr<JS::Object> parent, bool main)
{
    auto key = Utf16String::from_utf8_with_replacement_character(view(path_string(filename)));
    auto cached = TRY(m_commonjs_cache->get(key));
    if (cached.is_object()) {
        if (parent)
            TRY(add_child(*parent, cached.as_object()));
        return cached.as_object().get("exports"_utf16);
    }
    if (filename.extension() == ".node")
        return error("ERR_MITE_UNSUPPORTED_NATIVE_ADDON", "Native Node addons are not supported");
    bool json = filename.extension() == ".json";
    if (!json && TRY(is_es_module(filename)))
        return error("ERR_MITE_UNSUPPORTED_REQUIRE_ESM", "Synchronous require of ES modules is not implemented yet");
    GC::Root module { JS::Object::create(m_realm, m_realm.intrinsics().object_prototype()) };
    auto exports = JS::Object::create(m_realm, m_realm.intrinsics().object_prototype());
    module->define_direct_property("exports"_utf16, exports, JS::default_attributes);
    module->define_direct_property("filename"_utf16, js_string(m_vm, path_string(filename)), JS::default_attributes);
    module->define_direct_property("id"_utf16, js_string(m_vm, main ? "." : path_string(filename)), JS::default_attributes);
    module->define_direct_property("path"_utf16, js_string(m_vm, path_string(filename.parent_path())), JS::default_attributes);
    module->define_direct_property("parent"_utf16, parent ? JS::Value(parent) : JS::js_null(), JS::default_attributes);
    module->define_direct_property("children"_utf16, TRY(JS::Array::create(m_realm, 0)), JS::default_attributes);
    module->define_direct_property("loaded"_utf16, JS::Value(false), JS::default_attributes);
    GC::RootVector<JS::Value> search_paths;
    auto directory = filename.parent_path();
    while (!directory.empty()) {
        if (directory.filename() != "node_modules")
            search_paths.append(js_string(m_vm, path_string(directory / "node_modules")));
        auto parent_directory = directory.parent_path();
        if (parent_directory == directory)
            break;
        directory = parent_directory;
    }
    module->define_direct_property("paths"_utf16, JS::Array::create_from(m_realm, search_paths), JS::default_attributes);
    if (main)
        m_main_module = module.ptr();
    auto require = make_require(filename, *module);
    module->define_direct_property("require"_utf16, require, JS::default_attributes);
    m_commonjs_cache->define_direct_property(key, module.ptr(), JS::default_attributes);
    // Publish before executing: cycles must observe the initial exports object.
    auto execute = [&]() -> JS::ThrowCompletionOr<JS::Value> {
        if (parent)
            TRY(add_child(*parent, *module));
        std::ifstream input(filename, std::ios::binary);
        if (!input)
            return error("MODULE_NOT_FOUND", "Cannot read module " + path_string(filename));
        std::string source(std::istreambuf_iterator<char>(input), {});
        if (input.bad())
            return error("MODULE_NOT_FOUND", "Error reading module " + path_string(filename));
        if (source.starts_with("\xef\xbb\xbf"))
            source.erase(0, 3);
        if (json) {
            auto value = TRY(JS::JSONObject::parse_json(m_vm, Utf16String::from_utf8_with_replacement_character(view(source))));
            module->define_direct_property("exports"_utf16, value, JS::default_attributes);
        } else {
            if (source.starts_with("#!")) {
                auto end = source.find('\n');
                source.replace(0, end == std::string::npos ? source.size() : end, "//");
            }
            auto wrapped = "(function(exports, require, module, __filename, __dirname) {\n" + source + "\n})";
            GC::Root metadata { m_vm.heap().allocate<ModuleSource>(string(path_to_url(filename).serialize())) };
            auto parsed = JS::Script::parse(Utf16String::from_utf8_with_replacement_character(view(wrapped)), m_realm,
                view(path_string(filename)), {}, metadata.ptr(), 0);
            if (parsed.is_error())
                return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
            auto script = parsed.release_value();
            auto function = TRY(m_vm.run(*script));
            GC::RootVector<JS::Value> arguments;
            arguments.append(exports);
            arguments.append(require);
            arguments.append(module.ptr());
            arguments.append(js_string(m_vm, path_string(filename)));
            arguments.append(js_string(m_vm, path_string(filename.parent_path())));
            TRY(JS::call(m_vm, function, exports, ReadonlySpan<JS::Value> { arguments.data(), arguments.size() }));
        }
        module->define_direct_property("loaded"_utf16, JS::Value(true), JS::default_attributes);
        return module->get("exports"_utf16);
    };
    auto result = execute();
    if (result.is_error()) {
        if (parent) {
            auto cleanup = remove_child(*parent, *module);
            (void)cleanup;
        }
        auto cleanup = m_commonjs_cache->internal_delete(key);
        (void)cleanup;
    }
    return result;
}

JS::ThrowCompletionOr<JS::Value> ModuleLoader::run_commonjs_entry()
{
    std::error_code ec;
    auto filename = fs::canonical(m_entry_path, ec);
    if (ec)
        return error("MODULE_NOT_FOUND", "Cannot resolve entry module");
    return load_commonjs(filename, nullptr, true);
}

}
