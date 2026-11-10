/** No unverified keyboard-layout fallback or fabricated US key map. */
#include "Provider.h"

namespace Mite::Keyboard {
ErrorOr<JsonObject> snapshot()
{
    return Error::from_string_literal("Native keyboard layout is unavailable on this platform");
}
}
