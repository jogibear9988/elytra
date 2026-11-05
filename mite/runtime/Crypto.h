#pragma once
#include <LibJS/Runtime/Realm.h>
namespace Mite {
class EventLoop;
GC::Ref<JS::Object> create_crypto_host(JS::VM&,JS::Realm&,EventLoop&);
}
