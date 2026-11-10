/** Passive native polling stays on the VM owner loop; native shutdown revokes all JS edges. */
#include "Host.h"
#include "Provider.h"
#include "core/EventLoop.h"
#include "core/GCCaptures.h"
#include <LibJS/Runtime/AbstractOperations.h>
#include <LibJS/Runtime/Error.h>
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <LibJS/Runtime/ValueInlines.h>
#include <memory>
#include <utility>

namespace Mite::Keyboard {
namespace {
    JS::Completion fail(JS::VM& vm, bool closed = false)
    {
        auto code = closed ? "ERR_MITE_KEYBOARD_CLOSED"sv
#ifdef __APPLE__
                           : "ERR_MITE_KEYBOARD_LAYOUT"sv;
#else
                           : "ERR_MITE_UNSUPPORTED_API"sv;
#endif
        auto object = JS::Error::create(vm.realm(),
            closed ? "Keyboard service is closed"_utf16
                   : "Native keyboard layout query failed"_utf16);
        object->define_direct_property("code"_utf16,
            JS::PrimitiveString::create(vm, Utf16String::from_utf8(code)), JS::default_attributes);
        return JS::throw_completion(object);
    }

    ErrorOr<String> read_snapshot()
    {
        auto value = TRY(snapshot());
        auto text = value.serialized();
        if (text.byte_count() > 512 * 1024)
            return Error::from_string_literal("Keyboard snapshot exceeds byte budget");
        return text;
    }

    struct State;

    struct Timer {
        uv_timer_t handle {};
        std::shared_ptr<State> owner;
    };

    struct State {
        JS::Realm& realm;
        EventLoop& loop;
        GC::Root<JS::FunctionObject> callback;
        Timer* timer {};
        String previous;
        bool stopped {};

        void visit_capture_edges(GC::Cell::Visitor& visitor) const
        {
            if (!stopped)
                visitor.visit(realm);
            visitor.visit(callback.ptr());
        }

        void stop()
        {
            stopped = true;
            callback = nullptr;
            if (auto* current = std::exchange(timer, nullptr)) {
                uv_timer_stop(&current->handle);
                uv_close(reinterpret_cast<uv_handle_t*>(&current->handle),
                    [](uv_handle_t* handle) { delete static_cast<Timer*>(handle->data); });
            }
        }
    };
}

GC::Ref<JS::Object> create_host(JS::Realm& realm, EventLoop& loop)
{
    auto state = std::make_shared<State>(realm, loop);
    loop.on_shutdown([state] { state->stop(); });
    auto host = JS::Object::create(realm, nullptr);
    host->define_native_function(realm, "read"_utf16,
        GC::create_function(realm.heap(), GC::captures(shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (state->stopped)
                    return fail(vm, true);
                auto result = read_snapshot();
                if (result.is_error())
                    return fail(vm);
                return JS::PrimitiveString::create(vm, Utf16String::from_utf8(result.value()));
            }),
        0, JS::default_attributes);
    host->define_native_function(realm, "watch"_utf16,
        GC::create_function(realm.heap(), GC::captures(shared_capture(state)),
            [](auto& state, JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
                if (state->stopped)
                    return fail(vm, true);
                if (!vm.argument(0).is_function())
                    return vm.throw_completion<JS::TypeError>(
                        "Keyboard listener must be callable"_utf16);
                auto initial = read_snapshot();
                if (initial.is_error())
                    return fail(vm);
                if (!state->timer) {
                    auto* timer = new Timer { {}, state.value };
                    auto status = uv_timer_init(&state->loop.native_loop(), &timer->handle);
                    if (status) {
                        delete timer;
                        return fail(vm);
                    }

                    timer->handle.data = timer;
                    state->timer = timer;
                    status = uv_timer_start(
                        &timer->handle,
                        [](uv_timer_t* handle) {
                            auto& owner = *static_cast<Timer*>(handle->data)->owner;
                            if (owner.stopped || !owner.callback)
                                return;
                            auto latest = read_snapshot();
                            if (latest.is_error()) {
                                owner.stop();
                                owner.loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                                    return fail(owner.realm.vm());
                                });
                                return;
                            }

                            if (latest.value() == owner.previous)
                                return;
                            owner.previous = latest.release_value();
                            GC::Root callback { owner.callback };
                            owner.loop.dispatch([&]() -> JS::ThrowCompletionOr<JS::Value> {
                                return JS::call(owner.realm.vm(), *callback, JS::js_undefined());
                            });
                        },
                        500, 500);
                    if (status) {
                        state->stop();
                        return fail(vm);
                    }

                    // native-keymap's referenced threadsafe listener keeps Node alive too.
                    // Keep this real polling source live until explicit runtime shutdown.
                }

                state->previous = initial.release_value();
                state->callback = vm.argument(0).as_function();
                return JS::js_undefined();
            }),
        1, JS::default_attributes);
    return host;
}
}
