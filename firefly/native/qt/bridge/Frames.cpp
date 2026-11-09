/** Expose canonical frame snapshots to trusted main-process code, independently of preload IPC. */
#include "Impl.h"
#include "Value.h"
#include <LibJS/Runtime/NativeFunction.h>
#include <LibJS/Runtime/VM.h>
#include <UI/Qt/WebContentView.h>

namespace Firefly {
void Bridge::Impl::install_frames(JS::Object& host)
{
    auto object = JS::Object::create(realm, nullptr);
    object->define_native_function(
        realm, "snapshot"_utf16,
        [this](JS::VM& vm) -> JS::ThrowCompletionOr<JS::Value> {
            auto argument = vm.argument(0);
            if (!argument.is_int32() || argument.as_i32() <= 0)
                return preload_error(
                    vm, "ERR_INVALID_ARG_VALUE", "Invalid webContents frame owner"sv);
            auto found = windows.find(argument.as_i32());
            if (stopping || found == windows.end() || found->second.closing)
                return preload_error(
                    vm, "ERR_FIREFLY_OBJECT_DESTROYED", "Frame owner has been destroyed"sv);
            auto* view = found->second.view;
            auto snapshot = frames.snapshot(
                found->first, view->has_traversable() ? &view->traversable() : nullptr);
            if (snapshot.is_error()) {
                auto failure = snapshot.release_error();
                return preload_error(vm, "ERR_FIREFLY_FRAME_DATA",
                    failure.is_errno() ? "Cannot allocate frame snapshot"sv
                                       : failure.string_literal());
            }

            return BridgeDetail::text(vm, snapshot.release_value());
        },
        1, JS::default_attributes);
    host.define_direct_property("frames"_utf16, object, JS::default_attributes);
}
}
