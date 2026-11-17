/** Safe argument rendering shared by bootstrap and the stream-backed global Console. */
#pragma once
#include <AK/Utf16String.h>
#include <LibJS/Runtime/Completion.h>

namespace JS {
class VM;
}

namespace Mite {
// Produces argument text only; never writes or invokes author object coercion.
// Bound: 1 MiB of UTF-8 argument text, excluding the Console newline/prefix.
JS::ThrowCompletionOr<Utf16String> format_console_arguments(JS::VM&);
}
