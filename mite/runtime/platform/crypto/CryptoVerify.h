/** Narrow real public P-256 JWK and SHA-256/P1363 verification provider. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_verify_host(JS::Realm&);
}
