/** The parent establishes containment before resume; this entry independently verifies it. */
#include "SDK.h"
#include <Services/RendererSandbox.h>

namespace RendererSandbox {
ErrorOr<void> apply_sandbox(StringView)
{
    return Firefly::WindowsHelpers::verify_current();
}
}
