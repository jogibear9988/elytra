/** Own SQLite callbacks on the VM thread; execute database work through the worker queue. */
#include "Host.h"
#include "Store.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include <AK/JsonValue.h>
#include <AK/ScopeGuard.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <cstring>
#include <deque>
#include <memory>
#include <sqlite3.h>

namespace Mite::SQLite {
namespace {
    JS::Completion fail(JS::VM& vm, StringView code, StringView message)
    {
        auto error = JS::Error::create(*vm.current_realm(), Utf16String::from_utf8(message));
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(code)), JS::default_attributes);
        return JS::throw_completion(error);
    }

    struct Job {
        JsonObject input;
        JsonObject result;
        GC::Ptr<JS::FunctionObject> callback;
        size_t bytes;

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            visitor.visit(callback);
        }
    };

    struct State : public std::enable_shared_from_this<State> {
        JS::Realm& realm;
        EventLoop& loop;
        std::shared_ptr<Store> store { std::make_shared<Store>() };
        std::deque<std::shared_ptr<Job>> queue;
        size_t bytes {};
        int next_database { 1 };
        bool active {};
        bool stopped {};

        State(JS::Realm& realm, EventLoop& loop)
            : realm(realm)
            , loop(loop)
        {
        }

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            visitor.visit(realm);
            for (auto const& job : queue)
                visitor.visit(job->callback);
        }

        void stop()
        {
            if (stopped)
                return;
            stopped = true;
            store->stop();
            for (auto const& job : queue)
                job->callback = nullptr;
            queue.clear();
            bytes = 0;
        }

        JS::ThrowCompletionOr<void> start()
        {
            if (active || stopped || queue.empty())
                return {};
            auto job = queue.front();
            auto self = shared_from_this();
            auto database = store;
            active = true;
            auto result
                = loop.queue_work([database, job] { job->result = database->execute(job->input); },
                    GC::create_function(realm.vm().heap(),
                        GC::captures(shared_capture(self), shared_capture(job)),
                        [](auto& self, auto& job) -> JS::ThrowCompletionOr<JS::Value> {
                            self->active = false;
                            if (self->stopped)
                                return JS::js_undefined();
                            VERIFY(!self->queue.empty()
                                && self->queue.front().get() == job.value.get());
                            self->queue.pop_front();
                            self->bytes -= job->bytes;
                            GC::Root callback { job->callback };
                            job->callback = nullptr;
                            // Advance the accepted FIFO even if this callback throws.
                            // Reentrant submissions append behind already accepted work.
                            TRY(self->start());
                            if (!callback)
                                return JS::js_undefined();
                            auto payload = job->result.serialized();
                            return JS::call(self->realm.vm(), *callback, JS::js_undefined(),
                                JS::PrimitiveString::create(
                                    self->realm.vm(), Utf16String::from_utf8(payload)));
                        }));
            if (result.is_error()) {
                active = false;
                queue.pop_front();
                bytes -= job->bytes;
                job->callback = nullptr;
                return result.release_error();
            }

            return {};
        }
    };
}

GC::Ref<JS::Object> create_host(JS::Realm& realm, EventLoop& loop)
{
    auto state = std::make_shared<State>(realm, loop);
    loop.on_shutdown([state] { state->stop(); });
    GC::Root host { JS::Object::create(realm, nullptr) };
    host->define_direct_property("version"_utf16,
        JS::PrimitiveString::create(realm.vm(),
            Utf16String::from_utf8(
                StringView { sqlite3_libversion(), strlen(sqlite3_libversion()) })),
        JS::default_attributes);
    host->define_native_function(realm, "submit"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (!vm.argument(0).is_string() || !vm.argument(1).is_function())
                    return fail(vm, "ERR_INVALID_ARG_TYPE"sv,
                        "SQLite request requires JSON and callback"sv);
                auto view = vm.argument(0).as_string().utf16_string_view();
                if (view.length_in_code_units() > 2 * 1024 * 1024)
                    return fail(vm, "ERR_MITE_SQLITE_LIMIT"sv, "SQLite request exceeds 2 MiB"sv);
                auto text = view.to_utf8(AllowLonelySurrogates::No);
                if (text.is_error() || text.value().byte_count() > 2 * 1024 * 1024)
                    return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid SQLite request text"sv);
                auto parsed = JsonValue::from_string(text.value().bytes_as_string_view());
                if (parsed.is_error() || !parsed.value().is_object())
                    return fail(vm, "ERR_INVALID_ARG_VALUE"sv, "Invalid SQLite request object"sv);
                if (state->stopped)
                    return fail(vm, "ERR_MITE_SQLITE_CLOSED"sv, "SQLite service has stopped"sv);
                if (state->queue.size() >= 128
                    || state->bytes + text.value().byte_count() > 16 * 1024 * 1024)
                    return fail(
                        vm, "ERR_MITE_SQLITE_LIMIT"sv, "SQLite pending work limit exceeded"sv);
                auto input = parsed.release_value().as_object();
                if (input.get_string("op"sv) == "open"sv) {
                    if (state->next_database == 2147483647)
                        return fail(
                            vm, "ERR_MITE_SQLITE_LIMIT"sv, "SQLite identity limit exceeded"sv);
                    input.set("db"sv, state->next_database++);
                }

                auto job = std::make_shared<Job>(move(input), JsonObject {},
                    &vm.argument(1).as_function(), text.value().byte_count());
                state->queue.push_back(job);
                state->bytes += job->bytes;
                TRY(state->start());
                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    return *host;
}
}
