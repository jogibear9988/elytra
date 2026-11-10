/** Suspend Fetch without body buffering; each continuation is rooted and settled at most once. */
#include "Gate.h"
#include "native/web-request/Headers.h"
#include <AK/JsonArray.h>
#include <AK/NeverDestroyed.h>
#include <AK/StringBuilder.h>
#include <AK/ThreadID.h>
#include <AK/Utf8View.h>
#include <LibGC/Function.h>
#include <LibGC/Root.h>
#include <LibGC/Weak.h>
#include <LibHTTP/HeaderList.h>
#include <LibWeb/Bindings/PrincipalHostDefined.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/Fetch/Fetching/ClientContextSnapshots.h>
#include <LibWeb/Fetch/Infrastructure/FetchController.h>
#include <LibWeb/HTML/LocalNavigable.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Page/Page.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <vector>

namespace Firefly::WebRequestGate {
using Params = Web::Fetch::Infrastructure::FetchParams;
using Response = Web::Fetch::Infrastructure::Response;
using Pending = Web::Fetch::Fetching::PendingResponse;
using Controller = Web::Fetch::Infrastructure::FetchController;
using Request = Web::Fetch::Infrastructure::Request;

namespace {
    struct Logical {
        u32 id;
        GC::Weak<Controller> controller;
        GC::Weak<Web::DOM::Document> document;
    };

    struct Job {
        u32 epoch;
        u32 request {};
        Stage stage;
        RefPtr<Params const> params;
        GC::Root<Web::DOM::Document> document;
        GC::Root<Pending> pending;
        GC::Root<Before> before;
        GC::Root<Head> head;
        RefPtr<Response> response;
        JsonObject original_headers;
    };

    struct State {
        Policy policy;
        std::atomic<u64> published_policy { 0 };
        AK::ThreadID owner { AK::ThreadID::current() };
        Submit submit;
        Cancel cancel;
        Finish finish;
        std::vector<Logical> logical;
        std::vector<std::shared_ptr<Job>> jobs;
        u32 next_logical {};
    };

    State& state()
    {
        static NeverDestroyed<State> value;
        return *value;
    }

    GC::Ptr<Web::DOM::Document> source(Params const& params)
    {
        if (auto settings = Web::Fetch::Fetching::resolve_client(*params.request())) {
            auto* window = Web::HTML::window_from_global_object(settings->global_object());
            if (window
                && &window->associated_document().relevant_settings_object() == settings.ptr())
                return window->associated_document();
        }

        for (auto const& logical : state().logical)
            if (logical.controller.ptr() == params.controller())
                return logical.document.ptr();
        return nullptr;
    }

    bool current(Job const& job)
    {
        return job.epoch == state().policy.epoch && !job.params->is_canceled() && job.document
            && job.document->is_fully_active() && job.document->navigable();
    }

    void stop_body(Job const& job)
    {
        if (job.response)
            job.response->release_request_transfer_lease();
        job.params->controller()->stop_body_transport();
    }

