#include "URL.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <LibURL/Parser.h>
#include <LibURL/RustIntegration.h>
#include <unicode/uidna.h>
#include <vector>

namespace Mite {
namespace {
static JS::Value text(JS::VM& vm, StringView value) { return JS::PrimitiveString::create(vm, Utf16String::from_utf8(value)); }
static JS::ThrowCompletionOr<Utf16String> argument(JS::VM& vm, size_t index) { return vm.argument(index).to_utf16_string(vm); }
static GC::Ref<JS::Object> record(JS::Realm& realm, URL::URL const& url)
{
    auto& vm = realm.vm();
    GC::Root object { JS::Object::create(realm, nullptr) };
    auto set = [&](auto name, StringView value) { object->define_direct_property(name, text(vm, value), JS::default_attributes); };
    set("href"_utf16, url.serialize());
    set("protocol"_utf16, ByteString::formatted("{}:", url.scheme()));
    set("username"_utf16, url.username()); set("password"_utf16, url.password());
    auto hostname = url.host().has_value() ? url.serialized_host() : StringView {};
    set("hostname"_utf16, hostname);
    set("host"_utf16, url.port().has_value() ? ByteString::formatted("{}:{}", hostname, *url.port()) : ByteString(hostname));
    set("port"_utf16, url.port().has_value() ? ByteString::number(*url.port()) : ByteString {});
    set("pathname"_utf16, url.serialize_path());
    set("search"_utf16, url.query().has_value() && !url.query()->is_empty() ? ByteString::formatted("?{}", *url.query()) : ByteString {});
    set("hash"_utf16, url.fragment().has_value() && !url.fragment()->is_empty() ? ByteString::formatted("#{}", *url.fragment()) : ByteString {});
    set("origin"_utf16, url.origin().serialize());
    return *object;
}
static void strip_opaque_spaces(URL::URL& url)
{
    if (!url.has_an_opaque_path() || url.query().has_value() || url.fragment().has_value()) return;
    auto path = url.serialize_path();
    size_t end = path.length();
    while (end && path[end - 1] == ' ') --end;
    if (end != path.length()) url.set_opaque_path(path.substring_view(0, end));
}
}
GC::Ref<JS::Object> create_url_host(JS::Realm& realm)
{
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(realm, "parse"_utf16, [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto input = TRY(argument(vm, 0));
        Optional<URL::URL> base;
        if (!vm.argument(1).is_undefined()) {
            auto value = TRY(argument(vm, 1));
            base = URL::Parser::basic_parse(value.utf16_view());
            if (!base.has_value()) return JS::js_null();
        }
        auto parsed = URL::Parser::basic_parse(input.utf16_view(), base.has_value() ? Optional<URL::URL const&>(*base) : Optional<URL::URL const&> {});
        if (!parsed.has_value()) return JS::js_null();
        return JS::Value(record(realm, *parsed));
    }, 2, JS::default_attributes);
    host->define_native_function(realm, "update"_utf16, [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto href = TRY(argument(vm, 0));
        auto parsed = URL::Parser::basic_parse(href.utf16_view());
        VERIFY(parsed.has_value());
        auto key = TRY(argument(vm, 1));
        auto value = TRY(argument(vm, 2));
        auto input = value.utf16_view();
        auto& url = *parsed;
        if (key == "protocol"_utf16) { auto protocol = Utf16String::formatted("{}:", input); URL::Parser::basic_parse(protocol.utf16_view(), url, URL::Parser::State::SchemeStart); }
        else if (key == "username"_utf16) { if (!url.cannot_have_a_username_or_password_or_port()) url.set_username(input); }
        else if (key == "password"_utf16) { if (!url.cannot_have_a_username_or_password_or_port()) url.set_password(input); }
        else if (key == "host"_utf16 || key == "hostname"_utf16) {
            if (!url.has_an_opaque_path()) URL::Parser::basic_parse(input, url, key == "host"_utf16 ? URL::Parser::State::Host : URL::Parser::State::Hostname);
        } else if (key == "port"_utf16) {
            if (!url.cannot_have_a_username_or_password_or_port()) {
                if (input.is_empty()) url.set_port({});
                else URL::Parser::basic_parse(input, url, URL::Parser::State::Port);
            }
        } else if (key == "pathname"_utf16) {
            if (!url.has_an_opaque_path()) { url.set_path({}); URL::Parser::basic_parse(input, url, URL::Parser::State::PathStart); }
        } else if (key == "search"_utf16) {
            if (input.is_empty()) url.set_query({});
            else { if (input.starts_with("?"_utf16)) input = input.substring_view(1); url.set_query(""sv); URL::Parser::basic_parse(input, url, URL::Parser::State::Query); }
            strip_opaque_spaces(url);
        } else if (key == "hash"_utf16) {
            if (input.is_empty()) url.set_fragment({});
            else { if (input.starts_with("#"_utf16)) input = input.substring_view(1); url.set_fragment(""sv); URL::Parser::basic_parse(input, url, URL::Parser::State::Fragment); }
            strip_opaque_spaces(url);
        }
        return JS::Value(record(realm, url));
    }, 3, JS::default_attributes);
    host->define_native_function(realm, "decodeForm"_utf16, [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto input = TRY(argument(vm, 0));
        auto utf8 = MUST(input.utf16_view().to_utf8());
        struct Pair { Utf16String name, value; };
        Vector<Pair> pairs;
        URL::FFI::rust_url_form_urlencoded_parse({ reinterpret_cast<u8 const*>(utf8.bytes_as_string_view().characters_without_null_termination()), utf8.bytes_as_string_view().length() }, &pairs,
            [](void* context, URL::FFI::RustUrlByteSlice name, URL::FFI::RustUrlByteSlice value) {
                static_cast<Vector<Pair>*>(context)->append({ Utf16String::from_utf8({ reinterpret_cast<char const*>(name.data), name.length }),
                    Utf16String::from_utf8({ reinterpret_cast<char const*>(value.data), value.length }) });
            });
        GC::RootVector<JS::Value> result;
        for (auto const& pair : pairs) {
            GC::RootVector<JS::Value> fields;
            fields.append(JS::PrimitiveString::create(vm, pair.name)); fields.append(JS::PrimitiveString::create(vm, pair.value));
            result.append(JS::Array::create_from(realm, fields));
        }
        return JS::Value(JS::Array::create_from(realm, result));
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "encodeForm"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto list = TRY(vm.argument(0).to_object(vm));
        auto length = TRY(JS::length_of_array_like(vm, list));
        struct Pair { Utf16String name, value; };
        Vector<Pair> pairs;
        for (size_t index = 0; index < length; ++index) {
            auto pair = TRY(TRY(list->get(index)).to_object(vm));
            pairs.append({ TRY(TRY(pair->get(0)).to_utf16_string(vm)), TRY(TRY(pair->get(1)).to_utf16_string(vm)) });
        }
        Vector<URL::FFI::FfiFormTuple> tuples;
        for (auto const& pair : pairs) tuples.append({ URL::RustIntegration::rust_url_input(pair.name.utf16_view()), URL::RustIntegration::rust_url_input(pair.value.utf16_view()) });
        String output;
        URL::FFI::rust_url_form_urlencoded_serialize(tuples.data(), tuples.size(), { nullptr, 0 }, &output,
            [](void* context, URL::FFI::RustUrlByteSlice result) { *static_cast<String*>(context) = String::from_ascii_without_validation({ result.data, result.length }); });
        return text(vm, output);
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "decodePath"_utf16, [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto input = TRY(argument(vm, 0));
        auto encoded = MUST(input.utf16_view().to_utf8());
        auto decoded = URL::percent_decode(encoded);
        auto array = TRY(JS::Uint8Array::create(realm, decoded.length()));
        if (!decoded.is_empty()) array->viewed_array_buffer()->overwrite(0, decoded.characters(), decoded.length());
        return JS::Value(array);
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "asciiHost"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto input = TRY(argument(vm, 0));
        auto host = URL::Parser::parse_host(input.utf16_view());
        return host.has_value() ? text(vm, host->serialize()) : JS::js_null();
    }, 1, JS::default_attributes);
    host->define_native_function(realm, "unicodeHost"_utf16, +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto input = TRY(argument(vm, 0));
        auto encoded = MUST(input.utf16_view().to_utf8());
        auto bytes = encoded.bytes_as_string_view();
        if (bytes.starts_with('[')) return text(vm, bytes);
        UErrorCode status = U_ZERO_ERROR;
        auto* idna = uidna_openUTS46(UIDNA_CHECK_BIDI | UIDNA_CHECK_CONTEXTJ | UIDNA_NONTRANSITIONAL_TO_UNICODE, &status);
        if (U_FAILURE(status)) return vm.throw_completion<JS::Error>("Cannot initialize IDNA conversion"_utf16);
        std::vector<char> output(bytes.length() * 4 + 64);
        UIDNAInfo info = UIDNA_INFO_INITIALIZER;
        auto count = uidna_nameToUnicodeUTF8(idna, bytes.characters_without_null_termination(), static_cast<int32_t>(bytes.length()), output.data(), static_cast<int32_t>(output.size()), &info, &status);
        uidna_close(idna);
        if (U_FAILURE(status)) return text(vm, ""sv);
        return text(vm, StringView(output.data(), static_cast<size_t>(count)));
    }, 1, JS::default_attributes);
    return host;
}
}
