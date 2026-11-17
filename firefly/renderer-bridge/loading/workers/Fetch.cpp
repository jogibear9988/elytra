/** Keep all Fetch/GC objects on their worker; relay only copied native packets and decisions. */
#include "Fetch.h"
#include "Scope.h"
#include "loading/protocol/Response.h"
#include "native/protocol/FilePolicy.h"
#include "native/protocol/OriginPolicy.h"
#include "native/protocol/files/FilePacket.h"
#include "native/web-request/Headers.h"
#include <AK/NeverDestroyed.h>
#include <AK/StringBuilder.h>
#include <AK/Utf8View.h>
#include <LibGC/Root.h>
#include <LibGC/Weak.h>
#include <LibHTTP/HeaderList.h>
#include <LibURL/Parser.h>
#include <LibWeb/Fetch/Fetching/ClientContextSnapshots.h>
#include <LibWeb/Fetch/Infrastructure/ClientContextSnapshot.h>
#include <LibWeb/HTML/Scripting/Environments.h>
#include <atomic>
#include <map>
#include <memory>

namespace Firefly::WorkersFetch {
namespace {
    using Stage = WebRequestGate::Stage;

    struct Job {
        u32 key {}, epoch {};
        Stage stage {};
        RefPtr<Params const> params;
        GC::Root<Pending> pending;
        GC::Root<WebRequestGate::Before> before;
        GC::Root<WebRequestGate::Head> head;
        RefPtr<Response> response;
        RefPtr<ProtocolFetch::Response> protocol;
        JsonObject original_headers;
        size_t bytes {};
    };

    struct Logical {
        u32 key;
        GC::Weak<Web::Fetch::Infrastructure::FetchController> controller;
    };

    struct State {
        AK_ALLOC_WITH_KMALLOC;
        std::shared_ptr<Workers::Scope> scope;
        std::map<u32, Job> jobs;
        Vector<Logical> logical;
        u32 next {}, next_job {};
    };

    OwnPtr<State>& worker_state()
    {
        static thread_local NeverDestroyed<OwnPtr<State>> value;
        return *value;
    }

    std::atomic<size_t> retained_bytes { 0 }, retained_jobs { 0 };
    NeverDestroyed<Open> client_for;
    using Relay = Endpoint;

    // Owner-thread only. No Client/GC pointer is ever captured by a cross-thread task.
    auto& owner_relays()
    {
        static NeverDestroyed<std::map<std::pair<u64, u32>, Relay>> value;
        return *value;
    }

    bool reserve(std::atomic<size_t>& ledger, size_t size, size_t limit)
    {
        auto used = ledger.load();
        do {
            if (size > limit - used)
                return false;
        } while (!ledger.compare_exchange_weak(used, used + size));
        return true;
    }

    Optional<u32> logical_key(Params const& params)
    {
        if (!worker_state() || !worker_state()->scope || worker_state()->scope->revoked())
            return {};
        for (auto const& item : worker_state()->logical)
            if (item.controller.ptr() == params.controller())
                return item.key;
        if (worker_state()->logical.size() >= 8192 || worker_state()->next == 2147483647)
            return {};
        auto id = ++worker_state()->next;
        worker_state()->logical.append({ id, GC::Weak { *params.controller() } });
        return id;
    }

    void erase(u32 id)
    {
        auto found = worker_state()->jobs.find(id);
        if (found == worker_state()->jobs.end())
            return;
        retained_bytes.fetch_sub(found->second.bytes);
        retained_jobs.fetch_sub(1);
        worker_state()->jobs.erase(found);
    }

    void reject(u32 id)
    {
        if (!worker_state())
            return;
        auto found = worker_state()->jobs.find(id);
        if (found == worker_state()->jobs.end())
            return;
        auto job = move(found->second);
        retained_bytes.fetch_sub(job.bytes);
        retained_jobs.fetch_sub(1);
        worker_state()->jobs.erase(found);
        job.params->stop_body_transport();
        if (job.response)
            job.response->release_request_transfer_lease();
        if (job.protocol)
            job.protocol->stop();
        auto error = Response::network_error("Worker fetch scope was denied or retired"_string);
        if (job.pending && !job.pending->is_resolved())
            job.pending->resolve(error);
        else if (job.head)
            job.head->function()(move(error));
    }

