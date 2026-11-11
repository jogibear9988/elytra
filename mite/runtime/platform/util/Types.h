/** Real engine brands for node:util/types; never invoke JavaScript during a predicate. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_util_types_host(JS::Realm&);
}
