/** Verify the native helper's confinement before accepting its controller IPC. */
#include "SDK.h"
#include <VideoDecoderServer/Sandbox.h>

namespace VideoDecoderServer {
ErrorOr<void> apply_sandbox(StringView)
{
    return Firefly::WindowsHelpers::verify_current();
}
}
