/** Private native volume capability query for the audited fs-copyfile adapter. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Mite {
GC::Ref<JS::Object> create_file_clone_host(JS::Realm&);
}
