/** Authenticated native-only process control channel, separate from preload IPC. */
#pragma once
#include "native/protocol/OriginConfig.h"

namespace Firefly::HelperWebRequest {
ErrorOr<Protocols::OriginArguments> prepare(ReadonlySpan<StringView>);

void initialize();

void shutdown();
}
