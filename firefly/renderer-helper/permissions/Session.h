/** Startup configuration and authenticated private permission control for the helper process. */
#pragma once
#include "native/protocol/OriginConfig.h"

namespace Firefly::HelperPermissions {
ErrorOr<Protocols::OriginArguments> prepare(ReadonlySpan<StringView>);

void initialize();

void shutdown();
}
