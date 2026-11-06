/**
 * Adapts Ladybird LibURL to the runtime URL API. Native parsing, component updates and
 * file-path conversion back shared ESM/CommonJS/global constructors.
 */

#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_url_host(JS::Realm&);
}
