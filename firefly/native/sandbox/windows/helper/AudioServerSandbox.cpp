/** Verify the native helper's confinement before accepting its controller IPC. */
#include "SDK.h"
#include <AudioServer/Sandbox.h>

namespace AudioServer {
ErrorOr<void> apply_sandbox(StringView)
{
    return Firefly::WindowsHelpers::verify_current();
}
}
