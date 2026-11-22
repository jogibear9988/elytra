/** Private, synchronous helper-launch authority. No page/JS API or Core class-layout change. */
#pragma once
#include <AK/Error.h>

#if defined(FIREFLY_WINDOWS_HELPER_LAUNCH_BUILD)
#define FIREFLY_WINDOWS_HELPER_API __declspec(dllexport)
#else
#define FIREFLY_WINDOWS_HELPER_API __declspec(dllimport)
#endif

namespace Core {
struct ProcessSpawnOptions;
}

namespace Firefly::WindowsHelpers {
enum class Role {
    Unsupported,
    WebContent,
    ImageDecoder,
    Compositor,
    SystemProbe,
    RequestServer,
    AudioServer,
    MediaServer,
    VideoDecoderServer
};

class FIREFLY_WINDOWS_HELPER_API Scope {
public:
    Scope(Role, Core::ProcessSpawnOptions const&);

    ~Scope();

    Scope(Scope const&) = delete;
    Scope& operator=(Scope const&) = delete;

    Role role;
    Core::ProcessSpawnOptions const* options;
    Scope* previous;
};

FIREFLY_WINDOWS_HELPER_API bool has_launch_scope();

FIREFLY_WINDOWS_HELPER_API ErrorOr<void*> spawn(Core::ProcessSpawnOptions const&);
// Remove the exact HANDLE binding before the original Core destructor closes it.
FIREFLY_WINDOWS_HELPER_API bool release(void*);

FIREFLY_WINDOWS_HELPER_API ErrorOr<void> verify_current(Role = Role::WebContent);
}
