/** Shared native lifetime anchors; independent of LibJS, LibGC and Firefly. */
#pragma once
#include <memory>

namespace Mite {
/** Revoke borrowed captures before an owner's members or VM services retire. */
template <typename T> class NativeCaptureOwner {
public:
    struct Lifetime {
        T* owner;
    };

    NativeCaptureOwner()
        : m_capture_lifetime(std::make_shared<Lifetime>(Lifetime { static_cast<T*>(this) }))
    {
    }

    NativeCaptureOwner(NativeCaptureOwner const&) = delete;
    NativeCaptureOwner& operator=(NativeCaptureOwner const&) = delete;
    NativeCaptureOwner(NativeCaptureOwner&&) = delete;
    NativeCaptureOwner& operator=(NativeCaptureOwner&&) = delete;

    ~NativeCaptureOwner()
    {
        revoke_native_captures();
    }

    std::shared_ptr<Lifetime> native_capture_lifetime() const
    {
        return m_capture_lifetime;
    }

protected:
    // Owners with an explicit destructor/stop phase call this FIRST, before any
    // hook can force GC while their root vectors/other native members are retired.
    void revoke_native_captures()
    {
        m_capture_lifetime->owner = nullptr;
    }

private:
    std::shared_ptr<Lifetime> m_capture_lifetime;
};

}
