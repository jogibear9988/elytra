/** Per-document permission decisions. Pending callbacks never authorize a replacement document. */
#include "Impl.h"
#include "Value.h"
#include "core/EventLoop.h"
#include <AK/JsonArray.h>
#include <AK/JsonObject.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibWebCommon/SecureContexts/AbstractOperations.h>
#include <LibWebView/CanonicalDocument.h>
#include <LibWebView/CanonicalNavigable.h>
#include <LibWebView/CanonicalTraversable.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebContentPage.h>
#include <UI/Qt/WebContentView.h>
#include <algorithm>
#include <cmath>

namespace Firefly {
using BridgeDetail::text;

namespace {
    Optional<int> positive_id(JS::Value value)
    {
        if (!value.is_number())
            return {};
        auto number = value.as_double();
        if (!std::isfinite(number) || number < 1 || number > 2147483647
            || std::floor(number) != number)
            return {};
        return static_cast<int>(number);
    }

    StringView public_name(Permissions::Name name)
    {
        return name == Permissions::Name::Microphone ? "media"sv
                                                     : Web::Permissions::to_string(name);
    }

    Optional<JsonObject> details(WebView::WebContentPage& page, Permissions::DocumentId id)
    {
        constexpr size_t field_limit = 64 * 1024;
        auto document = page.hosted_document_with_id(id);
        if (!document)
            return {};
        Optional<String> requesting_url;
        page.for_each_navigable_with_a_hosted_active_document(
            [&](WebView::CanonicalNavigable const& navigable) {
                if (navigable.active_document().id() == id) {
                    requesting_url = navigable.active_document_url().serialize();
                    return IterationDecision::Break;
                }

                return IterationDecision::Continue;
            });
        if (!requesting_url.has_value() || requesting_url->byte_count() > field_limit)
            return {};
        auto security_origin = document->origin().serialize();
        auto top = document->top_level_origin();
        if (!top.has_value() || security_origin.byte_count() > field_limit)
            return {};
        auto embedding_origin = top->serialize();
        if (embedding_origin.byte_count() > field_limit)
            return {};
        JsonObject result;
        result.set("requestingUrl"sv, requesting_url.release_value());
        result.set("securityOrigin"sv, move(security_origin));
        result.set("embeddingOrigin"sv, move(embedding_origin));
        result.set("isMainFrame"sv, page.traversable().active_document().id() == id);
        return result;
    }
}

void Bridge::Impl::initialize_permissions()
{
    Permissions::install_hooks({ .policy =
                                     [this] {
                                         return Permissions::Policy { permission_epoch,
                                             !!permission_check, permission_requests };
                                     },
        .check = [this](auto& page, auto doc, auto epoch,
                     auto name) { return check_permission(page, doc, epoch, name); },
        .request = [this](auto& page, auto doc, auto name,
                       auto decision) { request_permission(page, doc, name, std::move(decision)); },
        .may_use =
            [this](auto& page, auto doc, auto name) {
                int id = permission_contents(page, doc);
                if (!id || name == Permissions::Name::Microphone)
                    return false;
                bool granted = std::any_of(
                    permission_grants.begin(), permission_grants.end(), [&](auto const& grant) {
                        return grant.contents == id && grant.document == doc && grant.name == name;
                    });
                return granted || check_permission(page, doc, permission_epoch, name);
            } });
}

int Bridge::Impl::permission_contents(WebView::WebContentPage& page, Permissions::DocumentId doc)
{
    if (stopping || !page.is_open())
        return 0;
    bool active = false;
    page.for_each_navigable_with_a_hosted_active_document(
        [&](WebView::CanonicalNavigable const& navigable) {
            if (navigable.active_document().id() == doc && page.hosts_active_document_of(navigable))
                active = true;
            return IterationDecision::Continue;
        });
    if (!active)
        return 0;
    for (auto const& [id, state] : windows)
        if (!state.closing && state.view == &page.view())
            return id;
    return 0;
}

void Bridge::Impl::configure_permission_page(WebView::WebContentPage& page)
{
    if (!stopping && permission_epoch)
        page.client().async_firefly_permission_policy(
            permission_epoch, !!permission_check, permission_requests);
}

void Bridge::Impl::broadcast_permissions()
{
    // A frame may live in another helper. Policy is process-wide, so one
    // ordered update per actual live client covers all of its hosted pages.
    std::unordered_set<WebView::WebContentClient*> clients;
    for (auto& [id, state] : windows) {
        (void)id;
        if (state.closing || !state.view->has_traversable())
            continue;
        state.view->traversable().for_each_hosting_page([&](auto& page) {
            if (page.is_open() && clients.insert(&page.client()).second)
                configure_permission_page(page);
        });
    }
}

bool Bridge::Impl::check_permission(
    WebView::WebContentPage& page, Permissions::DocumentId doc, u64 epoch, Permissions::Name name)
{
    auto id = permission_contents(page, doc);
    if (!id || epoch != permission_epoch || !permission_check || jobs.is_busy()
        || (dialogs && dialogs->is_synchronous()))
        return false;
    RefPtr<WebView::WebContentPage> keep_page_alive = &page;
    auto callback = permission_check;
    auto parsed_metadata = details(page, doc);
    if (!parsed_metadata.has_value())
        return false;
    auto metadata = parsed_metadata->serialized();
    if (metadata.byte_count() > 256 * 1024)
        return false;
    auto document = page.hosted_document_with_id(doc);
    if (!document)
        return false;
    auto top = document->top_level_origin();
    if (!top.has_value())
        return false;
    if (name == Permissions::Name::Geolocation
        && !document->is_allowed_to_use(
            Web::PermissionsPolicy::PolicyControlledFeature::Geolocation))
        return false;
    if (name == Permissions::Name::Microphone
        && !document->is_allowed_to_use(
            Web::PermissionsPolicy::PolicyControlledFeature::Microphone))
        return false;
    if (name == Permissions::Name::Notifications && !document->origin().is_same_origin(*top))
        return false;
    auto trustworthy = [](URL::Origin const& value) {
        return Web::SecureContexts::is_origin_potentially_trustworthy(value)
            == Web::SecureContexts::Trustworthiness::PotentiallyTrustworthy;
    };

    bool local_file = top->is_opaque() && page.view().url().scheme() == "file"sv;
    if (!local_file && (!trustworthy(*top) || !trustworthy(document->origin())))
        return false;
    auto origin = document->origin().serialize();
    bool allowed = false;
    jobs.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
        auto result = JS::call(vm, *callback, JS::js_undefined(), JS::Value(id),
            text(vm, public_name(name)), text(vm, origin), text(vm, metadata));
        if (result.is_error())
            return result.release_error();
        auto value = result.release_value();
        allowed = value.is_boolean() && value.as_bool();
        return JS::js_undefined();
    });
    if (jobs.status())
        gui.quit(jobs.status());
    return allowed && name != Permissions::Name::Microphone && permission_epoch == epoch
        && permission_contents(page, doc) == id;
}

