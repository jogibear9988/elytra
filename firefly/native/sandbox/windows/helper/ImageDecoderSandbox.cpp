/** Verify the parent's native confinement before accepting image-decoder IPC. */
#include "SDK.h"
#include <ImageDecoder/Sandbox.h>

namespace ImageDecoder {
ErrorOr<void> apply_sandbox(StringView)
{
    return Firefly::WindowsHelpers::verify_current();
}
}
