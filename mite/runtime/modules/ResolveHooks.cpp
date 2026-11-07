/**
 * Bridge realm-local synchronous resolve hooks to the existing native resolvers.
 * Callbacks capture the loader only; retained nextResolve functions never borrow a
 * native stack frame. The owning VM/loader roots the dispatcher and active results.
 */
#include "modules/ModuleLoader.h"
#include "modules/ModulePaths.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Intrinsics.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibURL/Parser.h>

namespace Mite {
using namespace ModulePaths;

static JS::Value hook_string(JS::VM& vm, std::string const& text)
{
    return JS::PrimitiveString::create(
        vm, Utf16String::from_utf8_with_replacement_character(view(text)));
}

static std::string hook_text(JS::Value value)
{
    VERIFY(value.is_string());
    return string(MUST(value.as_string().utf16_string_view().to_byte_string()));
}

JS::ThrowCompletionOr<JS::Value> ModuleLoader::run_resolve_hooks(
    std::string const& specifier, std::string const& parent, bool require, JS::Value attributes)
{
    if (!m_resolve_hook_dispatcher)
        return JS::js_undefined();
    return JS::call(m_vm, m_resolve_hook_dispatcher.ptr(), JS::js_undefined(),
        hook_string(m_vm, specifier), hook_string(m_vm, parent), JS::Value(require), attributes);
}

JS::ThrowCompletionOr<URL::URL> ModuleLoader::hook_result_url(
    JS::Object& result, std::string const& parent)
{
    auto value = TRY(result.get("url"_utf16));
    if (!value.is_string())
        return error(
            "ERR_INVALID_RETURN_PROPERTY_VALUE", "Resolve hook url must be a string", true);
    auto text = hook_text(value);
    auto parsed = URL::Parser::basic_parse(view(text));
    if (!parsed.has_value())
        return error("ERR_INVALID_URL", "Resolve hook must return an absolute module URL", true);
    // Registered host exports are allowed, but private shim schemes are never a
    // public entry to the internal CJS/builtin/host bridge modules.
    if (parsed->scheme() == "mite-host"sv) {
        if (!m_host_modules.contains(text.substr(10)))
            return error("ERR_UNSUPPORTED_ESM_URL_SCHEME", "Unregistered host module URL");
    } else {
        parsed = TRY(resolve(text, parent));
    }

    auto format = TRY(result.get("format"_utf16));
    if (!format.is_nullish()) {
        if (!format.is_string())
            return error(
                "ERR_INVALID_RETURN_PROPERTY_VALUE", "Resolve hook format must be a string", true);
        auto expected = std::string("builtin");
        if (parsed->scheme() == "file"sv) {
            auto path = url_path(*parsed);
            expected = path.extension() == ".json" ? "json"
                : TRY(is_es_module(path))          ? "module"
                                                   : "commonjs";
        }

        if (hook_text(format) != expected)
            return error("ERR_MITE_UNSUPPORTED_API",
                "Resolve format overrides require unsupported load hooks");
    }

    return parsed.release_value();
}

JS::ThrowCompletionOr<ModuleLoader::RequireResolution> ModuleLoader::resolve_require_hooked(
    std::string const& request, std::filesystem::path const& parent)
{
    auto parent_url = string(path_to_url(parent).serialize());
    auto value = TRY(run_resolve_hooks(request, parent_url, true, JS::js_undefined()));
    if (value.is_undefined())
        return resolve_require(request, parent);
    GC::Root result { value.as_object() };
    auto url = TRY(hook_result_url(*result, parent_url));
    auto text = string(url.serialize());
    if (url.scheme() == "mite-host"sv) {
        auto name = text.substr(10);
        return RequireResolution { {}, {}, name, name };
    }

    if (url.scheme() == "node"sv) {
        auto name = text.substr(5);
        return RequireResolution { {}, name, request == name ? name : "node:" + name, {} };
    }

    return resolve_require(path_string(url_path(url)), parent);
}

void ModuleLoader::install_resolve_hook_host(JS::Object& host)
{
    host.define_native_function(
        m_realm, "setResolveDispatcher"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_function() || m_resolve_hook_dispatcher)
                return error(
                    "ERR_INVALID_ARG_TYPE", "Resolve dispatcher must be installed once", true);
            m_resolve_hook_dispatcher = &vm.argument(0).as_object();
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        m_realm, "defaultResolve"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_string() || !vm.argument(1).is_object())
                return error(
                    "ERR_INVALID_ARG_TYPE", "Resolve requires a string and context object", true);
            auto& context = vm.argument(1).as_object();
            auto parent = TRY(context.get("parentURL"_utf16));
            if (!parent.is_string())
                return error(
                    "ERR_INVALID_ARG_TYPE", "Resolve parentURL must be a file URL string", true);
            auto parent_text = hook_text(parent);
            auto parent_url = URL::Parser::basic_parse(view(parent_text));
            if (!parent_url.has_value() || parent_url->scheme() != "file"sv)
                return error("ERR_MITE_UNSUPPORTED_API", "Only file parent URLs are implemented");
            // Apply the same file URL host/separator/NUL validation as createRequire.
            auto encoded = parent_url->serialize_path();
            if (!parent_url->serialized_host().is_empty() || parent_url->file_path().contains('\0')
                || encoded.contains("%2f"sv, CaseSensitivity::CaseInsensitive)
                || encoded.contains("%5c"sv, CaseSensitivity::CaseInsensitive))
                return error("ERR_INVALID_ARG_VALUE", "Invalid resolve parent file URL", true);
            auto conditions_value = TRY(context.get("conditions"_utf16));
            if (!conditions_value.is_object()
                || !JS::Array::is_engine_class_of(conditions_value.as_object()))
                return error("ERR_INVALID_ARG_TYPE", "Resolve conditions must be an array", true);
            GC::Root conditions_object { conditions_value.as_object() };
            auto length = TRY(conditions_object->get(vm.names.length)).as_double();
            if (length > 1024)
                return error("ERR_MITE_RESOLVE_HOOK_LIMIT", "Too many resolve conditions");
            std::vector<std::string> conditions;
            for (u32 index = 0; index < length; ++index) {
                auto condition = TRY(conditions_object->get(index));
                if (!condition.is_string())
                    return error(
                        "ERR_INVALID_ARG_TYPE", "Resolve conditions must be strings", true);
                conditions.push_back(hook_text(condition));
            }