    void settle(std::shared_ptr<Job> job, Optional<JsonObject> decision)
    {
        auto& jobs = state().jobs;
        auto found = std::find(jobs.begin(), jobs.end(), job);
        if (found == jobs.end())
            return;
        jobs.erase(found);
        bool allowed = current(*job) && decision.has_value();
        if (allowed) {
            auto validated = WebRequest::validate_decision(
                *decision, &job->original_headers, job->stage == Stage::Before);
            if (validated.is_error())
                allowed = false;
        }

        if (allowed) {
            auto cancel = decision->get_bool("cancel"sv);
            allowed = cancel.has_value() && !*cancel;
        }

        if (allowed) {
            auto field = job->stage == Stage::Before ? "requestHeaders"sv : "responseHeaders"sv;
            if (auto value = decision->get(field); value.has_value()) {
                allowed = value->is_object();
                auto list = HTTP::HeaderList::create();
                size_t bytes = 0, count = 0;
                if (allowed)
                    value->as_object().for_each_member([&](auto const& name, auto const& values) {
                        if (!allowed || !values.is_array() || name.is_empty()
                            || name.byte_count() > 256) {
                            allowed = false;
                            return;
                        }

                        auto name_bytes = name.to_byte_string();
                        if (!HTTP::is_header_name(name_bytes)) {
                            allowed = false;
                            return;
                        }

                        for (auto const& item : values.as_array().values()) {
                            if (!item.is_string()) {
                                allowed = false;
                                break;
                            }

                            StringBuilder decoded;
                            for (auto point : Utf8View(item.as_string()))
                                decoded.append(static_cast<char>(point));
                            auto text = decoded.to_byte_string();
                            bytes += name.byte_count() + text.length();
                            if (++count > 128 || bytes > 32768 || !HTTP::is_header_value(text)) {
                                allowed = false;
                                break;
                            }

                            list->append({ name_bytes, move(text) });
                        }
                    });
                if (allowed) {
                    if (job->stage == Stage::Before)
                        job->params->request()->set_header_list(move(list));
                    else
                        job->response->set_header_list(move(list));
                }
            }
        }

        if (!allowed) {
            stop_body(*job);
            auto error = Response::network_error(
                "Firefly webRequest canceled or retired this request"_string);
            if (job->pending)
                job->pending->resolve(error);
            else if (job->head)
                job->head->function()(move(error));
            return;
        }

        if (job->before) {
            auto next = job->before->function()();
            auto pending = job->pending;
            next->when_loaded(GC::create_function(GC::Heap::the(), GC::captures(pending),
                [](auto& pending, NonnullRefPtr<Response> response) {
                    pending->resolve(move(response));
                }));
        } else if (job->head)
            job->head->function()(job->response.release_nonnull());
    }

    Optional<u32> logical_id(Params const& params, Web::DOM::Document& document)
    {
        sweep();
        for (auto const& record : state().logical)
            if (record.controller.ptr() == params.controller())
                return record.id;
        if (state().logical.size() >= 1024 || state().next_logical == 2147483647)
            return {};
        auto id = ++state().next_logical;
        state().logical.push_back({ id, GC::Weak { *params.controller() }, GC::Weak { document } });
        return id;
    }

    StringView resource_type(Request const& request)
    {
        if (request.is_navigation_request())
            return "mainFrame"sv;
        if (!request.destination().has_value())
            return "xhr"sv;
        switch (*request.destination()) {
        case Request::Destination::Script:
            return "script"sv;
        case Request::Destination::Style:
            return "stylesheet"sv;
        case Request::Destination::Image:
            return "image"sv;
        case Request::Destination::Font:
            return "font"sv;
        case Request::Destination::Audio:
        case Request::Destination::Video:
            return "media"sv;
        case Request::Destination::Document:
            return "mainFrame"sv;
        case Request::Destination::Frame:
        case Request::Destination::IFrame:
            return "subFrame"sv;
        default:
            return "other"sv;
        }
    }