    bool apply(Job& job, Optional<JsonObject> const& decision)
    {
        if (!worker_state() || !worker_state()->scope || worker_state()->scope->revoked()
            || job.params->is_canceled() || job.epoch != WebRequestGate::policy().epoch
            || !decision.has_value()
            || WebRequest::validate_decision(
                *decision, &job.original_headers, job.stage == Stage::Before)
                .is_error())
            return false;
        auto canceled = decision->get_bool("cancel"sv);
        if (!canceled.has_value() || *canceled)
            return false;
        auto values = decision->get_object(
            job.stage == Stage::Before ? "requestHeaders"sv : "responseHeaders"sv);
        if (!values.has_value())
            return true;
        auto list = HTTP::HeaderList::create();
        bool valid = true;
        values->for_each_member([&](auto const& name, auto const& items) {
            if (!valid || !items.is_array()) {
                valid = false;
                return;
            }

            for (auto const& item : items.as_array().values()) {
                if (!item.is_string()) {
                    valid = false;
                    return;
                }

                StringBuilder decoded;
                for (auto point : Utf8View(item.as_string()))
                    decoded.append(static_cast<char>(point));
                list->append({ name.to_byte_string(), decoded.to_byte_string() });
            }
        });
        if (!valid)
            return false;
        if (job.stage == Stage::Before)
            job.params->request()->set_header_list(move(list));
        else
            job.response->set_header_list(move(list));
        return true;
    }

    void decided(u32 id, Optional<JsonObject> decision)
    {
        if (!worker_state())
            return;
        auto found = worker_state()->jobs.find(id);
        if (found == worker_state()->jobs.end())
            return;
        if (!apply(found->second, decision)) {
            reject(id);
            return;
        }

        auto job = move(found->second);
        retained_bytes.fetch_sub(job.bytes);
        retained_jobs.fetch_sub(1);
        worker_state()->jobs.erase(found);
        if (job.before) {
            auto pending = job.pending;
            job.before->function()()->when_loaded(GC::create_function(GC::Heap::the(),
                GC::captures(pending), [](auto& pending, NonnullRefPtr<Response> response) {
                    pending->resolve(move(response));
                }));
        } else
            job.head->function()(job.response.release_nonnull());
    }