            auto specifier = hook_text(vm.argument(0));
            bool require = vm.argument(2).to_boolean();
            Optional<URL::URL> url;
            if (require) {
                auto target = TRY(resolve_require(specifier, url_path(*parent_url), &conditions));
                url = !target.host.empty()
                    ? URL::Parser::basic_parse(view("mite-host:" + target.host))
                    : !target.builtin.empty()
                    ? URL::Parser::basic_parse(view("node:" + target.builtin))
                    : Optional<URL::URL> { path_to_url(target.path) };
            } else {
                url = TRY(resolve(specifier, parent_text, &conditions));
            }

            GC::Root result { JS::Object::create(
                m_realm, m_realm.intrinsics().object_prototype()) };
            result->define_direct_property(
                "url"_utf16, hook_string(vm, string(url->serialize())), JS::default_attributes);
            // Node omits a format hint for builtins/CommonJS require resolution.
            if (!require && url->scheme() == "file"sv) {
                auto path = url_path(*url);
                auto format = path.extension() == ".json" ? "json"
                    : TRY(is_es_module(path))             ? "module"
                                                          : "commonjs";
                result->define_direct_property(
                    "format"_utf16, hook_string(vm, format), JS::default_attributes);
            }

            return result.ptr();
        },
        3, JS::default_attributes);
}
}
