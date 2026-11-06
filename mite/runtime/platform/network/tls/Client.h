/** Owner-thread memory-BIO TLS engine; TCP and public stream policy stay in JS/libuv. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_tls_host(JS::Realm&);
}
