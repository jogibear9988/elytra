/** Keep Qt's optional keyword macros out of upstream GC declarations. */
#pragma once
#pragma push_macro("slots")
#undef slots
#include "../../../mite/runtime/core/GCCaptures.h"
#pragma pop_macro("slots")
