/** Sandboxed renderer process metadata only: no Mite, Node, filesystem or main-process capability.
 */
#pragma once
#include "native/preload/ProcessConfig.h"
#include <LibJS/Runtime/Completion.h>

namespace Firefly {
class IsolatedWorld;
}

namespace Firefly::PreloadProcess {
ErrorOr<void> prepare();

JS::ThrowCompletionOr<JS::Value> install(IsolatedWorld&, Configuration const&);
}
