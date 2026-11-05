#include "HTTP.h"
#include <AK/Utf16StringBuilder.h>
#include <LibGC/CellAllocator.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Array.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <llhttp.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

static_assert(LLHTTP_VERSION_MAJOR == 9 && LLHTTP_VERSION_MINOR == 3 && LLHTTP_VERSION_PATCH == 1);

namespace Mite {
namespace {
struct Event {
    enum Type { Headers, Body, Trailers, Complete, Begin } type;
    explicit Event(Type kind) : type(kind) { }
    std::string method, url, body;
    std::vector<std::string> headers;
    int major {}, minor {};
    bool keep_alive {}, upgrade {};
};
static JS::Value latin1(JS::VM& vm, std::string const& value)
{
    Utf16StringBuilder builder;
    for (unsigned char byte : value) builder.append_code_point(byte);
    return JS::PrimitiveString::create(vm, builder.to_string());
}
}
class HTTPParser final : public GC::Cell {
    GC_CELL(HTTPParser, GC::Cell);
    GC_DECLARE_ALLOCATOR(HTTPParser);
public:
    explicit HTTPParser(size_t maximum) : m_maximum(maximum)
    {
        llhttp_settings_init(&m_settings);
        m_settings.on_message_begin = [](llhttp_t* p) {
            auto& s = self(p); s.m_size = 0; s.m_url.clear(); s.m_field.clear(); s.m_value.clear(); s.m_headers.clear(); s.m_events.push_back(Event { Event::Begin }); return 0;
        };
        m_settings.on_url = [](llhttp_t* p, char const* data, size_t length) -> int { auto& s = self(p); if (s.account(length)) return HPE_USER; s.m_url.append(data, length); return 0; };
        m_settings.on_header_field = [](llhttp_t* p, char const* data, size_t length) -> int { auto& s = self(p); if (s.account(length)) return HPE_USER; s.m_field.append(data, length); return 0; };
        m_settings.on_header_value = [](llhttp_t* p, char const* data, size_t length) -> int { auto& s = self(p); if (s.account(length)) return HPE_USER; s.m_value.append(data, length); return 0; };
        m_settings.on_header_value_complete = [](llhttp_t* p) {
            auto& s = self(p); s.m_headers.push_back(std::move(s.m_field)); s.m_headers.push_back(std::move(s.m_value)); s.m_field.clear(); s.m_value.clear(); return 0;
        };
        m_settings.on_headers_complete = [](llhttp_t* p) {
            auto& s = self(p);
            Event event { Event::Headers };
            event.method = llhttp_method_name(static_cast<llhttp_method_t>(p->method));
            event.url = std::move(s.m_url); event.headers = std::move(s.m_headers);
            event.major = p->http_major; event.minor = p->http_minor;
            event.keep_alive = llhttp_should_keep_alive(p); event.upgrade = p->upgrade;
            s.m_events.push_back(std::move(event)); s.m_headers.clear(); return 0;
        };
        m_settings.on_body = [](llhttp_t* p, char const* data, size_t length) {
            auto& s = self(p); Event event { Event::Body }; event.body.assign(data, length); s.m_events.push_back(std::move(event)); return 0;
        };
        m_settings.on_message_complete = [](llhttp_t* p) {
            auto& s = self(p);
            if (!s.m_headers.empty()) { Event event { Event::Trailers }; event.headers = std::move(s.m_headers); s.m_events.push_back(std::move(event)); s.m_headers.clear(); }
            s.m_events.push_back(Event { Event::Complete }); return 0;
        };
        llhttp_init(&m_parser, HTTP_REQUEST, &m_settings);
        m_parser.data = this;
    }
    JS::ThrowCompletionOr<JS::Value> execute(JS::VM& vm)
    {
        m_events.clear();
        llhttp_errno_t status = HPE_OK;
        size_t consumed = 0;
        if (vm.argument(0).is_undefined()) status = llhttp_finish(&m_parser);
        else {
            auto value = vm.argument(0);
            if (!value.is_object() || !is<JS::Uint8Array>(value.as_object())) return vm.throw_completion<JS::TypeError>("HTTP parser input must be Uint8Array"_utf16);
            auto& array = static_cast<JS::Uint8Array&>(value.as_object());
            auto witness = JS::make_typed_array_with_buffer_witness_record(array, JS::ArrayBuffer::Order::SeqCst);
            if (JS::is_typed_array_out_of_bounds(witness)) return vm.throw_completion<JS::TypeError>("Detached HTTP parser input"_utf16);
            auto length = JS::typed_array_byte_length(witness);
            if (!length) status = llhttp_execute(&m_parser, "", 0);
            else array.viewed_array_buffer()->with_readonly_bytes(array.byte_offset(), length, [&](ReadonlyBytes bytes) {
                auto* start = reinterpret_cast<char const*>(bytes.data());
                status = llhttp_execute(&m_parser, start, bytes.size());
                auto position = reinterpret_cast<uintptr_t>(llhttp_get_error_pos(&m_parser));
                auto begin = reinterpret_cast<uintptr_t>(start);
                consumed = !status ? length : position >= begin && position <= begin + length ? position - begin : 0;
            });
        }
        auto& realm = vm.realm();
        auto result = JS::Object::create(realm, nullptr);
        GC::Root events { TRY(JS::Array::create(realm, 0)) };
        size_t index = 0;
        for (auto const& event : m_events) {
            auto value = JS::Object::create(realm, nullptr);
            value->define_direct_property("type"_utf16, JS::Value(static_cast<int>(event.type)), JS::default_attributes);
            if (event.type == Event::Body) {
                auto bytes = TRY(JS::Uint8Array::create(realm, event.body.size()));
                if (!event.body.empty()) bytes->viewed_array_buffer()->overwrite(0, event.body.data(), event.body.size());
                value->define_direct_property("data"_utf16, bytes, JS::default_attributes);
            }
            if (event.type == Event::Headers || event.type == Event::Trailers) {
                auto headers = TRY(JS::Array::create(realm, 0));
                for (size_t i = 0; i < event.headers.size(); ++i) TRY(headers->create_data_property_or_throw(i, latin1(vm, event.headers[i])));
                value->define_direct_property("headers"_utf16, headers, JS::default_attributes);
            }
            if (event.type == Event::Headers) {
                value->define_direct_property("method"_utf16, latin1(vm, event.method), JS::default_attributes);
                value->define_direct_property("url"_utf16, latin1(vm, event.url), JS::default_attributes);
                value->define_direct_property("major"_utf16, JS::Value(event.major), JS::default_attributes);
                value->define_direct_property("minor"_utf16, JS::Value(event.minor), JS::default_attributes);
                value->define_direct_property("keepAlive"_utf16, JS::Value(event.keep_alive), JS::default_attributes);
                value->define_direct_property("upgrade"_utf16, JS::Value(event.upgrade), JS::default_attributes);
            }
            TRY(events->create_data_property_or_throw(index++, value));
        }
        m_events.clear();
        result->define_direct_property("events"_utf16, events.ptr(), JS::default_attributes);
        if (status) {
            auto error = JS::Object::create(realm, nullptr);
            error->define_direct_property("code"_utf16, latin1(vm, m_overflow ? "HPE_HEADER_OVERFLOW" : llhttp_errno_name(status)), JS::default_attributes);
            auto* reason = llhttp_get_error_reason(&m_parser);
            error->define_direct_property("reason"_utf16, latin1(vm, m_overflow ? "Header overflow" : reason ? reason : "HTTP parse error"), JS::default_attributes);
            error->define_direct_property("bytesParsed"_utf16, JS::Value(static_cast<double>(consumed)), JS::default_attributes);
            result->define_direct_property("error"_utf16, error, JS::default_attributes);
        }
        return result;
    }
private:
    static HTTPParser& self(llhttp_t* p) { return *static_cast<HTTPParser*>(p->data); }
    bool account(size_t count) { if (count > m_maximum - m_size) { m_overflow = true; return true; } m_size += count; return false; }
    llhttp_t m_parser {};
    llhttp_settings_t m_settings {};
    size_t m_maximum, m_size {};
    bool m_overflow {};
    std::string m_url, m_field, m_value;
    std::vector<std::string> m_headers;
    std::vector<Event> m_events;
};
GC_DEFINE_ALLOCATOR(HTTPParser);

GC::Ref<JS::Object> create_http_host(JS::Realm& realm)
{
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(realm, "createParser"_utf16, [&realm](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
        auto maximum = vm.argument(0);
        if (!maximum.is_number() || !std::isfinite(maximum.as_double()) || std::trunc(maximum.as_double()) != maximum.as_double()
            || maximum.as_double() < 1 || maximum.as_double() > 2147483647)
            return vm.throw_completion<JS::TypeError>("Invalid HTTP header limit"_utf16);
        auto parser = vm.heap().allocate<HTTPParser>(static_cast<size_t>(maximum.as_double()));
        return JS::NativeFunction::create(realm, [parser](JS::VM& vm) { return parser->execute(vm); }, 1, "parseHTTP"_utf16);
    }, 1, JS::default_attributes);
    auto methods = MUST(JS::Array::create(realm, 0));
    size_t index = 0;
#define MITE_HTTP_METHOD(number, name, label) MUST(methods->create_data_property_or_throw(index++, latin1(realm.vm(), #label)));
    HTTP_METHOD_MAP(MITE_HTTP_METHOD)
#undef MITE_HTTP_METHOD
    host->define_direct_property("METHODS"_utf16, methods, JS::default_attributes);
    return host;
}
}
