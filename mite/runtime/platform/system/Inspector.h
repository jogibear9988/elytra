/** Native inspector state queries. Mite does not install a debugger backend. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Mite {
GC::Ref<JS::Object> create_inspector_host(JS::Realm&);
}
