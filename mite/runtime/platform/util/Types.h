/** Real engine brands for node:util/types; never invoke JavaScript during a predicate. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_util_types_host(JS::Realm&);

// Private predicates for clone admission; not additions to node:util/types.
GC::Ref<JS::Object> create_structured_clone_types_host(JS::Realm&);
}
