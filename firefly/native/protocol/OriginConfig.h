/** Bounded native parent/helper configuration. This is not a renderer/preload API. */
#pragma once
#include "OriginPolicy.h"
#include <AK/ByteString.h>
#include <AK/Vector.h>

namespace Firefly::Protocols {
constexpr size_t origin_config_limit = 8192;

ErrorOr<Vector<OriginScheme>> parse_origin_configuration(StringView);

ErrorOr<ByteString> serialize_origin_configuration(ReadonlySpan<OriginScheme>);

// Own all argv storage through the upstream entry call; no views borrow a temporary JSON buffer.
struct OriginArguments {
    Vector<ByteString> owned;
    Vector<char*> argv;
    Vector<StringView> strings;
};

// Validate all arguments/duplicates before atomically installing a supplied policy.
// Remove only the owned pre-delimiter option; preserve every remaining argument.
ErrorOr<OriginArguments> consume_origin_configuration(ReadonlySpan<StringView>);
}
