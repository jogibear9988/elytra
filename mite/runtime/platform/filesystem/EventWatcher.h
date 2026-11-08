/** Native filesystem event notifications; independently owned by each filesystem host. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
void install_event_watcher_host(JS::Realm&, JS::Object&, EventLoop&);
}
