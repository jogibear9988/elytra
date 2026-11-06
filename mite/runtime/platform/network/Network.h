/**
 * Owns TCP handles, reads/writes and connection lifecycle. Byte storage stays native while
 * work is pending; JS callbacks are dispatched through the runtime event loop.
 */

#pragma once
#include <AK/StringView.h>
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
int network_ip_family(StringView);

GC::Ref<JS::Object> create_network_host(JS::VM&, JS::Realm&, EventLoop&);
}
