/** libuv stat polling, independently owned by each native filesystem host. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
void install_stat_watcher_host(JS::Realm&, JS::Object&, EventLoop&);
}