void Bridge::Impl::request_permission(WebView::WebContentPage& page, Permissions::DocumentId doc,
    Permissions::Name name, Permissions::Decision decision)
{
    auto id = permission_contents(page, doc);
    size_t count = 0;
    for (auto const& [token, pending] : permission_pending) {
        (void)token;
        if (pending.contents == id)
            ++count;
    }

    if (!id || !permission_requests || permission_pending.size() >= 64 || count >= 16
        || permission_next == 2147483647) {
        decision(Web::Permissions::EffectiveState::Denied);
        return;
    }

    auto parsed_metadata = details(page, doc);
    if (!parsed_metadata.has_value()) {
        decision(Web::Permissions::EffectiveState::Denied);
        return;
    }

    auto metadata = parsed_metadata.release_value();
    auto token = permission_next + 1;
    metadata.set("token"sv, token);
    metadata.set("epoch"sv, permission_epoch);
    metadata.set("contentsId"sv, id);
    metadata.set("permission"sv, public_name(name));
    if (name == Permissions::Name::Microphone) {
        JsonArray types;
        MUST(types.append("audio"sv));
        metadata.set("mediaTypes"sv, move(types));
    }

    auto wire = metadata.serialized();
    if (wire.byte_count() > 256 * 1024) {
        decision(Web::Permissions::EffectiveState::Denied);
        return;
    }
    // Allocate authority only after all canonical metadata and JSON quotas pass.
    permission_next = token;
    permission_pending.emplace(token,
        PermissionPending { id, permission_epoch, &page, doc, name, std::move(decision),
            MonotonicTime::now() + AK::Duration::from_seconds(30) });
    enqueue(0, "permission-request", wire.to_byte_string());
}

bool Bridge::Impl::complete_permission(int token, u64 epoch, bool allowed)
{
    auto found = permission_pending.find(token);
    if (found == permission_pending.end() || found->second.epoch != epoch
        || permission_epoch != epoch)
        return false;
    auto pending = std::move(found->second);
    permission_pending.erase(found); // Reentrant callbacks cannot settle twice.
    bool current = permission_contents(*pending.page, pending.document) == pending.contents;
    auto already_granted
        = std::any_of(permission_grants.begin(), permission_grants.end(), [&](auto const& grant) {
              return grant.contents == pending.contents && grant.document == pending.document
                  && grant.name == pending.name;
          });
    allowed = allowed && current && MonotonicTime::now() < pending.deadline
        && pending.name != Permissions::Name::Microphone
        && (already_granted || permission_grants.size() < 256);
    if (!allowed) {
        std::erase_if(permission_grants, [&](auto const& grant) {
            return grant.contents == pending.contents && grant.document == pending.document
                && grant.name == pending.name;
        });
    }

    if (allowed && !already_granted)
        permission_grants.push_back({ pending.contents, pending.document, pending.name });

    pending.decision(allowed ? Web::Permissions::EffectiveState::Granted
                             : Web::Permissions::EffectiveState::Denied);
    return true;
}

