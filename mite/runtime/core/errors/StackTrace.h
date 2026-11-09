/** Capture real LibJS frames for Mite's Error.captureStackTrace API. */
#pragma once
#include <LibJS/Runtime/Completion.h>

namespace JS {
class Realm;
}

namespace Mite {
JS::ThrowCompletionOr<void> install_error_stacks(JS::Realm&);
}
