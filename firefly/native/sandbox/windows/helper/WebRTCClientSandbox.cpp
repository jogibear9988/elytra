/** Verify the AppContainer and network policy before starting the Rust service. */
#include "SDK.h"
#include <WebRTCClient/Sandbox.h>

namespace WebRTCClient {
ErrorOr<void> apply_sandbox(StringView mach_server_name, NetworkAccess network_access)
{
    if (!mach_server_name.is_empty() || network_access != NetworkAccess::Denied)
        return Error::from_string_literal(
            "Windows WebRTC requires private IPC and disabled network");

    return Firefly::WindowsHelpers::verify_current(Firefly::WindowsHelpers::Role::WebRTCClient);
}
}