void Bridge::Impl::reset_permissions(int id)
{
    std::vector<Permissions::Decision> decisions;
    for (auto it = permission_pending.begin(); it != permission_pending.end();) {
        if (!id || it->second.contents == id) {
            decisions.push_back(std::move(it->second.decision));
            it = permission_pending.erase(it);
        } else
            ++it;
    }

    std::erase_if(
        permission_grants, [&](auto const& grant) { return !id || grant.contents == id; });
    for (auto& decision : decisions)
        decision(Web::Permissions::EffectiveState::Denied);
}

bool Bridge::Impl::permission_event_current(ByteString const& wire)
{
    auto decoded = JsonValue::from_string(wire);
    if (decoded.is_error() || !decoded.value().is_object())
        return false;
    auto const& object = decoded.value().as_object();
    auto token = object.get_u32("token"sv), epoch = object.get_u32("epoch"sv);
    if (!token.has_value() || !epoch.has_value() || *token > 2147483647)
        return false;
    auto found = permission_pending.find(static_cast<int>(*token));
    if (found == permission_pending.end() || found->second.epoch != *epoch
        || permission_epoch != *epoch)
        return false;
    if (MonotonicTime::now() >= found->second.deadline
        || permission_contents(*found->second.page, found->second.document)
            != found->second.contents) {
        complete_permission(*token, *epoch, false);
        return false;
    }

    return true;
}

void Bridge::Impl::expire_permissions()
{
    // Frame-only navigation does not reset the top-level IPC identity. Retire
    // old frame grants from the real canonical tree, rather than retaining
    // unusable historical document IDs until the next top-level navigation.
    std::erase_if(permission_grants, [&](auto const& grant) {
        auto owner = windows.find(grant.contents);
        if (owner == windows.end() || owner->second.closing
            || !owner->second.view->has_traversable())
            return true;
        bool active = false;
        owner->second.view->traversable().for_each_hosting_page([&](auto& page) {
            if (active || !page.is_open())
                return;
            page.for_each_navigable_with_a_hosted_active_document([&](auto const& navigable) {
                if (navigable.active_document().id() == grant.document
                    && permission_contents(page, grant.document) == grant.contents) {
                    active = true;
                    return IterationDecision::Break;
                }

                return IterationDecision::Continue;
            });
        });
        return !active;
    });
    std::vector<std::pair<int, u64>> expired;
    for (auto const& [token, pending] : permission_pending)
        if (MonotonicTime::now() >= pending.deadline
            || permission_contents(*pending.page, pending.document) != pending.contents)
            expired.emplace_back(token, pending.epoch);
    for (auto [token, epoch] : expired)
        complete_permission(token, epoch, false);
}

void Bridge::Impl::install_permissions(JS::Object& host)
{
    auto object = JS::Object::create(realm, nullptr);
    object->define_native_function(
        realm, "configure"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto epoch = positive_id(vm.argument(0));
            auto callback = vm.argument(1);
            if (!epoch.has_value() || static_cast<u64>(*epoch) <= permission_epoch
                || (!callback.is_null() && !callback.is_function()) || !vm.argument(2).is_boolean())
                return preload_error(
                    vm, "ERR_INVALID_ARG_VALUE", "Invalid permission configuration"sv);
            if (stopping)
                return preload_error(vm, "ERR_FIREFLY_APP_EXITED", "Application has exited"sv);
            permission_epoch = *epoch;
            permission_check = callback.is_null() ? nullptr : &callback.as_function();
            permission_requests = vm.argument(2).as_bool();
            reset_permissions(0);
            broadcast_permissions();
            return JS::js_undefined();
        },
        3, JS::default_attributes);
    object->define_native_function(
        realm, "complete"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto token = positive_id(vm.argument(0)), epoch = positive_id(vm.argument(1));
            if (!token.has_value() || !epoch.has_value() || !vm.argument(2).is_boolean())
                return preload_error(vm, "ERR_INVALID_ARG_VALUE", "Invalid permission decision"sv);
            return JS::Value(
                !stopping && complete_permission(*token, *epoch, vm.argument(2).as_bool()));
        },
        3, JS::default_attributes);
    host.define_direct_property("permissions"_utf16, object, JS::default_attributes);
}
}