    bool queue_policy(Job job)
    {
        auto key = logical_key(*job.params);
        auto data = WebRequestGate::describe(*job.params, job.response);
        if (!key.has_value() || !data.has_value() || worker_state()->next_job == 2147483647)
            return false;
        auto bytes = data->serialized().byte_count();
        if (!reserve(retained_bytes, bytes, 32 * 1024 * 1024))
            return false;
        if (!reserve(retained_jobs, 1, 16384)) {
            retained_bytes.fetch_sub(bytes);
            return false;
        }

        auto id = ++worker_state()->next_job;
        job.key = *key;
        job.bytes = bytes;
        job.epoch = WebRequestGate::policy().epoch;
        auto original = data->get_object(
            job.stage == Stage::Before ? "requestHeaders"sv : "responseHeaders"sv);
        job.original_headers = original.release_value();
        auto scope = worker_state()->scope;
        auto stage = job.stage;
        worker_state()->jobs.emplace(id, move(job));
        bool sent = scope->post_owner(
            [scope, key = *key, id, stage, data = data.release_value()]() mutable {
                auto document = Workers::owner_document(*scope);
                bool accepted = document
                    && WebRequestGate::submit_worker(
                        stage, *document, scope->token(), key, move(data),
                        [scope, id](Optional<JsonObject> decision) {
                            scope->post_worker([id, decision = move(decision)]() mutable {
                                decided(id, move(decision));
                            });
                        },
                        [scope] { return !!Workers::owner_document(*scope); });
                if (!accepted)
                    scope->post_worker([id] { reject(id); });
            });
        if (!sent)
            reject(id);
        return true;
    }
}

void install_client_provider(Open provider)
{
    client_for.get() = move(provider);
}

void enter()
{
    VERIFY(!worker_state());
    worker_state() = make<State>();
    worker_state()->scope = Workers::current();
}

void retire()
{
    if (!worker_state())
        return;
    while (!worker_state()->jobs.empty())
        reject(worker_state()->jobs.begin()->first);
    worker_state()->logical.clear();
}

void leave()
{
    if (!worker_state())
        return;
    while (!worker_state()->jobs.empty())
        reject(worker_state()->jobs.begin()->first);
    for (auto const& logical : worker_state()->logical)
        if (worker_state()->scope) {
            auto scope = worker_state()->scope;
            scope->post_owner(
                [scope, key = logical.key] { WebRequestGate::cancel_worker(scope->token(), key); });
        }

    worker_state() = nullptr;
}

GC::Ref<Pending> before(
    JS::Realm&, Params const& params, GC::Ref<WebRequestGate::Before> continuation)
{
    // Chromium selects the Blob loader separately for a dedicated worker's main
    // script. Preserve SDK Blob-store/origin/MIME checks; no network policy is bypassed.
    if (params.request()->current_url().scheme() == "blob"sv
        && params.request()->destination()
            == Web::Fetch::Infrastructure::Request::Destination::Worker)
        return continuation->function()();
    auto pending = Pending::create(params);
    if (!worker_state() || !worker_state()->scope || worker_state()->scope->revoked()) {
        pending->resolve(Response::network_error("No admitted worker fetch scope"_string));
        return pending;
    }

    if (!logical_key(params).has_value()) {
        pending->resolve(Response::network_error("Worker logical request budget exhausted"_string));
        return pending;
    }

    if (!WebRequestGate::policy().before)
        return continuation->function()();
    Job job;
    job.params = params;
    job.pending = pending;
    job.before = continuation;
    job.stage = Stage::Before;
    if (!queue_policy(move(job)))
        pending->resolve(Response::network_error("Worker policy queue exhausted"_string));
    return pending;
}

bool headers(
    Params const& params, NonnullRefPtr<Response> response, GC::Ref<WebRequestGate::Head> head)
{
    Job job;
    job.params = params;
    job.response = response;
    job.head = head;
    job.stage = Stage::Headers;
    if (!queue_policy(move(job))) {
        response->release_request_transfer_lease();
        params.stop_body_transport();
        head->function()(Response::network_error("Worker header policy queue exhausted"_string));
    }

    return true;
}

void finish(Params const& params)
{
    if (!worker_state())
        return;
    for (size_t i = 0; i < worker_state()->logical.size(); ++i)
        if (worker_state()->logical[i].controller.ptr() == params.controller()) {
            auto key = worker_state()->logical[i].key;
            auto scope = worker_state()->scope;
            worker_state()->logical.remove(i);
            if (scope)
                scope->post_owner(
                    [scope, key] { WebRequestGate::cancel_worker(scope->token(), key); });
            return;
        }
}

void cancel(Params const& params)
{
    if (!worker_state())
        return;
    Vector<u32> ids;
    for (auto const& [id, job] : worker_state()->jobs)
        if (job.params->controller() == params.controller())
            ids.append(id);
    for (auto id : ids)
        reject(id);
    finish(params);
}

GC::Ref<Pending> dispatch(Params const& params)
{
    auto pending = Pending::create(params);
    auto key = logical_key(params);
    auto const& request = *params.request();
    auto client = request.client_snapshot();
    auto origin = request.origin().get_pointer<URL::Origin>();
    if (!key.has_value() || !client || !origin || request.is_navigation_request()
        || !request.method().is_one_of("GET"sv, "HEAD"sv)
        || request.header_list()->contains("Range"sv) || !origin->is_same_origin(client->origin)
        || !origin->is_same_origin(request.current_url().origin())) {
        pending->resolve(
            Response::network_error("Worker protocol request outside admitted scope"_string));
        return pending;
    }

    if (worker_state()->next_job == 2147483647 || !reserve(retained_jobs, 1, 16384)) {
        pending->resolve(Response::network_error("Worker protocol job budget exhausted"_string));
        return pending;
    }

    auto retained_url = request.current_url().serialize();
    auto url_bytes = retained_url.byte_count();
    if (url_bytes > 65536 || !reserve(retained_bytes, url_bytes, 32 * 1024 * 1024)) {
        retained_jobs.fetch_sub(1);
        pending->resolve(Response::network_error("Worker protocol URL budget exhausted"_string));
        return pending;
    }

    Job job;
    job.bytes = url_bytes;
    job.key = *key;
    key = ++worker_state()->next_job;
    job.params = params;
    job.pending = pending;
    worker_state()->jobs.emplace(*key, move(job));
    auto scope = worker_state()->scope;
    auto url = move(retained_url);
    bool sent = scope->post_owner([scope, key = *key, url = move(url),
                                      head = request.method() == "HEAD"sv] {
        auto document = Workers::owner_document(*scope);
        auto parsed = URL::Parser::basic_parse(url);
        auto deny = [&] { scope->post_worker([key] { reject(key); }); };
        if (!document || !parsed.has_value() || !client_for.get()) {
            deny();
            return;
        }

        auto route = std::pair { scope->token(), key };
        auto admitted = client_for.get()(*document, *parsed, head,
            { .packet =
                    [scope, key, route](ReadonlyBytes packet) {
                        auto bytes = ByteBuffer::copy(packet);
                        if (bytes.is_error())
                            return false;
                        auto copy = std::make_shared<ByteBuffer>(bytes.release_value());
                        auto decoded = Protocols::decode_file_packet(packet);
                        if (decoded.is_error())
                            return false;
                        bool terminal = decoded.value().kind == Protocols::FilePacketKind::Complete
                            || decoded.value().kind == Protocols::FilePacketKind::Cancel;
                        bool sent = scope->post_worker([key, copy] {
                            if (!worker_state())
                                return;
                            auto found = worker_state()->jobs.find(key);
                            if (found == worker_state()->jobs.end() || !found->second.protocol)
                                return;
                            auto response = found->second.protocol;
                            if (response->receive(copy->bytes()).is_error())
                                reject(key);
                        });
                        if (sent && terminal)
                            owner_relays().erase(route);
                        return sent;
                    },
                .closed =
                    [scope, key, route] {
                        owner_relays().erase(route);
                        scope->post_worker([key] { reject(key); });
                    },
                .valid = [scope] { return !!Workers::owner_document(*scope); } });
        if (admitted.is_error()) {
            deny();
            return;
        }

        auto endpoint = admitted.release_value();
        if (endpoint.epoch != scope->generation()) {
            endpoint.cancel();
            deny();
            return;
        }

        auto id = endpoint.request;
        auto epoch = endpoint.epoch;
        owner_relays().emplace(route, move(endpoint));
        if (!scope->post_worker([scope, key, epoch, id] {
                if (!worker_state() || !worker_state()->jobs.contains(key))
                    return;
                auto& job = worker_state()->jobs.find(key)->second;
                auto response = ProtocolFetch::Response::create(*job.params, epoch, id,
                    { .credit =
                            [scope, key](u64 count) {
                                scope->post_owner([scope, key, count] {
                                    auto found = owner_relays().find({ scope->token(), key });
                                    if (found != owner_relays().end())
                                        found->second.credit(count);
                                });
                            },
                        .cancel =
                            [scope, key] {
                                scope->post_owner([scope, key] {
                                    auto found = owner_relays().find({ scope->token(), key });
                                    if (found == owner_relays().end())
                                        return;
                                    auto relay = found->second;
                                    owner_relays().erase(found);
                                    relay.cancel();
                                });
                            },
                        .completed =
                            [scope, key] {
                                scope->post_owner([scope, key] {
                                    owner_relays().erase({ scope->token(), key });
                                });
                                erase(key);
                            } });
                if (response.is_error()) {
                    reject(key);
                    return;
                }

                job.protocol = response.release_value();
                auto pending = job.pending;
                job.protocol->when_loaded(GC::create_function(GC::Heap::the(),
                    GC::captures(pending), [](auto& pending, NonnullRefPtr<Response> response) {
                        pending->resolve(move(response));
                    }));
            })) {
            auto found = owner_relays().find(route);
            if (found != owner_relays().end()) {
                auto cancel = move(found->second.cancel);
                owner_relays().erase(found);
                cancel();
            }
        }
    });
    if (!sent)
        reject(*key);
    return pending;
}
}
