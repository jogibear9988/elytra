/**
 * Declares native process-host construction and environment conversion. Returned objects
 * belong to the supplied realm and must not outlive runtime teardown.
 */

#pragma once
#include "platform/process/Exit.h"
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/Realm.h>
#include <memory>

namespace Mite {
char const* native_platform();

JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_process(
    JS::VM&, JS::Realm&, std::shared_ptr<ProcessExitState> = {});
}
