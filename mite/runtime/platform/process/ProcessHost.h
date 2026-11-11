/**
 * Declares native process-host construction and environment conversion. Returned objects
 * belong to the supplied realm and must not outlive runtime teardown.
 */

#pragma once
#include "platform/process/Exit.h"
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/Realm.h>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace Mite {
char const* native_platform();
// Owner-thread native snapshot; absent outside a worker. Never holds GC data.
std::optional<std::unordered_map<std::string, std::string>> worker_environment_snapshot();

JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_process(JS::VM&, JS::Realm&,
    std::shared_ptr<ProcessExitState> = {},
    std::unordered_map<std::string, std::string> const* worker_environment = nullptr);
}
