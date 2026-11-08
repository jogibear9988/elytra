/** Discover CommonJS exports without executing applications; snapshot values only at ESM
 * evaluation. */
#include "BuiltinSources.h"
#include "modules/ModuleLoader.h"
#include "modules/ModulePaths.h"
#include <AK/HashTable.h>
#include <AK/ScopeGuard.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibJS/Script.h>
#include <fstream>
#include <iterator>

namespace Mite {
using namespace ModulePaths;

// Quote UTF-16 code units rather than replacing malformed surrogates via UTF-8.
// Export names are source data, never executable generated identifiers.
std::string ModuleLoader::quote_export_name(Utf16String const& name)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string result = "\"";
    for (size_t i = 0; i < name.length_in_code_units(); ++i) {
        auto unit = name.code_unit_at(i);
        if (unit >= 0x20 && unit < 0x7f && unit != '"' && unit != '\\') {
            result += static_cast<char>(unit);
        } else {
            result += "\\u";
            for (int shift = 12; shift >= 0; shift -= 4)
                result += hex[(unit >> shift) & 15];
        }
    }

    result += '"';
    return result;
}

JS::ThrowCompletionOr<std::vector<Utf16String>> ModuleLoader::commonjs_export_names(
    std::filesystem::path const& filename, size_t depth)
{
    auto identity = path_string(filename);
    if (auto found = m_commonjs_export_names.find(identity); found != m_commonjs_export_names.end())
        return found->second;
    if (depth > 512)
        return error("ERR_MITE_MODULE_LIMIT", "CommonJS reexport analysis depth exceeded");
    if (!m_commonjs_lexer) {
        auto parsed = JS::Script::parse(Utf16String::from_utf8(StringView(MITE_CJS_LEXER_SOURCE,
                                            sizeof(MITE_CJS_LEXER_SOURCE) - 1)),
            m_realm, "mite:internal/cjs-lexer"sv);
        if (parsed.is_error())
            return m_vm.throw_completion<JS::SyntaxError>(parsed.error().first().to_utf16_string());
        GC::Root script { parsed.release_value() };
        GC::Root factory { TRY(m_vm.run(*script)).as_object() };
        GC::Root host { JS::Object::create(m_realm, nullptr) };
        auto parser = TRY(JS::call(m_vm, factory, JS::js_undefined(), JS::Value(host.ptr())));
        m_commonjs_lexer = parser.as_object();
    }

    std::ifstream input(filename, std::ios::binary);
    if (!input)
        return error("MODULE_NOT_FOUND", "Cannot read module " + identity);
    std::string source(std::istreambuf_iterator<char>(input), {});
    if (input.bad())
        return error("MODULE_NOT_FOUND", "Error reading module " + identity);
    if (source.starts_with("\xef\xbb\xbf"))
        source.erase(0, 3);
    GC::Root metadata { TRY(
        JS::call(m_vm, m_commonjs_lexer.ptr(), JS::js_undefined(),
            JS::PrimitiveString::create(
                m_vm, Utf16String::from_utf8_with_replacement_character(view(source)))))
            .as_object() };
    GC::Root exports { TRY(metadata->get("exports"_utf16)).as_object() };
    GC::Root reexports { TRY(metadata->get("reexports"_utf16)).as_object() };
    std::vector<Utf16String> names;
    HashTable<Utf16String> seen;
    auto count = TRY(JS::length_of_array_like(m_vm, *exports));
    for (u64 i = 0; i < count; ++i) {
        auto name = TRY(exports->get(i)).as_string().utf16_string();
        if (seen.set(name) == AK::HashSetResult::InsertedNewEntry)
            names.push_back(name);
    }
    // Publish direct names first to break cycles; do not retain references across
    // recursion because insertion into the native unordered_map can rehash it.
    m_commonjs_export_names.emplace(identity, names);
    bool complete = false;
    ScopeGuard rollback([&] {
        if (!complete)
            m_commonjs_export_names.erase(identity);
    });
    count = TRY(JS::length_of_array_like(m_vm, *reexports));
    for (u64 i = 0; i < count; ++i) {
        auto request = TRY(reexports->get(i)).as_string().utf16_string();
        auto bytes = MUST(request.utf16_view().to_byte_string());
        auto resolved = resolve_require_hooked(string(bytes), filename);
        if (resolved.is_error())
            continue;
        auto target = resolved.release_value();
        if (!target.builtin.empty() || !target.host.empty() || target.path.extension() == ".json"
            || target.path.extension() == ".node")
            continue;
        if (TRY(is_es_module(target.path)))
            continue;
        auto nested = commonjs_export_names(target.path, depth + 1);
        if (nested.is_error())
            return nested.release_error();

        for (auto const& name : nested.release_value()) {
            if (seen.set(name) == AK::HashSetResult::InsertedNewEntry)
                names.push_back(name);
        }
    }

    m_commonjs_export_names[identity] = names;
    complete = true;
    return names;
}

JS::ThrowCompletionOr<JS::Value> ModuleLoader::commonjs_namespace_snapshot(
    std::filesystem::path const& filename, JS::Value names)
{
    GC::RootVector<JS::Value> values;
    values.append(TRY(load_commonjs(filename)));
    auto key = Utf16String::from_utf8_with_replacement_character(view(path_string(filename)));
    if (TRY(m_commonjs_esm_exports->has_own_property(key)))
        values[0] = TRY(m_commonjs_esm_exports->get(key));
    GC::Root list { TRY(names.to_object(m_vm)) };
    auto value = values[0];
    auto count = TRY(JS::length_of_array_like(m_vm, *list));
    for (u64 i = 0; i < count; ++i) {
        auto name = TRY(list->get(i)).as_string().utf16_string();
        GC::Root object { TRY(value.to_object(m_vm)) };
        auto own = TRY(object->internal_get_own_property(name));
        auto exported = JS::js_undefined();
        if (own.has_value()) {
            // Node deliberately swallows a getter's exception while retaining the
            // statically discovered namespace slot. Own-property traps still fail.
            auto result = object->get(name);
            if (!result.is_error())
                exported = result.release_value();
        }

        values.append(exported);
    }

    return JS::Array::create_from(m_realm, values);
}
}
