/** Native-only immutable scheme policy. No page/preload can configure process origins. */
#pragma once
#include <AK/ByteString.h>
#include <AK/Error.h>
#include <AK/Span.h>
#include <AK/String.h>
#include <LibURL/Origin.h>
#include <LibURL/URL.h>

namespace Firefly::Protocols {
struct OriginScheme {
    String name;
    bool standard { false };
    bool secure { false };
};

ErrorOr<void> validate_origin_schemes(ReadonlySpan<OriginScheme>);

// One atomic publication per process, before helper launch. Validation failures
// do not consume registration or mutate the existing origin policy.
ErrorOr<void> install_origin_schemes(ReadonlySpan<OriginScheme>);

// Only the native launcher reads this immutable snapshot; no environment fallback.
Optional<ByteString> configuration_for_helper();

// Launch seals an empty policy as well, preventing stale prelaunched helpers.
Optional<ByteString> configuration_for_helper_launch();

// Absent registration leaves upstream URL and secure-context behavior intact.
Optional<URL::Origin> custom_origin(URL::URL const&);

bool is_secure_origin(URL::Origin const&);

bool is_standard_scheme(StringView);
}
