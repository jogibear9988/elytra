/** Native-only immutable scheme policy. No page/preload can configure process origins. */
#pragma once
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

// One atomic publication per process, before application documents. Validation
// failures do not consume registration or mutate the existing origin policy.
ErrorOr<void> install_origin_schemes(ReadonlySpan<OriginScheme>);

// Absent registration leaves upstream URL and secure-context behavior intact.
Optional<URL::Origin> custom_origin(URL::URL const&);

bool is_secure_origin(URL::Origin const&);
}
