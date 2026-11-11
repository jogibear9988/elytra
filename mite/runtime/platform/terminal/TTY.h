/** Read-only terminal classification; no terminal handles or mode ownership. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_tty_host(JS::Realm&);
}