    Optional<JsonObject> metadata(
        Params const& params, Web::DOM::Document& doc, RefPtr<Response> response)
    {
        auto const& request = *params.request();
        auto url = request.current_url().serialize();
        if (url.byte_count() > 65536 || request.method().length() > 32)
            return {};
        auto logical = logical_id(params, doc);
        if (!logical.has_value())
            return {};
        JsonObject result;
        result.set("url"sv, move(url));
        result.set("method"sv, MUST(String::from_utf8(request.method().view())));
        result.set("resourceType"sv, resource_type(request));
        result.set("referrer"sv, ""sv);
        result.set("logicalId"sv, *logical);
        if (auto const* referrer = request.referrer().get_pointer<URL::URL>()) {
            auto text = referrer->serialize();
            if (text.byte_count() > 65536)
                return {};
            result.set("referrer"sv, move(text));
        }

        if (request.is_navigation_request()) {
            auto nav = doc.navigable();
            auto local = as_if<Web::HTML::LocalNavigable>(nav.ptr());
            auto const* navigation
                = local ? local->ongoing_navigation().get_pointer<Utf16String>() : nullptr;
            if (!navigation || navigation->length_in_code_units() > 1024)
                return {};
            auto text = navigation->to_utf8();
            result.set("navigation"sv, move(text));
        }

        if (response)
            result.set("statusCode"sv, response->status());
        {
            // Snapshot the actual request before CORS preflight/transport, not a
            // fabricated list of automatic HTTP headers added later by the SDK.
            auto list = response ? response->header_list() : request.header_list();
            JsonObject headers;
            size_t bytes = 0, count = 0;
            for (auto const& header : list->headers()) {
                bytes += header.name.length() + header.value.length();
                if (++count > 128 || bytes > 32768)
                    return {};
                auto normalized = header.name.to_lowercase();
                auto name = String::from_utf8(normalized.view());
                if (name.is_error())
                    return {};
                StringBuilder encoded;
                for (auto byte : header.value.bytes())
                    encoded.append_code_point(byte);
                auto value = encoded.to_string();
                if (value.is_error())
                    return {};
                auto values = headers.get_array(name.value());
                JsonArray array = values.value_or(JsonArray {});
                MUST(array.append(value.release_value()));
                headers.set(name.release_value(), move(array));
            }

            result.set(response ? "responseHeaders"sv : "requestHeaders"sv, move(headers));
        }

        if (result.serialized().byte_count() > 96 * 1024)
            return {};
        return result;
    }

