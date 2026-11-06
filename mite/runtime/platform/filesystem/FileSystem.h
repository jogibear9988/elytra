/**
 * Implements filesystem hosts and descriptor operations through libuv. Asynchronous calls
 * snapshot native inputs and root only the owner-thread completion; descriptors close through
 * explicit lifecycle rules.
 */

#pragma once
#include <LibJS/Runtime/Realm.h>

namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_filesystem_host(JS::VM&, JS::Realm&, EventLoop&);
}
