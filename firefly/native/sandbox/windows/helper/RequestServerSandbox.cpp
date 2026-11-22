/** Verify RequestServer's constrained outbound capability before accepting requests. */
#include "SDK.h"
#include <RequestServer/Sandbox.h>

namespace RequestServer {
ErrorOr<void> apply_sandbox(
    StringView, Vector<ByteString> const& certificates, Optional<StringView> disk_cache_directory)
{
    if (!certificates.is_empty() || disk_cache_directory.has_value())
        return Error::from_string_literal("Confined RequestServer cannot open external files");
    return Firefly::WindowsHelpers::verify_current(Firefly::WindowsHelpers::Role::RequestServer);
}
}
