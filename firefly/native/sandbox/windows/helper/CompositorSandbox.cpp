/** Verify confinement before the compositor accepts its browser control connection. */
#include "SDK.h"
#include <Compositor/Sandbox.h>

namespace Compositor {
ErrorOr<void> apply_sandbox(StringView, StringView, StringView)
{
    return Firefly::WindowsHelpers::verify_current();
}
}
