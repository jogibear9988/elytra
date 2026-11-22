/** Verify the native helper's confinement before accepting its controller IPC. */
#include "SDK.h"
#include <MediaServer/Sandbox.h>

namespace MediaServer {
ErrorOr<void> apply_sandbox(StringView)
{
    return Firefly::WindowsHelpers::verify_current();
}
}