    bool submit(std::shared_ptr<Job> job)
    {
        auto data = metadata(*job->params, *job->document, job->response);
        if (!data.has_value() || !state().submit || state().jobs.size() >= 16)
            return false;
        auto original = data->get_object(
            job->stage == Stage::Headers ? "responseHeaders"sv : "requestHeaders"sv);
        if (!original.has_value())
            return false;
        job->original_headers = original.release_value();

        state().jobs.push_back(job);
        auto weak = std::weak_ptr<Job>(job);
        auto request = state().submit(
            job->stage, *job->document, data.release_value(), [weak](auto decision) {
                if (auto owner = weak.lock())
                    settle(move(owner), move(decision));
            });
        if (request.is_error()) {
            settle(job, {});
            return true;
        }

        job->request = request.release_value();
        return true;
    }
}

void install(Submit submit, Cancel cancel, Finish finish)
{
    state().submit = move(submit);
    state().cancel = move(cancel);
    state().finish = move(finish);
}

Policy policy()
{
    auto value = state().published_policy.load(std::memory_order_acquire);
    return { static_cast<u32>(value & 0x7fffffff), !!(value & (1ull << 32)),
        !!(value & (1ull << 33)) };
}

bool configure(Policy next)
{
    if (!next.epoch || next.epoch > 2147483647 || next.epoch < state().policy.epoch)
        return false;
    if (next.epoch == state().policy.epoch)
        return next.before == state().policy.before && next.headers == state().policy.headers;
    auto jobs = state().jobs;
    for (auto& job : jobs) {
        if (state().cancel && job->request)
            state().cancel(job->request);
        settle(job, {});
    }

    state().logical.clear();
    state().policy = next;
    state().published_policy.store(static_cast<u64>(next.epoch) | (next.before ? 1ull << 32 : 0)
            | (next.headers ? 1ull << 33 : 0),
        std::memory_order_release);
    return true;
}

void sweep()
{
    std::vector<u32> expired;
    std::erase_if(state().logical, [&](auto const& record) {
        if (record.controller.ptr())
            return false;
        expired.push_back(record.id);
        return true;
    });
    for (auto id : expired)
        if (state().finish)
            state().finish(id);
}

void shutdown()
{
    auto jobs = state().jobs;
    for (auto& job : jobs)
        settle(job, {});
    state().logical.clear();
    state().submit = {};
    state().cancel = {};
    state().finish = {};
}

GC::Ref<Pending> before(JS::Realm& realm, Params const& params, GC::Ref<Before> continuation)
{
    auto selected = policy();
    if (!selected.before && !selected.headers)
        return continuation->function()();
    if (!state().owner.is_current_thread())
        return Pending::create(params,
            Response::network_error("webRequest worker interception is unsupported"_string));
    auto doc = source(params);
    if (!doc && params.request()->is_navigation_request()) {
        auto const* id = params.request()
                             ->traversable_for_user_prompts()
                             .get_pointer<Web::HTML::CrossProcessId>();
        if (id) {
            auto& page = Web::Bindings::principal_host_defined_page(realm);
            auto local = as_if<Web::HTML::LocalNavigable>(page.navigable_with_id(*id).ptr());
            if (local && local->is_top_level_traversable() && local->active_document()
                && params.request()->replaces_client_id()
                    == local->active_document()->relevant_settings_object().id)
                doc = local->active_document();
        }
    }

    if (!doc || !doc->is_fully_active() || !doc->navigable())
        return Pending::create(params,
            Response::network_error("webRequest cannot authenticate this request scope"_string));
    if (params.request()->is_navigation_request()) {
        auto const* prompt = params.request()
                                 ->traversable_for_user_prompts()
                                 .get_pointer<Web::HTML::CrossProcessId>();
        if (!doc->navigable()->is_top_level_traversable() || !prompt
            || *prompt != doc->navigable()->id()
            || params.request()->replaces_client_id() != doc->relevant_settings_object().id)
            return Pending::create(params,
                Response::network_error(
                    "webRequest subframe or unbound navigation is unsupported"_string));
    }

    if (!logical_id(params, *doc).has_value())
        return Pending::create(
            params, Response::network_error("webRequest logical request budget exceeded"_string));
    if (!selected.before)
        return continuation->function()();
    auto job = std::make_shared<Job>();
    job->epoch = selected.epoch;
    job->stage = Stage::Before;
    job->params = params;
    job->document = doc;
    job->before = continuation;
    job->pending = Pending::create(params);
    auto pending = job->pending;
    if (!submit(job)) {
        state().jobs.push_back(job);
        settle(job, {});
    }

    return *pending;
}

bool headers(Params const& params, NonnullRefPtr<Response> response, GC::Ref<Head> continuation)
{
    if (!policy().headers || response->is_network_error())
        return false;
    if (!state().owner.is_current_thread()) {
        response->release_request_transfer_lease();
        params.controller()->stop_body_transport();
        continuation->function()(
            Response::network_error("webRequest worker interception is unsupported"_string));
        return true;
    }

    auto doc = source(params);
    auto job = std::make_shared<Job>();
    job->epoch = policy().epoch;
    job->stage = Stage::Headers;
    job->params = params;
    job->document = doc;
    job->response = move(response);
    job->head = continuation;
    if (!doc || !doc->is_fully_active() || !submit(job)) {
        state().jobs.push_back(job);
        settle(job, {});
    }

    return true;
}

void finish(Params const& params)
{
    if (!state().owner.is_current_thread())
        return;
    for (auto it = state().logical.begin(); it != state().logical.end();) {
        if (it->controller.ptr() == params.controller()) {
            auto id = it->id;
            it = state().logical.erase(it);
            if (state().finish)
                state().finish(id);
        } else
            ++it;
    }
}

void cancel(Params const& params)
{
    if (!state().owner.is_current_thread())
        return;
    auto jobs = state().jobs;
    for (auto& job : jobs)
        if (job->params->controller().ptr() == params.controller().ptr()) {
            if (state().cancel && job->request)
                state().cancel(job->request);
            settle(job, {});
        }

    finish(params);
}
}
