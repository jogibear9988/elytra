/** Native process policy controls interception, never file-origin or filesystem authority. */
#pragma once
#include "OriginConfig.h"

namespace Firefly::Protocols {
struct FilePolicySnapshot {
    u64 generation {};
    bool active {};
};

FilePolicySnapshot file_policy_snapshot();

// Old control messages cannot roll back a newer registration. Equal messages are idempotent.
bool apply_file_policy(u64 generation, bool active);

ByteString file_policy_for_helper_launch();

ErrorOr<OriginArguments> consume_file_policy_configuration(ReadonlySpan<StringView>);
}
