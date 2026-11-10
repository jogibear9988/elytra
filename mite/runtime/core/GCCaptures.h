/** Explicit native ownership for GC callbacks; JavaScript edges remain traceable. */
#pragma once
#include "NativeCaptureLifetime.h"
#include <LibGC/Function.h>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>

// These standard-library values contain native bytes only, never GC cells.
GC_UNTRACED_CAPTURE(std::string);
GC_UNTRACED_CAPTURE(std::filesystem::path);

namespace Mite {
/** Borrow an owner through a stable revocation anchor, never a dangling T*. */
template <typename T> struct NativeReference {
    std::shared_ptr<typename NativeCaptureOwner<std::remove_const_t<T>>::Lifetime> lifetime;

    T* try_get() const
    {
        return lifetime->owner;
    }

    T& get() const
    {
        // Execution remains subject to the embedding's live-owner admission.
        // Visitors, unlike callbacks, may run after native teardown.
        VERIFY(try_get());
        return *try_get();
    }

    void visit_edges(GC::Cell::Visitor& visitor) const
    {
        if (auto* owner = try_get())
            owner->visit_capture_edges(visitor);
    }
};

template <typename T> NativeReference<T> native_reference(T& value)
{
    return { value.native_capture_lifetime() };
}

/** Keep a native service alive while tracing its declared JavaScript dependencies. */
template <typename T> struct SharedCapture {
    std::shared_ptr<T> value;

    T* operator->() const
    {
        return value.get();
    }

    T& operator*() const
    {
        return *value;
    }

    operator std::shared_ptr<T> const&() const
    {
        return value;
    }

    void visit_edges(GC::Cell::Visitor& visitor) const
    {
        if constexpr (!GC::IsUntracedCapture<T>::value)
            value->visit_capture_edges(visitor);
    }
};

template <typename T> SharedCapture<T> shared_capture(std::shared_ptr<T> value)
{
    return { std::move(value) };
}
}
