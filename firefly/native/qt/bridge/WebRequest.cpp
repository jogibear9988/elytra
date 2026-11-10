/** Session request decisions bind to the authenticated helper and canonical active document. */
#include "Impl.h"
#include "Value.h"
#include "native/web-request/Authority.h"
#include "native/web-request/Headers.h"
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/PrimitiveString.h>
#include <LibJS/Runtime/VM.h>
#include <LibURL/Parser.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/WebContentPage.h>
#include <UI/Qt/WebContentView.h>
#include <cmath>

namespace Firefly {
namespace {
    Optional<u32> policy_id(JS::Value value)
    {
        if (!value.is_number())
            return {};
        auto number = value.as_double();
        if (!std::isfinite(number) || number < 1 || number > 2147483647
            || std::floor(number) != number)
            return {};
        return static_cast<u32>(number);
    }
}

void Bridge::Impl::initialize_web_request()
{
    web_request = adopt_ref(*new WebRequest::Broker(
        [this](
            auto& client, auto const& packet) { return authenticate_web_request(client, packet); },
        [this](int contents, ByteString metadata) {
            enqueue(contents, "web-request", move(metadata));
        }));
}

Optional<JsonObject> Bridge::Impl::authenticate_web_request(
    WebView::WebContentClient& client, WebRequest::Packet const& packet)
{
    if (stopping)
        return {};
    auto* page = client.page(Web::PageId { packet.page });
    if (!page || !page->is_open() || &page->client() != &client)
        return {};
    auto navigable = page->hosted_navigable({ packet.navigable_namespace, packet.navigable_local });
    Web::HTML::CrossProcessId document { packet.document_namespace, packet.document_local };
    if (!navigable.has_value() || !page->hosts_active_document_of(*navigable)
        || navigable->active_document().id() != document)
        return {};
    auto contents = permission_contents(*page, document);
    auto owner = windows.find(contents);
    if (!contents || owner == windows.end() || owner->second.closing || !owner->second.native_view()
        || !owner->second.native_view()->has_traversable())
        return {};
    // Populate public frame identity from the complete canonical tree, never from
    // a packet's URL or a page-authored IPC payload.
    if (frames.snapshot(contents, &owner->second.native_view()->traversable()).is_error())
        return {};
    auto url = packet.payload.get_string("url"sv);
    auto method = packet.payload.get_string("method"sv);
    auto type = packet.payload.get_string("resourceType"sv);
    auto referrer = packet.payload.get_string("referrer"sv);
    if (!url.has_value() || url->byte_count() > 32768 || !method.has_value() || method->is_empty()
        || method->byte_count() > 64 || !type.has_value() || type->byte_count() > 64
        || !referrer.has_value() || referrer->byte_count() > 32768)
        return {};
    auto parsed_url = URL::Parser::basic_parse(*url);
    if (!parsed_url.has_value() || url->bytes_as_string_view().contains('\0')
        || method->bytes_as_string_view().contains('\0')
        || method->bytes_as_string_view().contains('\r')
        || method->bytes_as_string_view().contains('\n')
        || referrer->bytes_as_string_view().contains('\0'))
        return {};
    auto navigation = packet.payload.get_string("navigation"sv);
    if (navigation.has_value() && !WebRequest::owns_navigation(*page, *navigable, *navigation))
        return {};
    JsonObject result;
    result.set("contentsId"sv, contents);
    result.set(
        "document"sv, MUST(String::formatted("{}:{}", document.namespace_id, document.local_id)));
    result.set("url"sv, *url);
    result.set("method"sv, *method);
    result.set("resourceType"sv, *type);
    result.set("referrer"sv, *referrer);
    result.set("initiatorOrigin"sv, navigable->active_document().origin().serialize());
    if (packet.kind == WebRequest::Kind::Before) {
        auto headers = packet.payload.get_object("requestHeaders"sv);
        if (!headers.has_value() || WebRequest::validate_headers(*headers).is_error())
            return {};
        result.set("requestHeaders"sv, *headers);
    }

    if (packet.kind == WebRequest::Kind::Headers) {
        auto status = packet.payload.get_u32("statusCode"sv);
        auto line = packet.payload.get_string("statusLine"sv);
        auto headers = packet.payload.get_object("responseHeaders"sv);
        if (!status.has_value() || *status < 100 || *status > 599
            || (line.has_value() && line->byte_count() > 4096) || !headers.has_value()
            || WebRequest::validate_headers(*headers).is_error())
            return {};
        result.set("statusCode"sv, *status);
        if (line.has_value())
            result.set("statusLine"sv, *line);
        result.set("responseHeaders"sv, *headers);
    }

    return result;
}

void Bridge::Impl::install_web_request(JS::Object& host)
{
    auto object = JS::Object::create(realm, nullptr);
    object->define_native_function(realm, "configure"_utf16,
        native_binding<&Impl::native_webrequest_configure>(), 3, JS::default_attributes);
    object->define_native_function(realm, "complete"_utf16,
        native_binding<&Impl::native_webrequest_complete>(), 3, JS::default_attributes);
    host.define_direct_property("webRequest"_utf16, object, JS::default_attributes);
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_webrequest_configure(JS::VM& vm)
{
    auto epoch = policy_id(vm.argument(0));
    if (!epoch.has_value() || !vm.argument(1).is_boolean() || !vm.argument(2).is_boolean())
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid webRequest configuration"sv);
    if (stopping || !web_request)
        return preload_error(vm, "ERR_FIREFLY_APP_EXITED", "Application has exited"sv);
    if (!web_request->configure(*epoch, vm.argument(1).as_bool(), vm.argument(2).as_bool()))
        return preload_error(vm, "ERR_FIREFLY_WEB_REQUEST_POLICY", "Stale webRequest policy"sv);
    for (auto const& [id, window] : windows) {
        (void)id;
        if (window.closing || !window.native_view() || !window.native_view()->has_traversable())
            continue;
        window.native_view()->traversable().for_each_hosting_page([&](auto& page) {
            if (auto result = web_request->bind(page); result.is_error())
                warnln("ERR_FIREFLY_WEB_REQUEST_CHANNEL: {}", result.error());
        });
    }

    return JS::js_undefined();
}

JS::ThrowCompletionOr<JS::Value> Bridge::Impl::native_webrequest_complete(JS::VM& vm)
{
    auto token = policy_id(vm.argument(0)), epoch = policy_id(vm.argument(1));
    if (!token.has_value() || !epoch.has_value() || !vm.argument(2).is_string())
        return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid webRequest decision"sv);
    auto encoded
        = vm.argument(2).as_string().utf16_string_view().to_utf8(AllowLonelySurrogates::No);
    if (encoded.is_error())
        return preload_error(
            vm, "ERR_FIREFLY_WEB_REQUEST_DATA", "Decision contains invalid Unicode"sv);
    auto wire = encoded.release_value();
    if (wire.byte_count() > WebRequest::packet_payload_limit)
        return preload_error(
            vm, "ERR_FIREFLY_WEB_REQUEST_DATA", "Decision metadata limit exceeded"sv);
    auto parsed = JsonValue::from_string(wire);
    if (parsed.is_error() || !parsed.value().is_object()
        || !parsed.value().as_object().get_bool("cancel"sv).has_value())
        return preload_error(vm, "ERR_FIREFLY_WEB_REQUEST_DATA", "Malformed webRequest decision"sv);
    return JS::Value(
        web_request && web_request->complete(*token, *epoch, parsed.value().as_object()));
}

}
