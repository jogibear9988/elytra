/** Bounded libuv watchers with VM roots, exclusive Mite ownership and host disposition restore. */
#include "Signals.h"
#include "core/EventLoop.h"
#include <AK/NeverDestroyed.h>
#include <LibGC/Root.h>
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <csignal>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace Mite {
namespace {
    struct State;
    void closed(uv_handle_t*);

    struct Watch {
        std::shared_ptr<State> owner;
        uv_signal_t handle {};
        GC::Root<JS::FunctionObject> callback;
        bool active { false };
#ifndef _WIN32
        struct sigaction previous {};
        struct sigaction installed {};
#endif
    };

    struct Ownership {
        std::mutex mutex;
        std::unordered_map<int, Watch*> signals;
    };

    NeverDestroyed<Ownership> ownership;

    JS::Completion failure(JS::VM& vm, char const* code, char const* message)
    {
        GC::Root error { JS::Error::create(
            vm.realm(), Utf16String::from_utf8(StringView(message, strlen(message)))) };
        error->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(StringView(code, strlen(code)))),
            JS::default_attributes);
        return JS::throw_completion(error);
    }

    bool permitted(int number)
    {
        // Exclude fault/profiler/child-management signals, and uncatchable signals.
        return number == SIGINT || number == SIGTERM
#ifdef SIGPIPE
            || number == SIGPIPE
#endif
#ifdef SIGHUP
            || number == SIGHUP
#endif
#ifndef _WIN32
            || number == SIGUSR2
#endif
            ;
    }

#ifndef _WIN32
    bool same_handler(struct sigaction const& left, struct sigaction const& right)
    {
        if (left.sa_flags != right.sa_flags)
            return false;
        if (left.sa_flags & SA_SIGINFO)
            return left.sa_sigaction == right.sa_sigaction;
        return left.sa_handler == right.sa_handler;
    }
#endif

    struct State {
        JS::VM& vm;
        EventLoop& loop;
        bool allow;
        bool stopped { false };
        // Include deferred uv_close callbacks, not just active subscriptions.
        size_t handles { 0 };
        std::unordered_map<int, Watch*> watches;

        void close(int number)
        {
            auto found = watches.find(number);
            if (found == watches.end())
                return;
            auto* watch = found->second;
            watches.erase(found);
            {
                std::lock_guard lock(ownership->mutex);
                if (watch->active) {
                    watch->active = false;
#ifndef _WIN32
                    struct sigaction current {};
                    VERIFY(sigaction(number, nullptr, &current) == 0);
#endif
                    VERIFY(uv_signal_stop(&watch->handle) == 0);
#ifndef _WIN32
                    struct sigaction after {};
                    VERIFY(sigaction(number, nullptr, &after) == 0);
                    // Leave another live libuv watcher's handler in place. If a
                    // host replaced our handler, preserve its latest disposition.
                    bool replaced = !same_handler(current, watch->installed);
                    if (replaced)
                        VERIFY(sigaction(number, &current, nullptr) == 0);
                    else if (after.sa_handler == SIG_DFL)
                        VERIFY(sigaction(number, &watch->previous, nullptr) == 0);
#endif
                    VERIFY(ownership->signals.erase(number) == 1);
                }
            }

            watch->callback = nullptr;
            uv_close(reinterpret_cast<uv_handle_t*>(&watch->handle), closed);
        }
    };

    void closed(uv_handle_t* handle)
    {
        auto* watch = static_cast<Watch*>(handle->data);
        auto owner = watch->owner;
        VERIFY(owner->handles > 0);
        --owner->handles;
        delete watch;
    }

    void delivered(uv_signal_t* handle, int number)
    {
        auto* watch = static_cast<Watch*>(handle->data);
        if (!watch->active || watch->owner->stopped)
            return;
        auto owner = watch->owner;
        GC::Root callback { watch->callback };
        owner->loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
            return JS::call(owner->vm, *callback, JS::js_undefined(), JS::Value(number));
        });
    }
}

GC::Ref<JS::Object> create_signal_host(JS::Realm& realm, EventLoop& loop, bool allow)
{
    auto state = std::make_shared<State>(realm.vm(), loop, allow);
    loop.on_shutdown([state] {
        state->stopped = true;
        while (!state->watches.empty())
            state->close(state->watches.begin()->first);
    });
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(
        realm, "start"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_int32() || !vm.argument(1).is_function())
                return failure(vm, "ERR_INVALID_ARG_TYPE", "Invalid native signal subscription");
            int number = vm.argument(0).as_i32();
            if (!state->allow)
                return failure(vm, "ERR_MITE_SIGNAL_HOST_POLICY",
                    "This host has not enabled process signal subscriptions");
            if (!permitted(number))
                return failure(vm, "ERR_MITE_UNSUPPORTED_SIGNAL",
                    "This signal is not supported by Mite's native watcher policy");
            if (state->stopped || state->watches.contains(number))
                return failure(
                    vm, "ERR_MITE_SIGNAL_STATE", "Signal subscription is closed or already active");
            if (state->handles >= 32)
                return failure(vm, "ERR_MITE_SIGNAL_LIMIT", "Too many pending signal handles");
            std::lock_guard lock(ownership->mutex);
            if (ownership->signals.contains(number))
                return failure(
                    vm, "ERR_MITE_SIGNAL_OWNED", "Another runtime owns this process signal");
            auto* watch = new Watch { state, {}, GC::Root { vm.argument(1).as_function() } };
#ifndef _WIN32
            if (sigaction(number, nullptr, &watch->previous) != 0) {
                delete watch;
                return failure(
                    vm, "ERR_MITE_SIGNAL_STATE", "Cannot inspect host signal disposition");
            }

            if (watch->previous.sa_handler != SIG_DFL && watch->previous.sa_handler != SIG_IGN) {
                delete watch;
                return failure(
                    vm, "ERR_MITE_SIGNAL_OWNED", "An existing host handler owns this signal");
            }
#endif
            int status = uv_signal_init(&state->loop.native_loop(), &watch->handle);
            if (status) {
                delete watch;
                return failure(vm, uv_err_name(status), uv_strerror(status));
            }

            watch->handle.data = watch;
            ++state->handles;
            uv_unref(reinterpret_cast<uv_handle_t*>(&watch->handle));
            status = uv_signal_start(&watch->handle, delivered, number);
            if (status) {
                watch->callback = nullptr;
                uv_close(reinterpret_cast<uv_handle_t*>(&watch->handle), closed);
                return failure(vm, uv_err_name(status), uv_strerror(status));
            }

            watch->active = true;
#ifndef _WIN32
            VERIFY(sigaction(number, nullptr, &watch->installed) == 0);
#endif
            state->watches.emplace(number, watch);
            ownership->signals.emplace(number, watch);
            return JS::js_undefined();
        },
        2, JS::default_attributes);
    host->define_native_function(
        realm, "stop"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (!vm.argument(0).is_int32())
                return failure(vm, "ERR_INVALID_ARG_TYPE", "Invalid native signal number");
            state->close(vm.argument(0).as_i32());
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    return host;
}
}
