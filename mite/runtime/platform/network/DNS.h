/**
 * Performs OS-backed asynchronous address lookup. Native address records are copied before
 * returning to the realm; JavaScript adapters handle Node overloads and result-order policy.
 */

#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_dns_host(JS::VM&, JS::Realm&, EventLoop&);
}
