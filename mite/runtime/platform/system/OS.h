/**
 * Collects OS information and translates it to owned JavaScript data. Native snapshots must
 * not expose platform pointers or share mutable result objects between calls.
 */

#pragma once
#include <LibJS/Runtime/Completion.h>
#include <LibJS/Runtime/Realm.h>

namespace Mite {
JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_os_constants(JS::Realm&);

JS::ThrowCompletionOr<GC::Ref<JS::Object>> create_os_host(JS::Realm&);
}
