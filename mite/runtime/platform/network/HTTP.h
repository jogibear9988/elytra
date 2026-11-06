/**
 * Connects llhttp parsing to the JavaScript HTTP compatibility layer. Parser callbacks emit
 * validated native framing observations; socket ownership remains in the TCP host.
 */

#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
GC::Ref<JS::Object> create_http_host(JS::Realm&);
}
