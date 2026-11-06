/** Bounded native inflate steps; caller-owned byte streams supply input/pressure. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Mite {
GC::Ref<JS::Object> create_inflate_host(JS::Realm&);
}
