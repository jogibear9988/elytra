/** Suspend Fetch without body buffering; each continuation is rooted and settled at most once. */
#include "Gate.h"
#include "native/platform/QueueSnapshots.h"
#include "native/web-request/Headers.h"
#include "native/web-request/Limits.h"
#include <AK/JsonArray.h>
#include <AK/NeverDestroyed.h>
#include <AK/ScopeGuard.h>
#include <AK/StringBuilder.h>
#include <AK/ThreadID.h>
#include <AK/Time.h>
#include <AK/Utf8View.h>
#include <LibCore/EventLoop.h>
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
        MonotonicTime deadline { MonotonicTime::now() + AK::Duration::from_seconds(30) };
        size_t bytes {};
        bool accounted {};
        bool submitted {};
        bool completed {};
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
        QueueSnapshotClock diagnostics;
        u32 next_logical {};
        size_t bytes {};
        size_t active {};
        bool scheduled {};
        bool pumping {};
        bool draining {};
        bool sweeping {};
        std::vector<Controller*> cancelling;
    };

    State& state()
    {
        static NeverDestroyed<State> value;
        return *value;
    }

    constexpr size_t document_job_limit = 8192;
    constexpr size_t process_job_limit = 16384;
    constexpr size_t document_byte_limit = 32 * 1024 * 1024;
    constexpr size_t process_byte_limit = 64 * 1024 * 1024;
    constexpr size_t active_limit = 16;

    void pump();

    void schedule()
    {
        if (state().scheduled || state().draining || !state().submit || state().jobs.empty())
            return;
        state().scheduled = true;
        Core::deferred_invoke([] {
            state().scheduled = false;
            pump();
        });
    }

    std::pair<size_t, size_t> document_usage(Web::DOM::Document const* document)
    {
        size_t count = 0, bytes = 0;
        for (auto const& job : state().jobs)
            if (job->accounted && job->document.ptr() == document) {
                ++count;
                bytes += job->bytes;
            }
        return { count, bytes };
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
        return !state().draining && job.epoch == state().policy.epoch
            && MonotonicTime::now() < job.deadline && !job.params->is_canceled() && job.document
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
        if (job->accounted) {
            state().bytes -= job->bytes;
            job->accounted = false;
        }
        if (job->submitted) {
            --state().active;
            job->submitted = false;
        }
        schedule();
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
            cancel(*job->params);
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
        for (auto const& record : state().logical)
            if (record.controller.ptr() == params.controller())
                return record.id;
        sweep();
        if (state().logical.size() >= WebRequest::logical_request_limit
            || state().next_logical == 2147483647) {
            if (state().logical.size() >= WebRequest::logical_request_limit)
                WebRequest::logical_limit_diagnostic(
                    WebRequest::LogicalLimitStage::Gate, state().logical.size());
            return {};
        }
        auto id = ++state().next_logical;
        state().logical.push_back({ id, GC::Weak { *params.controller() }, GC::Weak { document } });
        WebRequest::logical_retention_diagnostic(
            WebRequest::LogicalLimitStage::Gate, state().logical.size());
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
        if (!current(*job) || !state().submit || state().jobs.size() >= process_job_limit)
            return false;
        auto usage = document_usage(job->document.ptr());
        if (usage.first >= document_job_limit)
            return false;
        auto data = metadata(*job->params, *job->document, job->response);
        if (!data.has_value())
            return false;
        auto bytes = data->serialized().byte_count();
        if (bytes > document_byte_limit - usage.second
            || bytes > process_byte_limit - state().bytes)
            return false;
        job->bytes = bytes;
        job->accounted = true;
        state().bytes += bytes;
        state().jobs.push_back(move(job));
        schedule();
        return true;
    }

    void pump()
    {
        if (state().pumping || state().draining || !state().submit)
            return;
        state().pumping = true;
        ScopeGuard done([] { state().pumping = false; });
        while (!state().draining && state().submit && state().active < active_limit) {
            auto found = std::find_if(state().jobs.begin(), state().jobs.end(),
                [](auto const& job) { return !job->submitted; });
            if (found == state().jobs.end())
                return;
            auto job = *found; // Retain roots across synchronous native reentry.
            if (!current(*job)) {
                settle(job, {});
                continue;
            }
            // Native identity, URL and headers must still be valid at promotion.
            auto data = metadata(*job->params, *job->document, job->response);
            auto usage = document_usage(job->document.ptr());
            auto bytes = data.has_value() ? data->serialized().byte_count() : 0;
            if (!data.has_value() || bytes > document_byte_limit - (usage.second - job->bytes)
                || bytes > process_byte_limit - (state().bytes - job->bytes)) {
                settle(job, {});
                continue;
            }
            auto original = data->get_object(
                job->stage == Stage::Headers ? "responseHeaders"sv : "requestHeaders"sv);
            if (!original.has_value()) {
                settle(job, {});
                continue;
            }
            state().bytes = state().bytes - job->bytes + bytes;
            job->bytes = bytes;
            job->original_headers = original.release_value();
            job->submitted = true;
            ++state().active;
            auto weak = std::weak_ptr<Job>(job);
            auto request = state().submit(
                job->stage, *job->document, data.release_value(), [weak](auto decision) {
                    if (auto owner = weak.lock()) {
                        owner->completed = true;
                        settle(move(owner), move(decision));
                    }
                });
            if (request.is_error()) {
                settle(job, {});
            } else if (job->submitted) {
                job->request = request.release_value();
            } else if (!job->completed && state().cancel) {
                // A peer may admit after reentrant shutdown/cancellation.
                state().cancel(request.release_value());
            }
        }
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
    if (state().draining || !next.epoch || next.epoch > 2147483647
        || next.epoch < state().policy.epoch)
        return false;
    if (next.epoch == state().policy.epoch)
        return next.before == state().policy.before && next.headers == state().policy.headers;
    state().draining = true;
    ScopeGuard resume([] { state().draining = false; });
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
    if (state().sweeping)
        return;
    state().sweeping = true;
    ScopeGuard done([] { state().sweeping = false; });
    if (auto after = state().diagnostics.due())
        std::fprintf(stderr,
            "FIREFLY_QUEUE_SNAPSHOT stage=web-request-gate after=%u jobs=%zu active=%zu "
            "waiting=%zu metadata_bytes=%zu logical=%zu epoch=%u\n",
            after, state().jobs.size(), state().active, state().jobs.size() - state().active,
            state().bytes, state().logical.size(), state().policy.epoch);
    auto jobs = state().jobs;
    for (auto& job : jobs)
        if (!current(*job)) {
            if (state().cancel && job->request)
                state().cancel(job->request);
            settle(job, {});
        }
    std::vector<u32> expired;
    std::erase_if(state().logical, [&](auto const& record) {
        if (record.controller.ptr() && record.document.ptr()
            && record.document.ptr()->is_fully_active())
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
    state().draining = true;
    auto jobs = state().jobs;
    for (auto& job : jobs) {
        if (state().cancel && job->request)
            state().cancel(job->request);
        settle(job, {});
    }
    state().logical.clear();
    state().submit = {};
    state().cancel = {};
    state().finish = {};
    state().draining = false;
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
    auto* controller = params.controller().ptr();
    if (std::find(state().cancelling.begin(), state().cancelling.end(), controller)
        != state().cancelling.end())
        return;
    state().cancelling.push_back(controller);
    ScopeGuard done([] { state().cancelling.pop_back(); });
    auto jobs = state().jobs;
    for (auto& job : jobs)
        if (job->params->controller().ptr() == controller) {
            if (state().cancel && job->request)
                state().cancel(job->request);
            settle(job, {});
        }

    finish(params);
}
}
