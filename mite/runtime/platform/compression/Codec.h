/** Realm-owned streaming zlib handles; no borrowed JS memory survives a step. */
#pragma once
#include <LibJS/Runtime/Object.h>

namespace Mite {
GC::Ref<JS::Object> create_zlib_host(JS::Realm&);
}
