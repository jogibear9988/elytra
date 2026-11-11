/** Real directory streams; each native operation holds an RAII directory lease. */
#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
void install_directory_host(JS::Realm&, JS::Object&, EventLoop&);
}
