#pragma once
#include <LibJS/Runtime/Realm.h>
#include <AK/StringView.h>
namespace Mite {
class EventLoop;
int network_ip_family(StringView);
GC::Ref<JS::Object> create_network_host(JS::VM&, JS::Realm&, EventLoop&);
}
