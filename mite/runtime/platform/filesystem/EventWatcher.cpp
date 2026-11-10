/** Native libuv filesystem events, preserving raw filename bytes and bounded handle ownership. */
#include "platform/filesystem/EventWatcher.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include "platform/filesystem/WatcherSupport.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <memory>
#include <unordered_map>

namespace Mite {
namespace {
    struct State;

    struct Watch {
        std::shared_ptr<State> owner;
        uint64_t id;
        uv_fs_event_t handle {};
        GC::Root<JS::FunctionObject> callback;
    };

    struct State {
        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            visitor.visit(realm);
            for (auto const& entry : active)
                visitor.visit(entry.second->callback.ptr());
        }

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
            // Callback roots and admission are revoked before native close. The
            // shared owner and handle storage survive until the real close callback.
            watch.callback = nullptr;
            uv_fs_event_stop(&watch.handle);
            uv_close(reinterpret_cast<uv_handle_t*>(&watch.handle), [](uv_handle_t* handle) {
                auto* watch = static_cast<Watch*>(handle->data);
                --watch->owner->handles;
                delete watch;
            });
        }
    };

    void changed(uv_fs_event_t* handle, char const* filename, int events, int status)
    {
        auto& watch = *static_cast<Watch*>(handle->data);
        auto& owner = *watch.owner;
        if (!watch.callback || owner.stopped)
            return;
        // libuv owns the filename only for this callback. Copy the actual byte name,
        // not a synthesized basename or UTF-8-decoded replacement. NULL stays NULL.
        size_t length {};
        if (filename) {
            while (length <= 1048576 && filename[length])
                ++length;
            if (length > 1048576) {
                status = UV_ENOBUFS;
                filename = nullptr;
            }
        }

        owner.loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
            GC::Root callback { watch.callback };
            GC::Root<JS::Uint8Array> bytes;
            if (filename) {
                bytes = TRY(JS::Uint8Array::create(owner.realm, length));
                bytes->viewed_array_buffer()->overwrite(0, filename, length);
            }

            return JS::call(owner.realm.vm(), *callback, JS::js_undefined(), JS::Value(status),
                JS::Value(events), bytes ? JS::Value(bytes.ptr()) : JS::js_null());
        });
    }
}

void install_event_watcher_host(JS::Realm& realm, JS::Object& host, EventLoop& loop)
{
    auto state = std::make_shared<State>(realm, loop);
    loop.on_shutdown([state] {
        state->stopped = true;
        while (!state->active.empty())
            state->close(*state->active.begin()->second);
    });
    host.define_native_function(realm, "fsWatchStart"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                auto path = TRY(WatcherSupport::path_argument(vm, vm.argument(0)));
                if (!vm.argument(1).is_boolean() || !vm.argument(2).is_boolean()
                    || !vm.argument(3).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Invalid internal event watcher arguments"_utf16);
                GC::Root result { JS::Object::create(state->realm, nullptr) };
                unsigned flags {};
                int status = state->stopped                                          ? UV_ECANCELED
                    : state->handles >= 256 || state->next_id >= 9007199254740992ULL ? UV_ENOSPC
                                                                                     : 0;
                if (vm.argument(2).as_bool()) {
#if defined(__APPLE__) || defined(_WIN32)
                    flags = UV_FS_EVENT_RECURSIVE;
#else
                // Node's Linux recursive directory-tree watcher is separate
                // userland machinery. libuv cannot implement that contract here.
                status = UV_ENOSYS;
#endif
                }

                uint64_t id {};
                if (!status) {
                    auto* watch = new Watch { state, state->next_id++, {},
                        GC::Root { vm.argument(3).as_function() } };
                    id = watch->id;
                    status = uv_fs_event_init(&state->loop.native_loop(), &watch->handle);
                    if (status)
                        delete watch;
                    else {
                        ++state->handles;
                        watch->handle.data = watch;
                        state->active.emplace(watch->id, watch);
                        status = uv_fs_event_start(&watch->handle, changed, path.c_str(), flags);
                        if (status)
                            state->close(*watch);
                        else if (!vm.argument(1).as_bool())
                            uv_unref(reinterpret_cast<uv_handle_t*>(&watch->handle));
                    }
                }

                result->define_direct_property(
                    "id"_utf16, JS::Value(static_cast<double>(id)), JS::default_attributes);
                result->define_direct_property(
                    "status"_utf16, JS::Value(status), JS::default_attributes);
                return result.ptr();
            }),
        4, JS::default_attributes);
    host.define_native_function(realm, "fsWatchStop"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (auto* watch = state->get(vm.argument(0)))
                    state->close(*watch);
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    host.define_native_function(realm, "fsWatchRef"_utf16,
        GC::create_function(realm.vm().heap(), GC::captures(shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (auto* watch = state->get(vm.argument(0))) {
                    if (vm.argument(1).to_boolean())
                        uv_ref(reinterpret_cast<uv_handle_t*>(&watch->handle));
                    else
                        uv_unref(reinterpret_cast<uv_handle_t*>(&watch->handle));
                }

                return JS::js_undefined();
            }),
        2, JS::default_attributes);
    host.define_native_function(
        realm, "fsWatchError"_utf16,
        +[](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            return WatcherSupport::error(vm, "watch", true);
        },
        2, JS::default_attributes);
}
}
