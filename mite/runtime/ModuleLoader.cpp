#include "ModuleLoader.h"

#include <AK/JsonObject.h>
#include <AK/JsonValue.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PromiseCapability.h>
#include <LibJS/SyntheticModule.h>
#include <LibURL/Parser.h>
#include <cstring>
#include <fstream>
#include <iterator>

namespace Mite {
namespace fs = std::filesystem;

static StringView view(std::string const& value)
{
    return { value.data(), value.size() };
}

static std::string string(StringView value)
{
    return { value.characters_without_null_termination(), value.length() };
}

static std::string path_string(fs::path const& path)
{
    auto utf8 = path.generic_u8string();
    return { reinterpret_cast<char const*>(utf8.data()), utf8.size() };
}

static fs::path url_path(URL::URL const& url)
{
    auto path = url.file_path();
    return fs::path(std::u8string(reinterpret_cast<char8_t const*>(path.characters()), path.length()));
}

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
{
    m_vm.host_load_imported_module = [this](JS::ImportedModuleReferrer referrer, JS::ModuleRequest const& request,
                                        GC::Ptr<GC::Cell>, JS::ImportedModulePayload payload) {
        auto load_request = [&]() -> JS::ThrowCompletionOr<GC::Ref<JS::Module>> {
            auto bytes = MUST(request.module_specifier.view().to_byte_string());
            auto url = TRY(resolve(string(bytes), referrer_url(referrer)));
            bool json_type = false;
            for (auto const& attribute : request.attributes) {
                if (attribute.key != "type"_utf16 || attribute.value != "json"_utf16)
                    return error("ERR_IMPORT_ATTRIBUTE_UNSUPPORTED", "Unsupported import attribute", true);
                json_type = true;
            }
            bool json_file = url_path(url).extension() == ".json";
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
        auto found = m_urls.find(&module);
        VERIFY(found != m_urls.end());
        auto url = URL::Parser::basic_parse(view(found->second));
        VERIFY(url.has_value());
        auto path = url_path(*url);
        auto add = [&](JS::PropertyKey const& name, std::string const& value) {
            properties.set(name, JS::PrimitiveString::create(m_vm, Utf16String::from_utf8_with_replacement_character(view(value))));
        };
        add("url"_utf16, found->second);
        add("filename"_utf16, path_string(path));
        add("dirname"_utf16, path_string(path.parent_path()));
        properties.set("main"_utf16, JS::Value(found->second == m_entry_url));
        return properties;
    };
}

void ModuleLoader::register_script(JS::Script& script)
{
    m_scripts.append(script);
    m_urls.emplace(&script, m_entry_url);
}

std::string ModuleLoader::referrer_url(JS::ImportedModuleReferrer const& referrer) const
{
    return referrer.visit([&](auto const& record) {
        auto found = m_urls.find(record.ptr());
        return found == m_urls.end() ? m_entry_url : found->second;
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
        auto config = directory / "package.json";
        std::error_code filesystem_error;
        bool exists = fs::exists(config, filesystem_error);
        if (filesystem_error)
            return error("ERR_INVALID_PACKAGE_CONFIG", "Cannot inspect " + path_string(config));
        if (exists) {
            std::ifstream input(config, std::ios::binary);
            if (!input)
                return error("ERR_INVALID_PACKAGE_CONFIG", "Cannot read " + path_string(config));
            std::string source(std::istreambuf_iterator<char>(input), {});
            if (input.bad())
                return error("ERR_INVALID_PACKAGE_CONFIG", "Cannot read " + path_string(config));
            auto json = AK::JsonValue::from_string(view(source));
            if (json.is_error() || !json.value().is_object())
                return error("ERR_INVALID_PACKAGE_CONFIG", "Invalid package config " + path_string(config));
            auto type = json.value().as_object().get_string("type"sv);
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
        return error("ERR_MODULE_NOT_FOUND", "Cannot resolve package '" + specifier + "'; package resolution is not implemented yet");
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
    auto path = url_path(url);
    auto extension = path.extension();
    if (extension != ".mjs" && extension != ".js" && extension != ".cjs" && extension != ".json")
        return error("ERR_UNKNOWN_FILE_EXTENSION", "Unknown module file extension: " + path_string(extension), true);
    if (extension != ".json" && !TRY(is_es_module(path)))
        return error("ERR_MITE_UNSUPPORTED_MODULE_FORMAT", "CommonJS module interoperation is not implemented yet: " + path_string(path));
    std::ifstream input(path, std::ios::binary);
    if (!input)
        return error("ERR_MODULE_NOT_FOUND", "Cannot read module: " + path_string(path));
    std::string source(std::istreambuf_iterator<char>(input), {});
    if (input.bad())
        return error("ERR_MODULE_NOT_FOUND", "Error reading module: " + path_string(path));
    auto text = Utf16String::from_utf8_with_replacement_character(view(source));
    auto parse_source = [&]() -> JS::ThrowCompletionOr<GC::Ref<JS::Module>> {
        if (extension == ".json")
            return TRY(JS::parse_json_module(m_realm, text, ByteString(view(identity))));
        auto parsed = JS::SourceTextModule::parse(text, m_realm, view(identity));
        if (parsed.is_error())
            return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
        return parsed.release_value();
    };
    auto module = TRY(parse_source());
    m_modules.emplace(identity, GC::Root<JS::Module> { module.ptr() });
    m_urls.emplace(module.ptr(), identity);
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

}
