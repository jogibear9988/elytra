/** Real libuv stat polling with bounded active/closing handles and owner-thread snapshots. */
#include "platform/filesystem/StatWatcher.h"
#include "core/EventLoop.h"
#include "platform/filesystem/StatSnapshot.h"
#include "platform/filesystem/WatcherSupport.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/TypedArray.h>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

namespace Mite {
namespace {
    struct State;

    struct Watch {
        std::shared_ptr<State> owner;
        uint64_t id;
        uv_fs_poll_t handle {};
        GC::Root<JS::FunctionObject> callback;
    };

    struct State {
        JS::Realm& realm;
        EventLoop& loop;
        std::unordered_map<uint64_t, Watch*> active;
        uint64_t next_id { 1 };
        size_t handles {};
        bool stopped {};

        ~State()
        {
            VERIFY(active.empty() && !handles);
        }

        Watch* get(JS::Value value)
        {
            if (!value.is_number() || !std::isfinite(value.as_double()) || value.as_double() < 1
                || value.as_double() >= 9007199254740992.0
                || std::floor(value.as_double()) != value.as_double())
                return nullptr;
            auto found = active.find(static_cast<uint64_t>(value.as_double()));
            return found == active.end() ? nullptr : found->second;
        }

        void close(Watch& watch)
        {
            active.erase(watch.id);
            // Retire JS roots synchronously; native storage remains until libuv has
            // drained any pending worker stat and internal timer-close callbacks.
            watch.callback = nullptr;
            uv_fs_poll_stop(&watch.handle);
            uv_close(reinterpret_cast<uv_handle_t*>(&watch.handle), [](uv_handle_t* handle) {
                auto* watch = static_cast<Watch*>(handle->data);
                --watch->owner->handles;
                delete watch;
            });
        }
    };

}

void install_stat_watcher_host(JS::Realm& realm, JS::Object& host, EventLoop& loop)
{
    auto state = std::make_shared<State>(realm, loop);
    loop.on_shutdown([state] {
        state->stopped = true;
        while (!state->active.empty())
            state->close(*state->active.begin()->second);
    });
    host.define_native_function(
        realm, "statWatchStart"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto path = TRY(WatcherSupport::path_argument(vm, vm.argument(0)));
            auto interval = vm.argument(1);
            if (!interval.is_number() || !std::isfinite(interval.as_double())
                || interval.as_double() < 0 || interval.as_double() > 0xffffffffu
                || std::floor(interval.as_double()) != interval.as_double()
                || !vm.argument(3).is_function())
                return vm.throw_completion<JS::TypeError>(
                    "Invalid internal stat watcher arguments"sv);
            GC::Root result { JS::Object::create(state->realm, nullptr) };
            int status = state->stopped                                          ? UV_ECANCELED
                : state->handles >= 256 || state->next_id >= 9007199254740992ULL ? UV_ENOSPC
                                                                                 : 0;
            uint64_t id {};
            if (!status) {
                auto* watch = new Watch { state, state->next_id++, {},
                    GC::Root { vm.argument(3).as_function() } };
                id = watch->id;
                status = uv_fs_poll_init(&state->loop.native_loop(), &watch->handle);
                if (status)
                    delete watch;
                else {
                    ++state->handles;
                    watch->handle.data = watch;
                    state->active.emplace(watch->id, watch);
                    status = uv_fs_poll_start(
                        &watch->handle,
                        [](uv_fs_poll_t* handle, int status, uv_stat_t const* previous,
                            uv_stat_t const* current) {
                            auto& watch = *static_cast<Watch*>(handle->data);
                            auto& owner = *watch.owner;
                            if (!watch.callback || owner.stopped)
                                return;
                            owner.loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                                GC::Root callback { watch.callback };
                                uv_stat_t zero {};
                                GC::Root current_raw { make_stat_snapshot(
                                    owner.realm, current ? *current : zero) };
                                GC::Root previous_raw { make_stat_snapshot(
                                    owner.realm, previous ? *previous : zero) };
                                return JS::call(owner.realm.vm(), *callback, JS::js_undefined(),
                                    JS::Value(status), current_raw.ptr(), previous_raw.ptr());
                            });
                        },
                        path.c_str(), static_cast<unsigned>(interval.as_double()));
                    if (status)
                        state->close(*watch);
                    else if (!vm.argument(2).to_boolean())
                        uv_unref(reinterpret_cast<uv_handle_t*>(&watch->handle));
                }
            }

            result->define_direct_property(
                "id"_utf16, JS::Value(static_cast<double>(id)), JS::default_attributes);
            result->define_direct_property(
                "status"_utf16, JS::Value(status), JS::default_attributes);
            return result.ptr();
        },
        4, JS::default_attributes);
    host.define_native_function(
        realm, "statWatchError"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return WatcherSupport::error(vm, "watchFile");
        },
        2, JS::default_attributes);
    host.define_native_function(
        realm, "statWatchStop"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (auto* watch = state->get(vm.argument(0)))
                state->close(*watch);
            return JS::js_undefined();
        },
        1, JS::default_attributes);
    host.define_native_function(
        realm, "statWatchRef"_utf16,
        [state](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            if (auto* watch = state->get(vm.argument(0))) {
                if (vm.argument(1).to_boolean())
                    uv_ref(reinterpret_cast<uv_handle_t*>(&watch->handle));
                else
                    uv_unref(reinterpret_cast<uv_handle_t*>(&watch->handle));
            }

            return JS::js_undefined();
        },
        2, JS::default_attributes);
}
}
