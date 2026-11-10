/** Test-frame references stay mutable without becoming owning captures or dangling GC edges. */
#pragma once
#include "../../../mite/runtime/core/GCCaptures.h"
#include <AK/JsonArray.h>

namespace Firefly::Tests {
template <typename T> struct CaptureSlot : Mite::NativeCaptureOwner<CaptureSlot<T>> {
    explicit CaptureSlot(T& value)
        : value(value)
    {
    }

    T& value;

    void visit_capture_edges(GC::Cell::Visitor& visitor) const
    {
        if constexpr (requires { static_cast<bool>(value); }) {
            if (!static_cast<bool>(value))
                return;
        }

        if constexpr (std::is_arithmetic_v<T> || std::is_same_v<T, JsonArray>) {
            visitor.ignore(value);
        } else if constexpr (requires { value->realm(); }) {
            // The caller owns the native world. Trace only its current actual
            // realm; copying the RefPtr would prevent owner-drop GC assertions.
            if (value->realm())
                visitor.visit(*value->realm());
        } else if constexpr (requires { value->visit_edges(visitor); }) {
            visitor.visit(*value);
        } else if constexpr (requires {
                                 value.function;
                                 value.owner;
                             }) {
            visitor.visit(value.function);

            visitor.visit(value.owner);
        } else {
            static_assert(sizeof(T) == 0, "Declare the real cells held by this test slot");
        }
    }
};
}
